/* SPDX-License-Identifier: GPL-3.0-only
 * Native HOME: copied musical state only. No audio arrays, clock or flash writes.
 * One complete bounded model is compared before any raster/hash/transfer work.
 */
#include "cf_native_fonts.h"
/* Shared knob labels encode accidentals as pixel-font glyph bytes. */
static unsigned cfn_code(unsigned c)
{ return c==0x80?'#':c==0x81?'b':c<32 || c>126?'?':c; }
/* Sparse large face: unsupported text falls back as a complete string. */
static const cfn_font_t *cfn_face(const cfn_font_t *f,const char *s)
{
    if(f==&CFN_64)for(;*s;s++)if(!f->g[cfn_code((uint8_t)*s)-32].adv)return &CFN_32;
    return f;
}
static int cfn_width(const cfn_font_t *f, const char *s)
{
    f=cfn_face(f,s);
    int w=0; for (;*s;s++) { unsigned c=cfn_code((uint8_t)*s); w+=f->g[c-32].adv; } return w;
}
static void cfn_coverage(int x,int y,unsigned coverage,uint8_t ink)
{
    if(!coverage || x<fb_cx0 || x>=fb_cx1 || y<fb_cy0 || y>=fb_cy1)return;
    if(coverage==3){fb[y*FB_W+x]=ink;return;}
    unsigned role=ink==FL_INK?0:ink==FL_INK2 || ink==FL_INK3 || ink==50 || ink==53?1:
        ink==48 || ink==49 || ink==52?2:ink==56?3:ink==FL_C(FL_R_ROSE,0)?4:CFN_AA_ROLES;
    uint8_t bg=fb[y*FB_W+x];if(bg==ink)return;
    if(CF_STUDIO && !CF_SCENES && bg==CGU_GLASS){
        unsigned unit=ink==CGU_PROGRESSION_INK?4u:(unsigned)ink-CGU_BAND_INK_BASE;
        if(unit<5u){fb[y*FB_W+x]=(uint8_t)(CGU_GLASS_AA+unit*2+coverage-1);return;}
    }
    unsigned surface=bg==FL_BG?0:bg==FL_BG1 || bg==FL_BG2?1:bg==48 || bg==49 || bg==52?2:bg==FL_BG3?3:CFN_AA_SURFACES;
    if(bg>=CFN_AA_BASE && bg<CFN_AA_BASE+CFN_AA_ROLES*CFN_AA_SURFACES*2) {
        unsigned entry=(bg-CFN_AA_BASE)/2;
        if(entry%CFN_AA_ROLES==role){surface=entry/CFN_AA_ROLES;unsigned old=(bg-CFN_AA_BASE)%2+1;if(old>coverage)coverage=old;}
    }
    if(role<CFN_AA_ROLES && surface<CFN_AA_SURFACES)fb[y*FB_W+x]=(uint8_t)(CFN_AA_BASE+(surface*CFN_AA_ROLES+role)*2+coverage-1);
    else if(CF_STUDIO && !CF_SCENES && role<2 && bg>=RAKT_CHORDS && bg<=RAKT_FX_SPACE)
        fb[y*FB_W+x]=(uint8_t)(CGU_FACE_AA+(bg-RAKT_CHORDS)*4+role*2+coverage-1);
    else if(CF_STUDIO && !CF_SCENES && role<2 && bg>=CGU_METAL && bg<CGU_MATERIAL_AA)
        fb[y*FB_W+x]=(uint8_t)(CGU_MATERIAL_AA+(bg>=CGU_GRILLE?4:0)+role*2+coverage-1);
    else if(coverage>=2)fb[y*FB_W+x]=ink; /* retain legacy graph colours */
}
static void cfn_large_glyph(const cfn_glyph_t *g,int x,int y,uint8_t ink)
{
    const uint8_t *data=CFN_64_BITS+g->off;
    unsigned run=0, repeated=0, byte=0, bit=0;
    for(unsigned j=0;j<g->h;j++){
        for(unsigned i=0;i<g->w;i++,bit++){
            if(!(bit&3u)){
                if(!run){
                    unsigned code=*data++;
                    repeated=code&128u;
                    run=(code&127u)+1u;
                    if(repeated)byte=*data++;
                }
                if(!repeated)byte=*data++;
                run--;
            }
            cfn_coverage(x+(int)i,y+(int)j,(byte>>(6u-2u*(bit&3u)))&3u,ink);
        }
        if((j&7u)==7u)CF_PUMP();
    }
}

