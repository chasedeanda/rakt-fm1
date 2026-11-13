/* SPDX-License-Identifier: GPL-3.0-only
 * Studio's live mixer. The keyboard follows the panel's F3–G5 layout.
 */
typedef struct {
    uint32_t keys;
    uint16_t bpm, ready;
    uint8_t view, muted, cut, solo, drums;
    uint8_t level[CGS_NPART];
    uint8_t paused, playing, ext, waiting, pending, fill, dropped, repeat;
    uint8_t wave_phase, wave_genre, wave_density, wave_bend;
    char message[48];
} cgu_glow_model_t;
static uint8_t cgu_glow_valid;

static void cgu_glow_text_up(int baseline,int y,const char *name,uint8_t ink)
{
    const cfn_font_t *font=&CFN_11;
    for(;*name;name++){
        const cfn_glyph_t *g=&font->g[cfn_code((uint8_t)*name)-32];
        for(unsigned row=0;row<g->h;row++)
            for(unsigned col=0;col<g->w;col++){
                unsigned bit=row*g->w+col;
                unsigned coverage=(font->bits[g->off+bit/4]>>(6-2*(bit&3)))&3u;
                cfn_coverage(baseline+g->y+(int)row,y-g->x-(int)col,coverage,ink);
            }
        y-=g->adv;
    }
}

enum { CGU_GLOW_TOP=94, CGU_GLOW_BOTTOM=222 };

static void cgu_glow_map(const cgu_glow_model_t *m)
{
    static const char notes[]="FGABCDEFGABCDEFG";
    static const char *const actions[]={
        "Pause","Restart","Unmute","Tap","Fill hold","Fill bar","Drop","Riser","Repeat","Echo out","HP sweep"
    };
    for(int black=0;black<=1;black++){
        for(unsigned key=0;key<CF_NKEYS;key++){
            if(cf_key_is_upper(key)!=black)continue;
            unsigned p=cgu_glow_place(key,black);
            unsigned white=black?cgu_glow_place(key,0):p;
            int x=8+(int)white*224/16;
            int width=13;
            int held=(m->keys>>key)&1u;
            int enabled=1, on=held;
            uint8_t fill=CGU_METAL+3, accent=CGU_PROGRESSION_INK, ink=FL_INK;
            char name[16]="";
            if(p<CGS_NPART){
                unsigned part=cgu_part(p), bit=1u<<part;
                accent=CGU_INK_BASE+part;
                if(!black)str_cpy(name,"Solo ",sizeof name);
                unsigned length=black?0:5;
                str_cpy(name+length,CGS_PART_NAME[part],sizeof name-length);
                on=black?(m->muted&bit)!=0:(m->solo&bit)!=0;
            } else if(black && p>=6u && p<10u){
                str_cpy(name,CGU_GLO_DRUMS[p-6u],sizeof name);
                accent=CGU_INK_BASE+CGS_DRUMS;
                on=(m->drums>>(p-6u))&1u;
            } else if(!black && p<=CGU_GLO_SWEEP){
                const char *label=actions[p-CGU_GLO_PAUSE];
                if(p==CGU_GLO_PAUSE && m->paused)label="Resume", on=1;
                if(p==CGU_GLO_FILL_BAR && m->pending==CFPERF_FILL)on=1;
                if(p==CGU_GLO_DROP && (m->dropped || m->pending==CFPERF_DROP))label="Bring back", on=1;
                str_cpy(name,label,sizeof name);
                enabled=(m->ready>>p)&1u;
                if(p>=CGU_GLO_RISER){
                    static const uint8_t colors[]={CGS_CHORDS,CGS_BASS,CGS_DRUMS,CGS_LAYER};
                    accent=CGU_INK_BASE+colors[p-CGU_GLO_RISER];
                } else if(p>=CGU_GLO_FILL)accent=RAKT_GREEN;
            }
            if(black){
                x-=5;width=10;
                fb_rbox(x-1,CGU_GLOW_TOP-1,12,60,2,CGU_METAL+10);
                fill=name[0]?CGU_GLASS:CGU_METAL+1;
            }
            if(on){
                fill=black?CGU_INK_BASE+CGS_DRUMS:accent;
                ink=CGU_METAL+10;
            } else if(!enabled)ink=FL_INK2;
            int height=black?58:CGU_GLOW_BOTTOM-CGU_GLOW_TOP;
            if(black || on)fb_rbox(x,CGU_GLOW_TOP,width,height,1,fill);
            else fb_vline(x+width,CGU_GLOW_TOP,height,CGU_METAL+2);
            if(name[0]){
                if(!on)fb_hline(x+1,CGU_GLOW_TOP,width-2,enabled?accent:CGU_METAL+5);
                cgu_glow_text_up(x+width/2+4,black?CGU_GLOW_TOP+53:207,name,ink);
            }
            if(!black){
                char note[2]={notes[p],0};
                cfn_center(&CFN_11,x+width/2,218,note,on?ink:FL_INK2);
            }
        }
    }
}