static void cfn_text(const cfn_font_t *f,int x,int baseline,const char *s,uint8_t ink)
{
    f=cfn_face(f,s);
    for(;*s;s++) {
        unsigned c=cfn_code((uint8_t)*s);
        const cfn_glyph_t *g=&f->g[c-32];
        if(f==&CFN_64){
            cfn_large_glyph(g,x+g->x,baseline+g->y,ink);
            x+=g->adv;
            continue;
        }
        for(unsigned j=0;j<g->h;j++) {
            for(unsigned i=0;i<g->w;i++) {
                unsigned bit=j*g->w+i;
                unsigned coverage=(f->bits[g->off+bit/4]>>(6-2*(bit&3)))&3u;
                cfn_coverage(x+g->x+(int)i,baseline+g->y+(int)j,coverage,ink);
            }
            if(g->h>24 && (j&7u)==7u)CF_PUMP(); /* releases/stop input between bounded glyph strips */
        }
        x+=g->adv;
    }
}
static void cfn_center(const cfn_font_t *f,int x,int baseline,const char *s,uint8_t ink)
{ cfn_text(f,x-cfn_width(f,s)/2,baseline,s,ink); }
/* True round outlines. Callers budget the complete diameter, not a cropped arc. */
static void cfn_circle(int cx,int cy,int radius,uint8_t ink)
{
    int x=radius,y=0,error=1-radius;
    while(x>=y) {
        fb_pset(cx+x,cy+y,ink);fb_pset(cx+y,cy+x,ink);fb_pset(cx-y,cy+x,ink);fb_pset(cx-x,cy+y,ink);
        fb_pset(cx-x,cy-y,ink);fb_pset(cx-y,cy-x,ink);fb_pset(cx+y,cy-x,ink);fb_pset(cx+x,cy-y,ink);
        y++;if(error<0)error+=2*y+1;else{x--;error+=2*(y-x)+1;}
    }
}
/* Window text deliberately; essential chord identity uses composition below. */
static void cfn_fit(const cfn_font_t *f,int x,int baseline,const char *s,int width,uint8_t ink)
{
    char b[64];str_cpy(b,s,sizeof b);unsigned n=str_len(b);
    if(cfn_width(f,b)>width) {
        while(n && cfn_width(f,b)+cfn_width(f,"...")>width)b[--n]=0;
        str_cpy(b+n,"...",sizeof b-n);
    }
    cfn_text(f,x,baseline,b,ink);
}
static void cfn_squeezed(const cfn_font_t *f,int x,int baseline,const char *s,int width,uint8_t ink);
#include "cf_style.c"
/* Translate a freshly composed panel in place, never a second framebuffer.
 * Used only for the finite workspace arrival; controls and header stay fixed. */
static void cfn_shift_panel(int dx,int y0,int y1)
{
    if(!dx)return;
    for(int y=y0;y<y1;y++) {
        volatile uint8_t *row=fb+y*FB_W;
        if(dx>0){for(int p=FB_W-1;p>=dx;p--)row[p]=row[p-dx];for(int p=0;p<dx;p++)row[p]=FL_BG;}
        else {for(int p=0;p<FB_W+dx;p++)row[p]=row[p-dx];for(int p=FB_W+dx;p<FB_W;p++)row[p]=FL_BG;}
        if((y&15)==15)CF_PUMP();
    }
}
#if !CF_STUDIO || CF_SCENES
#include "cf_mascot.c"
#endif
#include "cf_keyboard_learn.c"
typedef struct {
    char chord[24],degree[12],notes[48],idea[40],sound[16],context[48],status[24],pending[48],next_setup[48],applied[64];
    char rail[4][24],numeral[4][12];
    char theory[48];
    char value[4][14],label[4][12];
    uint8_t live,hold,slot,first,count,total,skin,motion,battery,halo,rec,bank,disabled[4],emphasis[4],cursor_width;
    uint8_t pulse,halo_ink,activity,activity_step;
    uint8_t moment,build,bloom,engine_page,engine_bar,beat,focus,level[4],pin_mask;
    uint8_t workspace,zoom,genre,solo,muted,held_parts,shaping,sound_page,knob[4],part_level[4];
    uint8_t signal_peak,drum_groups,phrase_steps,layer_reply,bass_reply,section;
    uint8_t rack_knob[CGU_BAND_ARP][4], amp_knob[CGS_NPART][4];
    uint8_t fx_send_shape[4][2];
    uint8_t rack_level, rack_arp_step, rack_arp_mode, rack_lens;
    uint8_t generator_phase, generator_density, generator_swing, glitter_phase;
    uint16_t steps[4][2],bass_delays[2],drum_hits,matches;
    int16_t bank_dx;
    int8_t chord_dy,control_dx;
    uint16_t rack_scroll;
#if !CF_STUDIO || CF_SCENES
    cfm_pose_t mascot;
#endif
    cf_learn_model_t learn;
} cf_stage_model_t;
static cf_stage_model_t cf_stage_last;
static uint8_t cf_stage_valid;
static char cf_stage_notes[48];
static unsigned cf_stage_draws;
static uint16_t cf_stage_energy;
static uint32_t cf_stage_tick,cf_stage_chord_ms;
static char cf_stage_symbol[24];
static const char *cf_stage_change_name(unsigned change)
{
    static const char *const names[]={"","Fresh Take","Back One","Vibe","Direction","Groove","Feel","FX","Progression"};
    return names[change];
}

static void cf_stage_append(char *line,unsigned capacity,const char *text)
{
    unsigned len=str_len(line);
    str_cpy(line+len,text,capacity-len);
}

static int cf_stage_progression_preview(cf_stage_model_t *m)
{
    if(cge_preview>=cf_progression_count() || m->bank==CF_HB_FLOW)return 0;
    str_cpy(m->pending,"Preview: ",sizeof m->pending);
    cf_stage_append(m->pending,sizeof m->pending,cf_progression_get(cge_preview)->name);
    str_cpy(m->next_setup,cge_active()?"SEL queues for bar 1":"SEL chooses progression",sizeof m->next_setup);
    return 1;
}