static const char *cgu_glow_part_state(const cgu_glow_model_t *m,unsigned part)
{
    unsigned bit=1u<<part;
    return m->muted&bit?"Mute":m->solo&bit?"Solo":m->cut&bit?"Cut":"On";
}

static void cgu_glow_state(const cgu_glow_model_t *m)
{
    for(unsigned i=0;i<NELEM(CGU_GLO_DRUMS);i++){
        int x=9+i*56, muted=(m->drums>>i)&1u;
        uint8_t ink=muted?CGU_INK_BASE+CGS_DRUMS:FL_INK;
        fb_rbox(x,101,53,27,2,CGU_GLASS);
        if(muted)fb_hline(x+3,126,47,ink);
        cfn_center(&CFN_11,x+26,118,CGU_GLO_DRUMS[i],ink);
    }
    fb_hline(12,136,216,CGU_METAL+4);
    cfn_text(&CFN_12,13,150,"C4 Pause / resume    D4 Restart",FL_INK2);
    cfn_text(&CFN_12,13,163,"E4 Unmute all    F4 Tap tempo",FL_INK2);
    cfn_text(&CFN_12,13,176,"G4 Fill held    A4 Fill next bar",FL_INK2);
    cfn_text(&CFN_12,13,189,"B4 Drop / bring back drums",FL_INK2);
    cfn_text(&CFN_12,13,202,"C5 Riser    D5 Beat Repeat",FL_INK2);
    cfn_text(&CFN_12,13,215,"E5 Echo Out    F5 High-pass sweep",FL_INK2);
}

static void cgu_glow_header(const cgu_glow_model_t *m)
{
    static const char *const tabs[]={"Map","State"};
    const char *status=m->waiting?"Clock wait":m->paused?"Paused":m->pending==CFPERF_FILL?"Fill next":
        m->pending==CFPERF_DROP?"Drop next":m->pending==CFPERF_BACK?"Back next":m->dropped?"Drums off":
        m->fill==CFPERF_FILL_AUTO?"Turnaround":m->fill?"Fill":m->playing?"Playing":"Stopped";
    char tempo[16];
    kint(tempo,m->bpm,m->ext?" EXT":" BPM");
    cfn_page_header("GLO Live",tempo,C_ACC(0));

    fb_rbox(4,33,122,18,2,CGU_GLASS);
    cgu_lamp(14,42,m->playing || m->waiting || m->paused,m->waiting || m->paused?CGU_BAND_INK_BASE+2:CGU_BAND_INK_BASE+1);
    cfn_fit(&CFN_12,24,46,status,98,FL_INK);
    for(unsigned i=0;i<CGU_GLO_VIEWS;i++){
        int x=130+i*54, selected=i==m->view;
        cfn_center(&CFN_12,x+26,46,tabs[i],selected?FL_INK:FL_INK2);
        if(selected)fb_hline(x,50,52,CGU_PROGRESSION_INK);
    }
    for(unsigned i=0;i<CGS_NPART;i++){
        unsigned part=cgu_part(i), bit=1u<<part;
        int x=4+i*58;
        uint8_t accent=CGU_INK_BASE+part;
        char name[16]={'K',(char)('1'+i),' ',0}, level[8];
        str_cpy(name+3,CGS_PART_NAME[part],sizeof name-3);
        kint(level,m->level[part],"%");
        fb_rbox(x,54,56,38,2,RAKT_CHORDS+part);
        fb_hline(x+3,56,50,accent);
        cfn_center(&CFN_11,x+28,67,name,FL_INK);
        cfn_center(&CFN_12,x+28,79,level,FL_INK);
        cgu_lamp(x+10,86,!(m->cut&bit),accent);
        cfn_center(&CFN_11,x+32,90,cgu_glow_part_state(m,part),FL_INK2);
    }
}

static void cgu_glow_draw(void)
{
    static cgu_glow_model_t last;
    cgu_glow_model_t m={0};
    m.keys=cgu_glow.keys;
    m.ext=cs.clock_in==1u;
    m.bpm=m.ext?cf_sn.ext_bpm:cs.bpm;
    m.view=cgu_glow.view;
    for(unsigned i=0;i<=CGU_GLO_SWEEP;i++)m.ready|=(uint16_t)(cgu_glow_ready(i)<<i);
    m.muted=cgs.muted;
    m.cut=(uint8_t)cgs_mute_mask();
    m.solo=cgs.solo|cgs.held_solo;
    m.drums=cgs.drum_mutes;
    memcpy(m.level,cgs.level,sizeof m.level);
    m.paused=cf_sn.ac_paused;
    m.waiting=cf_sn.ac_waiting || cf_sn.ac_lost;
    m.playing=cf_sn.running;
    m.pending=cf_sn.perf_pending;
    m.fill=cgs.fill_held?CFPERF_FILL_MANUAL:cf_sn.perf_fill;
    m.dropped=cf_sn.perf_dropped;
    m.repeat=cfx_repeat_request?(uint8_t)(8u<<cgu_glow.repeat_rate):0;
    m.wave_genre=cgs_active()?cgs.playing_genre:cgs.genre;
    m.wave_density=cgs.energy;
    unsigned swing=cgs.meter==6u?0:cgs.swing;
    int effect=cfx_riser_step || cfx_repeat_request || cfx_echo_request || cfx_sweep_step;
    m.wave_bend=(uint8_t)clampi((int)swing+(effect?30:0),0,100);
    if(cf_motion)m.wave_phase=(uint8_t)(cu.now/100u);
    if(cu.pop==POP_MSG)str_cpy(m.message,cu.msg,sizeof m.message);
    if(cgu_glow_valid && !fb_full && !memcmp(&m,&last,sizeof m))return;
    last=m;cgu_glow_valid=1;
    cgu_glow_header(&m);
    int left=m.view==CGU_GLO_MAP?8:4, width=240-left*2;
    int height=CGU_GLOW_BOTTOM-CGU_GLOW_TOP;
    fb_rbox(left,CGU_GLOW_TOP,width,height,2,CGU_METAL+3);
    cgu_panel_waves(left+1,CGU_GLOW_TOP+1,width-2,height-2,m.wave_genre,
        m.wave_density,m.wave_bend,m.wave_phase,CGU_METAL+1);
    if(m.view==CGU_GLO_MAP)cgu_glow_map(&m);
    else cgu_glow_state(&m);
    static const uint8_t rainbow[]={CGU_INK_BASE+CGS_CHORDS,CGU_INK_BASE+CGS_BASS,
        CGU_INK_BASE+CGS_LAYER,RAKT_GREEN};
    for(unsigned i=0;i<NELEM(rainbow);i++)fb_hline(8+56*(int)i,226,56,rainbow[i]);
    char repeat[28]="Repeat 1/";
    if(m.repeat)kint(repeat+9,m.repeat,0);
    if(m.repeat)str_cpy(repeat+str_len(repeat),"   PRESETS Rate",sizeof repeat-str_len(repeat));
    cfn_fit(&CFN_11,10,237,m.repeat?repeat:m.message[0]?m.message:"Black mute   ALGO View   SAVE Settings",220,
        m.repeat || m.message[0]?FL_INK:FL_INK2);
    cf_present();
}