static void cf_stage_engine_model(cf_stage_model_t *m)
{
    int flow=m->bank==CF_HB_FLOW,active=cf_sn.ac_on && cf_sn.ac_idea==CF_P_ENGINE;
    int loaded=cge_selected();
    unsigned vibe=loaded?cge.current.vibe:cge.vibe_request;
    unsigned direction=loaded?cge.direction:cge.direction_request;
    m->engine_page=1;
    m->focus=flow?cge_flow_focus:cge_engine_focus;
    str_cpy(m->idea,loaded?cge_view.name:CGE_VIBES[vibe].name,sizeof m->idea);
    m->pending[0]=m->next_setup[0]=0;
    static const char *const labels[2][4]={
        {"1 Energy","2 Feel","3 Space","4 Take"},
        {"1 Vibe","2 Mode","3 Surprise","4 Pins"}
    };
    for(unsigned i=0;i<4;i++)str_cpy(m->label[i],labels[flow][i],sizeof m->label[i]);
    if(flow) {
        m->level[2]=cge.surprise;
    } else {
        m->level[0]=cge.energy;m->level[1]=cge.feel;m->level[2]=cge.space;
    }

    char key[4];
    unsigned tonic=active?cf_sn.ac_tonic:cs.chord.tonic;
    unsigned scale=active?cf_sn.ac_scale:cs.chord.scale;
    cf_pc_name(tonic,&cu_spell,key);
    const char *mode=scale==CF_SC_MAJOR?"Major":scale==CF_SC_MINOR?"Minor":cf_progression_mode(scale);
    m->context[0]=0;
    if(loaded) {
        cf_stage_append(m->context,sizeof m->context,CGE_VIBES[vibe].name);
        cf_stage_append(m->context,sizeof m->context," / ");
        cf_stage_append(m->context,sizeof m->context,key);
        cf_stage_append(m->context,sizeof m->context," ");
        cf_stage_append(m->context,sizeof m->context,mode);
        cf_stage_append(m->context,sizeof m->context," / ");
    }
    cf_stage_append(m->context,sizeof m->context,CGE_DIRECTION_NAME[direction]);
    if(cs.clock_in==1)str_cpy(m->sound,"MIDI clock",sizeof m->sound);
    else kint(m->sound,cs.bpm," BPM");

    if(active && !cf_sn.ac_waiting) {
        m->engine_bar=cge.bar+1;
        if(cf_sn.beats_done)m->beat=1+(cf_sn.beats_done-1)%4;
        str_cpy(m->notes,"Feel ",sizeof m->notes);
        kint(m->notes+5,cge.feel_applied,"% / ");
        cf_stage_append(m->notes,sizeof m->notes,cge_articulation_name());
        cf_stage_append(m->notes,sizeof m->notes," / ARP ");
        cf_stage_append(m->notes,sizeof m->notes,CGE_ARP_NAMES[cge.arp_applied]);
        unsigned rhythm=cf_sn.ac_rhythm,fx=cfx_identify(&cs.fx);
        const char *effect=cfx_bypass || !cs.fx.on?"Off":fx<FXP_FACTORY_N?FXP_FACTORY[fx].name:
            fx<FXP_FACTORY_N+FXP_SLOTS?fxp_bank.slot[fx-FXP_FACTORY_N].name:"Custom";
        str_cpy(m->applied,"Groove ",sizeof m->applied);
        cf_stage_append(m->applied,sizeof m->applied,cf_sn.perf_dropped?"Dropped":rhythm<DM_N?DM_RHY[rhythm].name:"Off");
        cf_stage_append(m->applied,sizeof m->applied," / FX ");
        cf_stage_append(m->applied,sizeof m->applied,effect);
        if(!m->moment && !cf_sn.perf_pending && !cf_sn.ac_paused && !cf_sn.ac_lost)
            str_cpy(m->status,"Playing",sizeof m->status);
    } else {
        str_cpy(m->notes,flow?"K1 picks Haze or Drive":"Turn SELECT to browse progressions",sizeof m->notes);
        if(!cf_sn.ac_on && cs.clock_in!=1)str_cpy(m->status,"Ready",sizeof m->status);
    }

    if(cf_stage_progression_preview(m))return;
    unsigned pending=cge.pending;
    if(active && pending) {
        unsigned change=cge_change(pending);
        const char *name=cf_stage_change_name(change);
        if(change==CGE_CHANGE_VIBE)name=CGE_VIBES[cge.next.vibe].name;
        if(change==CGE_CHANGE_DIRECTION)name=CGE_DIRECTION_NAME[cge.direction_request];
        if(change==CGE_CHANGE_GROOVE)name=DM_RHY[cge_prepared_plan.rhythm].name;
        if(change==CGE_CHANGE_FEEL)name="New Feel";
        if(change==CGE_CHANGE_FX)name=FXP_FACTORY[cge_fx_id[cge.next.vibe][cge.next.fx]].name;
        if(change==CGE_CHANGE_PICK)name=cge_progression_name(cge.next.id);
        str_cpy(m->pending,"At bar 1: ",sizeof m->pending);
        cf_stage_append(m->pending,sizeof m->pending,name);
    }
    if(cge_arp_pending()) {
        unsigned next=(cge.bar/2u+1u)*2u%16u+1u;
        char *line=m->pending[0]?m->next_setup:m->pending;
        str_cpy(line,"At bar ",sizeof m->pending);
        kint(line+7,next,": ");
        if(cge.arp_request!=cge.arp_applied) {
            cf_stage_append(line,sizeof m->pending,"ARP ");
            cf_stage_append(line,sizeof m->pending,CGE_ARP_NAMES[cge.arp_request]);
        } else if(cge.feel!=cge.feel_applied) {
            cf_stage_append(line,sizeof m->pending,"Feel ");
            kint(line+str_len(line),cge.feel,"%");
        } else cf_stage_append(line,sizeof m->pending,"ARP settings");
    }
    if(cge_notice) {
        char *line=m->pending[0]?m->next_setup:m->pending;
        str_cpy(line,cge_notice,sizeof m->pending);
    }
    if(m->next_setup[0])return;
    if(active && !m->pending[0] && direction==CGE_JAM && (!cge.surprise || !cge_jam_choices())) {
        str_cpy(m->pending,!cge.surprise?"Jam paused / Surprise 0%":"Jam blocked by pins",sizeof m->pending);
        str_cpy(m->next_setup,!cge.surprise?"Flow K3: turn up Surprise":"Flow > Pins: free Groove or FX",sizeof m->next_setup);
        return;
    }
    if(flow && m->focus==3) {
        str_cpy(m->next_setup,"SEL or K4 opens Pins",sizeof m->next_setup);
    } else if(!active) {
        str_cpy(m->next_setup,cf_sn.ac_on?"Press PLAY to stop":"Press PLAY to start",sizeof m->next_setup);
    } else if(cge.last_change) {
        str_cpy(m->next_setup,cf_stage_change_name(cge.last_change),sizeof m->next_setup);
        cf_stage_append(m->next_setup,sizeof m->next_setup," applied");
    } else if(flow) {
        const char *hint=direction==CGE_REPEAT?"Repeat holds this loop":!cge.surprise?"Jam changes off":
            cge.surprise<50?"Jam changes every 2 loops":"Jam changes each loop";
        str_cpy(m->next_setup,hint,sizeof m->next_setup);
    } else {
        str_cpy(m->next_setup,"SELECT browse / SEL Chord Lab",sizeof m->next_setup);
    }
}
#include "cf_workspace_view.c"
#include "cf_glow_view.c"
static void cf_stage_model(cf_stage_model_t *m)
{
    memset(m,0,sizeof *m);
    m->bank=cf_home_bank;
    if(CF_STUDIO && !CF_SCENES){cgu_model(m);return;}
    if(m->bank==CF_HB_FLOW && cge_pins_open) {
        m->engine_page=2;m->focus=cge.pin;m->pin_mask=cge.locks;m->skin=cf_skin;
        str_cpy(m->status,cf_acc_transport(),sizeof m->status);
        return;
    }
    name_update();
    if(m->bank==CF_HB_LEARN)cf_learn_model(&m->learn);
    m->bank_dx=(int8_t)(cf_tween_offset(cf_home_bank_ms,160,10)*(cf_nav_dir<0?-1:1));
    m->live=cu_name_live;m->hold=cf_sn.out.hold;m->skin=cf_skin;m->motion=cf_motion;m->battery=(uint8_t)cf_batt();
    str_cpy(m->chord,cu_name[0]?cu_name:"--",sizeof m->chord);
    str_cpy(m->degree,cu_name_sub,sizeof m->degree);
    if(str_len(m->chord)!=str_len(cf_stage_symbol) || memcmp(m->chord,cf_stage_symbol,str_len(m->chord))) {
        str_cpy(cf_stage_symbol,m->chord,sizeof cf_stage_symbol);
        cf_stage_chord_ms=cf_motion && m->live?(cu.now?cu.now:1u):0;
    }
    m->chord_dy=(int8_t)cf_tween_offset(cf_stage_chord_ms,120,5);
    unsigned n=0;const uint8_t *note=0;
    if(cf_sn.out.sounding && cf_sn.out.n){n=cf_sn.out.n;note=cf_sn.out.note;}
    else if((cf_sn.ac_on || cf_sn.sq_on) && cf_sn.sq_dn){n=cf_sn.sq_dn;note=cf_sn.sq_dnote;}
    if(n) {
        cf_stage_notes[0]=0;
        for(unsigned i=0;i<n && i<6;i++) {
            char b[4];cf_pc_name(note[i]%12,&cu_spell,b);
            if(i)str_cpy(cf_stage_notes+str_len(cf_stage_notes),"  ",sizeof cf_stage_notes-str_len(cf_stage_notes));
            str_cpy(cf_stage_notes+str_len(cf_stage_notes),b,sizeof cf_stage_notes-str_len(cf_stage_notes));
        }
    }
    if(m->live && !m->degree[0] && n) {
        int root=cf_sn.out.sounding?cf_sn.out.root_pc:cf_sn.sq_droot%12;
        cf_harmony_numeral(cs.chord.scale,cs.chord.tonic,root,m->degree);
    }
    str_cpy(m->notes,cf_stage_notes,sizeof m->notes);
    const cf_progression_t *p=cf_progression_current();
    if(cf_sn.out.sounding && cf_sn.out.n && !cf_sn.out.single)
        m->matches=cf_progression_highlight(p,cs.chord.scale,cs.chord.tonic,cf_sn.out.root_pc,cf_sn.out.mask);
    if(cpg.idea==CF_P_DRAFT){str_cpy(m->idea,"Draft: ",sizeof m->idea);str_cpy(m->idea+7,p->name,sizeof m->idea-7);}else str_cpy(m->idea,p->name,sizeof m->idea);str_cpy(m->sound,cf_snd[0].name,sizeof m->sound);
    char b[16],key[4];cf_pc_name(cs.chord.tonic,&cu_spell,key);
    str_cpy(m->context,key,sizeof m->context);str_cpy(m->context+str_len(m->context)," ",sizeof m->context-str_len(m->context));
    str_cpy(m->context+str_len(m->context),cf_progression_mode(cs.chord.scale),sizeof m->context-str_len(m->context));
    kint(b,cs.beats,cs.beats==6?"/8":"/4");
    str_cpy(m->context+str_len(m->context),"   ",sizeof m->context-str_len(m->context));
    str_cpy(m->context+str_len(m->context),b,sizeof m->context-str_len(m->context));
    str_cpy(m->status,cf_acc_transport(),sizeof m->status);
    /* The existing recording plaque owns count-in/capture/save truth. */
    unsigned plaque=dm_plaque(m->pending,sizeof m->pending);
    if(plaque==DMP_REC || plaque==DMP_RECIN) {
        m->rec=1;
        str_cpy(m->status,plaque==DMP_RECIN?"COUNT IN":"REC",sizeof m->status);
    } else m->pending[0]=0;
    if(!m->rec)cf_acc_pending(m->pending,sizeof m->pending,m->next_setup,sizeof m->next_setup);
    m->total=p->n;m->slot=cf_sn.ac_on && !(cf_sn.ac_waiting&&!cf_sn.ac_paused)?cf_sn.ac_slot:255;
    m->first=m->slot<p->n?(m->slot/4)*4:0;m->count=p->n-m->first;if(m->count>4)m->count=4;
    for(unsigned i=0;i<m->count;i++) {
        unsigned at=m->first+i;int root=cf_progression_root(p,at,cs.chord.scale,cs.chord.tonic);
        uint32_t mask=cf_progression_slot_mask(p,at,cs.chord.scale,cs.chord.tonic,&ch);
        const cf_source_chord_t *source=cf_progression_source(p);
        int bass=source && source[at].bass>=0?(root+source[at].bass)%12:-1;
        cf_chord_name(root,mask,bass,&cu_spell,m->rail[i]);
        cf_harmony_numeral(cs.chord.scale,cs.chord.tonic,root,m->numeral[i]);
    }
    for(int i=0;i<4;i++){cf_knob_t k;memset(&k,0,sizeof k);ui_knob_info(i,&k);str_cpy(m->label[i],k.name,sizeof m->label[i]);str_cpy(m->value[i],k.val,sizeof m->value[i]);m->disabled[i]=k.dis;}
    cf_stage_energy=cfn_motion.energy;cf_stage_tick=cu.now;
    unsigned clock=(cf_sn.running || cf_sn.metro_on) && cf_sn.beats_done && cfn_motion.fresh &&
        !cf_sn.ac_lost && !cf_sn.ac_waiting && !cf_sn.ac_paused;
    unsigned phase=cf_sn.beat_acc/(FS*60u/16u);if(phase>15)phase=15;
    m->halo=cf_motion && cfn_motion.active?(uint8_t)(1+cfn_motion.energy/64+(cfn_motion.parts>1?1:0)):0;
    if(m->halo) {
        unsigned progress=clock?phase:cf_stage_chord_ms && (int32_t)(cf_stage_chord_ms-cf_motion_epoch)>0 && cu.now-cf_stage_chord_ms<320?(cu.now-cf_stage_chord_ms)*16/320:16;
        if(progress<15) {
            m->pulse=42+progress*10/14; /* 84..104 px, complete inside y=51..155 */
            unsigned peak=m->halo>4?4:m->halo;
            m->halo_ink=CFN_HALO_0+(peak*(15-progress)+14)/15-1;
        }
    }
    if(cf_motion && clock){m->activity=1;m->activity_step=phase/4;}
    else if(cf_motion && cfn_motion.active){m->activity=2;m->activity_step=cfn_motion.energy/64;}
    for(unsigned k=0;k<4;k++)m->emphasis[k]=cfn_knob(k);
    m->cursor_width=cfn_cursor(CFN_CURSOR_HOME|m->slot);
    if(!m->rec && cf_perform_ready()) {
        m->moment=cf_sn.perf_dropped?CFPERF_DROP:cf_sn.perf_fill?CFPERF_FILL:0;
        if(cf_sn.perf_pending)str_cpy(m->status,cf_sn.perf_pending==CFPERF_FILL?"FILL NEXT":
            cf_sn.perf_pending==CFPERF_DROP?"DROP NEXT":"BACK NEXT",sizeof m->status);
        else if(m->moment)str_cpy(m->status,m->moment==CFPERF_FILL?"FILL":"DRUMS OFF",sizeof m->status);
        if(cf_motion && m->halo) {
            if(m->moment==CFPERF_FILL) {
                unsigned bt=cf_sn.sq_bt?cf_sn.sq_bt:1;
                unsigned t=cf_sn.dm_on?cf_sn.dm_t:cf_sn.sq_t;
                m->build=1+(uint8_t)(t*15u/bt);
            }
            uint32_t age=cu.now-cfn_moment_ms;
            if(cfn_moment_ms && age<650u)m->bloom=1+(uint8_t)(age*15u/650u);
        }
    }
#if !CF_STUDIO || CF_SCENES
    cfm_model(&m->mascot,cf_stage_chord_ms);
    m->pulse=m->mascot.ring;m->halo_ink=m->mascot.ring_ink;
#endif
    if(m->bank==CF_HB_LEARN) {
        m->bank_dx=m->chord_dy=0;m->halo=m->pulse=m->halo_ink=m->build=m->bloom=0;
#if !CF_STUDIO || CF_SCENES
        memset(&m->mascot,0,sizeof m->mascot);
#endif
        memset(m->emphasis,0,sizeof m->emphasis);m->cursor_width=0;
    }
    int engine=cf_sn.ac_on && cf_sn.ac_idea==CF_P_ENGINE;
    if((m->bank==CF_HB_ENGINE || m->bank==CF_HB_FLOW) && !m->rec && !cf_sn.sq_on && !sqk.on && (!cf_sn.ac_on || engine))
        cf_stage_engine_model(m);
    else if(!m->rec && !cf_sn.sq_on && !sqk.on) {
        if(engine && !cf_sn.ac_waiting)m->engine_bar=cge.bar+1;
        if(!cf_stage_progression_preview(m) && engine && cge.request_kind==CGE_PICK && (cge.pending&CGE_HARMONY)) {
            str_cpy(m->pending,"At bar 1: ",sizeof m->pending);
            cf_stage_append(m->pending,sizeof m->pending,cge_progression_name(cge.next.id));
        }
    }
}
/* Preserve the complete spelling for unusually long altered/slash names.
 * Only horizontal glyph coordinates are compressed in the bounded nameplate. */
static void cfn_squeezed(const cfn_font_t *f,int x,int baseline,const char *s,int width,uint8_t ink)
{
    f=cfn_face(f,s);
    int natural=cfn_width(f,s),advance=0;
    if(natural<=width){cfn_text(f,x,baseline,s,ink);return;}
    for(;*s;s++) {
        unsigned c=cfn_code((uint8_t)*s);const cfn_glyph_t *g=&f->g[c-32];
        for(unsigned j=0;j<g->h;j++) {
            for(unsigned i=0;i<g->w;i++){unsigned bit=j*g->w+i;unsigned coverage=(f->bits[g->off+bit/4]>>(6-2*(bit&3)))&3u;cfn_coverage(x+(advance+g->x+(int)i)*width/natural,baseline+g->y+(int)j,coverage,ink);}
            if((j&7u)==7u)CF_PUMP();
        }
        advance+=g->adv;
    }
}
static void cf_stage_chord(const char *name,int y,uint8_t ink)
{
    if(cu_name_single && str_len(name)>2){int w=cfn_width(&CFN_32,name);if(w>114)w=114;cfn_squeezed(&CFN_32,175-w/2,y,name,114,ink);return;}
    char root[3]={name[0],0,0};unsigned n=1;if(name[1]=='b'||name[1]=='#'){root[1]=name[1];n=2;}
    int w=cfn_width(&CFN_64,root)+cfn_width(&CFN_32,name+n),x=175-w/2;
    if(w>114){int small=cfn_width(&CFN_32,name);if(small>114)small=114;cfn_squeezed(&CFN_32,175-small/2,y,name,114,ink);return;}
    cfn_text(&CFN_64,x,y,root,ink);cfn_text(&CFN_32,x+cfn_width(&CFN_64,root),y-15,name+n,ink);
}
static void cf_stage_engine_tabs(unsigned current,const char *status)
{
    static const char *const tabs[]={"Jam","Flow","Pins"};
    for(unsigned i=0;i<3;i++) {
        int x=8+46*(int)i;
        if(i==current)fb_rect(x,4,42,19,FL_BG2);
        cfn_center(&CFN_12,x+21,17,tabs[i],i==current?C_ACC(0):FL_INK2);
    }
    cfn_text(&CFN_12,230-cfn_width(&CFN_12,status),17,status,FL_INK);
    fb_hline(10,25,220,FL_BG3);
}
static void cf_stage_pins_draw(const cf_stage_model_t *m)
{
    cf_stage_engine_tabs(2,m->status);
    cfn_text(&CFN_16,10,45,"Keep what you like",FL_INK);
    cfn_text(&CFN_12,10,60,"Pinned settings stay as you set them.",FL_INK2);
    for(unsigned i=0;i<5;i++) {
        int y=68+23*(int)i;
        int pinned=m->pin_mask&(1u<<i),selected=i==m->focus;
        if(selected){fb_rect(8,y,224,22,FL_BG2);fb_rect(8,y,3,22,C_ACC(0));}
        cfn_text(&CFN_14,17,y+16,CGE_PIN_NAME[i],FL_INK);
        cfn_text(&CFN_14,165,y+16,pinned?"Pinned":"Free",pinned?C_ACC(0):FL_INK2);
    }
    int pinned=m->pin_mask&(1u<<m->focus);
    cfn_text(&CFN_12,10,199,pinned?"SEL frees selected setting":"SEL pins selected setting",C_ACC(0));
    fb_hline(10,208,220,FL_BG3);
    cfn_text(&CFN_12,10,227,"K4 selects",FL_INK2);
    cfn_text(&CFN_12,139,227,"HOME: Flow",FL_INK);
}
static void cf_stage_engine_draw(const cf_stage_model_t *m)
{
    int flow=m->bank==CF_HB_FLOW;
    cf_stage_engine_tabs(flow,m->status);
    cfn_fit(&CFN_16,10,41,m->idea,220,FL_INK);
    cfn_fit(&CFN_12,10,56,m->context,157,FL_INK2);
    cfn_text(&CFN_12,230-cfn_width(&CFN_12,m->sound),56,m->sound,FL_INK2);

#if !CF_STUDIO || CF_SCENES
    cfm_pose_t pose=m->mascot;
    pose.sx=pose.sx*5/8;pose.sy=pose.sy*5/8;pose.y-=21*256;
    pose.burst=0;
    fb_clip(0,60,115,109);
    cfm_draw(&pose);
    fb_unclip();
#endif
    if(m->engine_bar || m->live) {
        fb_clip(116,57,238,108);
        cf_stage_chord(m->chord,103,m->live?FL_INK:FL_INK2);
        fb_unclip();
    }
    if(m->engine_bar) {
        char bar[16]="Bar ";
        kint(bar+4,m->engine_bar," / 16");
        cfn_text(&CFN_16,10,120,bar,FL_INK);
        for(unsigned i=0;i<4;i++)
            fb_rect(175+i*15,110,10,9,i+1==m->beat?C_ACC(0):FL_BG3);
    }
    for(unsigned i=0;i<16;i++) {
        int x=10+(int)i*13+(int)(i/4)*4;
        uint8_t ink=i+1==m->engine_bar?C_ACC(0):i+1<m->engine_bar?FL_INK2:FL_BG3;
        fb_rect(x,125,11,6,ink);
    }
    cfn_fit(&CFN_12,10,145,m->notes,220,FL_INK);
    cfn_fit(&CFN_12,10,158,m->applied,220,FL_INK);
    if(m->pending[0])fb_rect(6,162,228,32,FL_BG2);
    cfn_fit(&CFN_14,10,176,m->pending,220,C_ACC(0));
    cfn_fit(&CFN_12,10,191,m->next_setup,220,FL_INK);

    fb_hline(10,198,220,FL_BG3);
    for(unsigned i=0;i<4;i++) {
        int x=(int)i*60;
        if(i)fb_vline(x,203,34,FL_BG3);
        if(m->focus==i)fb_rect(x+2,202,56,37,FL_BG2);
        cfn_center(&CFN_11,x+30,212,m->label[i],m->focus==i?C_ACC(0):FL_INK2);
        uint8_t ink=m->disabled[i]?FL_INK2:FL_INK;
        if(!flow && i==3) {
            cfn_center(&CFN_11,x+30,224,"CW Fresh",ink);
            cfn_center(&CFN_11,x+30,236,"CCW Back",ink);
        } else {
            const cfn_font_t *face=cfn_width(&CFN_14,m->value[i])<=54?&CFN_14:&CFN_11;
            cfn_center(face,x+30,230,m->value[i],ink);
        }
        if((flow && i==2) || (!flow && i<3)) {
            fb_rect(x+7,235,46,3,FL_BG3);
            fb_rect(x+7,235,46*m->level[i]/100,3,m->disabled[i]?FL_INK2:C_ACC(0));
        }
    }
}

static void cf_stage_draw(void)
{
    cf_stage_model_t m;cf_stage_model(&m);
    if(CF_STUDIO && !CF_SCENES && cf_stage_valid && !fb_full &&
        m.signal_peak!=cf_stage_last.signal_peak){
        uint8_t peak=m.signal_peak;
        m.signal_peak=cf_stage_last.signal_peak;
        if(!memcmp(&m,&cf_stage_last,sizeof m)){
            cf_stage_last.signal_peak=peak;
            cf_stage_draws++;
            fb_unclip();cgu_output_meter(156,24,peak);
            if(!cu.pop)cf_present();
            return;
        }
        m.signal_peak=peak;
    }
    if(cf_stage_valid && !fb_full && !memcmp(&m,&cf_stage_last,sizeof m))return;
    cf_stage_last=m;cf_stage_valid=1;cf_stage_draws++;
    fb_unclip();fb_fill(FL_BG);
    if(CF_STUDIO && !CF_SCENES){
        cgu_workspace_draw(&m);
        if(!cu.pop)cf_present();
        return;
    }
    if(m.bank==CF_HB_LEARN) {
        char heading[24]="KEYBOARD ";kint(heading+9,m.bank+1,"/");
        kint(heading+str_len(heading),CF_HB_N,0);
        cfn_text(&CFN_12,10,17,heading,FL_INK);
        cfn_fit(&CFN_12,140,17,m.status,90,C_ACC(0));
        fb_hline(10,25,220,FL_BG3);
        cfn_fit(&CFN_12,10,43,m.context,136,FL_INK2);
        cfn_text(&CFN_12,156,43,m.learn.automatic?"Harmony On":"Harmony Off",FL_INK2);
        cfn_text(&CFN_12,10,64,m.hold?"HOLD":m.live?"NOW":cu_name[0]?"LAST":"READY",FL_INK2);
        cfn_fit(&CFN_16,53,64,m.chord,177,m.live?FL_INK:FL_INK2);
        cfn_fit(&CFN_12,10,80,m.notes,220,FL_INK2);
        cfn_text(&CFN_12,10,95,"Chords",FL_INK2);
        cfn_text(&CFN_12,230-cfn_width(&CFN_12,"Roots / degrees"),95,"Roots / degrees",FL_INK2);
        cf_learn_keyboard(&m.learn,0,FB_W);
        cfn_fit(&CFN_16,10,192,m.learn.title,220,FL_INK);
        cfn_fit(&CFN_12,10,208,m.learn.help,220,FL_INK2);
        cfn_fit(&CFN_12,10,222,m.learn.detail,220,FL_INK2);
        fb_hline(10,227,220,FL_BG3);
        cfn_fit(&CFN_12,10,239,"K1 Harmony / K2 Reset / HOME Core",220,FL_INK2);
        if(m.pending[0]) {
            fb_rect(0,67,240,17,FL_BG);cfn_fit(&CFN_12,10,80,m.pending,220,C_ACC(0));
        }
        if(!cu.pop)cf_present();return;
    }
    if(m.engine_page) {
        if(m.engine_page==2)cf_stage_pins_draw(&m);
        else cf_stage_engine_draw(&m);
        if(!cu.pop)cf_present();
        return;
    }
    int queued=m.next_setup[0]!=0,cue=m.pending[0]!=0;
#if !CF_STUDIO || CF_SCENES
    cfm_pose_t pose=m.mascot;
    if(cue){pose.sx=pose.sx*3/4;pose.sy=pose.sy*3/4;pose.y+=9*256;}
    int ring_y=cue?114:105;
    cfn_circle(57,ring_y,cue?33:44,CFN_HALO_0);
    if(m.pulse){unsigned r=cue?m.pulse*3u/4u:m.pulse;cfn_circle(57,ring_y,(int)r,m.halo_ink);}
    cfm_draw(&pose);
#endif
    char window[20],status[48],bank[24];
    if(m.engine_bar)kint(window,m.engine_bar,"/16");
    else if(m.slot<m.total){kint(window,m.slot+1,"/");kint(window+str_len(window),m.total,0);}
    else{kint(window,m.first+1,"-");kint(window+str_len(window),m.first+m.count,"/");kint(window+str_len(window),m.total,0);}
    str_cpy(status,m.status,sizeof status);str_cpy(status+str_len(status)," ",sizeof status-str_len(status));str_cpy(status+str_len(status),window,sizeof status-str_len(status));
    int status_x=210-cfn_width(&CFN_12,status);
    cfn_fit(&CFN_16,10,20,m.idea,status_x-22,FL_INK);
    cfn_text(&CFN_12,status_x,17,status,C_ACC(0));
    if(m.activity==1) {
        static const uint8_t xy[4][2]={{224,8},{228,12},{224,16},{220,12}};
        for(unsigned i=0;i<4;i++){int size=i==m.activity_step?3:1;fb_rect(xy[i][0]-size/2,xy[i][1]-size/2,size,size,i==m.activity_step?C_ACC(0):FL_INK2);}
    } else if(m.activity==2){int size=3+(m.activity_step>2?2:m.activity_step);fb_rect(224-size/2,12-size/2,size,size,C_ACC(0));}
    cfn_fit(&CFN_12,10,41,m.context,145,FL_INK2);
    str_cpy(bank,CF_HOME_BANK_NAME[m.bank],sizeof bank);
    str_cpy(bank+str_len(bank)," ",sizeof bank-str_len(bank));kint(bank+str_len(bank),m.bank+1,"/");kint(bank+str_len(bank),CF_HB_N,0);
    const cfn_font_t *bank_font=m.bank==CF_HB_VARIATION?&CFN_11:&CFN_12;
    fb_clip(156,28,234,46);cfn_text(bank_font,230-cfn_width(bank_font,bank)+m.bank_dx,41,bank,FL_INK2);fb_unclip();
    if(m.pending[0])cfn_fit(&CFN_12,10,queued?55:59,m.pending,220,m.rec?FL_C(FL_R_ROSE,0):C_ACC(0));
    else cfn_text(&CFN_12,118,59,m.hold?"HOLD":m.live?"NOW":cu_name[0]?"LAST":"READY",FL_INK2);
    if(queued)cfn_fit(&CFN_12,10,68,m.next_setup,220,FL_INK2);
    char degree[24];str_cpy(degree,queued?(m.hold?"Hold ":m.live?"Now ":"Last "):"",sizeof degree);
    str_cpy(degree+str_len(degree),m.degree,sizeof degree-str_len(degree));
    cfn_fit(&CFN_16,118,cue?91:77,degree,114,C_ACC(0));
    fb_clip(116,cue?92:80,238,145);cf_stage_chord(m.chord,cue?141:130,m.live?FL_INK:FL_INK2);fb_unclip();
    if(!cue)cfn_fit(&CFN_12,118,151,m.notes,114,FL_INK2);
    for(unsigned i=0;i<m.count;i++) {
        int x=30+(int)i*60;
        fb_clip((int)i*60,157,(int)(i+1)*60,195);
        unsigned match=(m.matches>>(m.first+i))&1u;
        if(match)fb_rect((int)i*60+3,159,54,33,C_ACC(0));
        cfn_center(&CFN_11,x,168,m.numeral[i],match?56:m.slot==m.first+i?C_ACC(0):FL_INK2);
        const cfn_font_t *rail_face=&CFN_12;
        int rail_width=cfn_width(rail_face,m.rail[i]);if(rail_width>54)rail_width=54;
        cfn_squeezed(rail_face,x-rail_width/2,184,m.rail[i],54,match?56:m.slot==m.first+i?C_ACC(0):FL_INK);
        if(m.slot==m.first+i)fb_rect((int)i*60+12,191,36*m.cursor_width/100,2,C_ACC(0));
    }
    if(m.bank==CF_HB_CORE && cs.chord.scale==CF_SC_MAJOR)str_cpy(m.value[2],"Major",sizeof m.value[2]);
    cfn_controls(m.label,m.value,m.disabled,m.emphasis);
    fb_unclip();if(!cu.pop)cf_present();
}
