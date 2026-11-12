/* SPDX-License-Identifier: GPL-3.0-only
 * Native adapters for every retained control surface. The existing controllers,
 * settings, record store and sound engines remain the only owners of behavior.
 * Bounded copied models allow a resting page to skip raster/hash/LCD work.
 */
enum { N_PARAMS, N_LINES, N_GRID, N_KEYS, N_NAME, N_ENV, N_LIST, N_RESULT, N_FX };
typedef struct {
    char title[24],section[48],status[24],hint[64],detail[64];
    char label[4][12],value[4][14],row[9][48],right[4][24];
    uint32_t bits[8],ends[8],keys;
    uint8_t kind,skin,rows,focus,columns,first,cursor,playrow,playcol,disabled[4],emphasis[4];
    uint8_t progress,cursor_width,focus_weight,env[4],name_cursor,rec,cols[8],grid_label_width,page,npages;
    int8_t dx;
} cfn_page_t;
_Static_assert(sizeof(cfn_page_t)<1200,"native page model stays bounded");
static cfn_page_t cfn_page_last;
static uint8_t cfn_page_valid;
static unsigned cfn_page_draws;
static const char *const CFN_GLOBAL[]={"Display","MIDI","Channels","Play","Reset","Sound library","Diagnostics","Harmony","Variation","Accompany","Browse","Recordings","Tools","Engine Locks"};
static void cfn_string(char *dst,const char *src,unsigned n){str_cpy(dst,src,n);}
#define NP(field,text) cfn_string(m->field,text,sizeof m->field)
static void cfn_diag_row(cfn_page_t *m,unsigned row,const char *label,uint32_t a,const char *mid,uint32_t b,const char *unit)
{
    char n[16];str_cpy(m->row[row],label,sizeof m->row[row]);diag_num(n,a);
    str_cpy(m->row[row]+str_len(m->row[row]),n,sizeof m->row[row]-str_len(m->row[row]));
    if(mid){str_cpy(m->row[row]+str_len(m->row[row]),mid,sizeof m->row[row]-str_len(m->row[row]));diag_num(n,b);str_cpy(m->row[row]+str_len(m->row[row]),n,sizeof m->row[row]-str_len(m->row[row]));}
    if(unit)str_cpy(m->row[row]+str_len(m->row[row]),unit,sizeof m->row[row]-str_len(m->row[row]));
}
static void cfn_diagnostics(cfn_page_t *m)
{
    const uint32_t *v=cfd.v;unsigned hw=v[D_HARDWARE];m->kind=N_LINES;m->rows=8;
    NP(section,CF_DIAG_PAGES[cfd.page]);NP(status,cfd.hold?"HOLD":hw?"LIVE":"HOST");
    switch(cfd.page) {
    case DP_OVERVIEW:
        if(hw && v[D_HALVES]){cfn_diag_row(m,0,"Audio ",diag_pct(v[D_CPU_Q8],256),"% / peak ",diag_pct(v[D_PEAK],v[D_BUDGET]),"%");cfn_diag_row(m,1,"Work ",v[D_PEAK]," / ",v[D_BUDGET]," us");}
        else {NP(row[0],"Timing: device required");NP(row[1],"Host CPU is not device CPU");}
        cfn_diag_row(m,2,"Voices C ",v[D_CHORD]," / B ",v[D_BASS],0);
        cfn_diag_row(m,3,"Layer ",v[D_LAYER]," / drums ",v[D_DRUMS],0);
        cfn_diag_row(m,4,"Shed ",v[D_SHED]," / late ",v[D_LATE],0);
        cfn_diag_row(m,5,"USB under ",v[D_UNDER]," / over ",v[D_OVER],0);
        if(hw){cfn_diag_row(m,6,"RAM ",v[D_RAM]/1024," / ",v[D_RAM_CAP]/1024,"K");cfn_diag_row(m,7,"Pool ",v[D_POOL]/1024," / ",v[D_POOL_CAP]/1024,"K");}
        else {NP(row[6],"RAM / pool: device build");cfn_diag_row(m,7,"Session ",v[D_SESSION_MS]/1000,0,0," s");}break;
    case DP_TIMING:
        cfn_diag_row(m,0,"Session ",v[D_SESSION_MS]/1000,0,0," s");
        if(hw){cfn_diag_row(m,1,"Average ",v[D_AVG]," / peak ",v[D_PEAK]," us");cfn_diag_row(m,2,"Wall ",v[D_WALL]," / lag ",v[D_LAG]," us");cfn_diag_row(m,3,"Lag + wall ",v[D_PRESSURE],0,0," us");}
        else {NP(row[1],"CPU: device required");NP(row[2],"Host timing is not shown");}
        cfn_diag_row(m,4,"Late ",v[D_LATE]," / stale ~",v[D_STALE],0);
        cfn_diag_row(m,5,"Shed ",v[D_SHED]," / steal ",v[D_STEAL],0);
        cfn_diag_row(m,6,"UI ",v[D_UI_HZ]," Hz / gap ",v[D_UI_GAP]," ms");
        cfn_diag_row(m,7,"Tail ",v[D_TAIL]," / core1 state ",v[D_CORE1],0);break;
    case DP_MEMORY:
        if(hw){cfn_diag_row(m,0,"RAM ",v[D_RAM]," / ",v[D_RAM_CAP]," B");cfn_diag_row(m,1,"RAM left ",v[D_RAM_CAP]-v[D_RAM],0,0," B");cfn_diag_row(m,2,"Pool left ",v[D_POOL_CAP]-v[D_POOL],0,0," B");cfn_diag_row(m,3,"XIP ",v[D_XIP]/1024," / ",v[D_XIP_CAP]/1024,"K");cfn_diag_row(m,4,"RAM code ",v[D_RAMTEXT],0,0," B");}
        else {NP(row[0],"Memory: device linker");NP(row[1],"Host sizes are not shown");}
        NP(row[5],"Stack0: not instrumented");
        if(v[D_CORE1]==C1_OK)cfn_diag_row(m,6,"Core1 stack ",v[D_C1_STACK]," / ",2816," B");else NP(row[6],"Core1: off / unavailable");
        NP(row[7],"Static RAM is not heap");break;
    default:
        NP(row[0],!(v[D_USB_STATE]&1)?"USB off":!(v[D_USB_STATE]&2)?"USB closed":v[D_USB_STATE]&(4|8|16)?"USB hold / suspend":"USB capture");
        cfn_diag_row(m,1,"Rate ",v[D_USB_RATE],0,0," Hz");cfn_diag_row(m,2,"Ring ",v[D_RING]," / ",v[D_RING_CAP]," frames");
        cfn_diag_row(m,3,"Underruns ",v[D_UNDER],0,0,0);cfn_diag_row(m,4,"Overruns ",v[D_OVER],0,0,0);cfn_diag_row(m,5,"Missed ",v[D_MISS],0,0,0);
        if(hw)cfn_diag_row(m,6,"Resample peak ",v[D_RS_PEAK],0,0," us");else NP(row[6],"Timing: device required");
        cfn_diag_row(m,7,"Stale estimate ",v[D_STALE],0,0,0);break;
    }
}
/* A grid displays the selected four-bar window, preserving exact event time.
 * Starts and sustained occupancy differ; the cursor never sweeps past a slot. */
static void cfn_record_grid(cfn_page_t *m,const sq_pat_t *p)
{
    m->kind=N_GRID;m->rows=4;m->first=squ.bar/4*4;
    m->columns=squ.trk==SQ_DRUMS?sq_dsteps():SQ_ST(p);if(m->columns>32)m->columns=32;
    m->focus=squ.bar-m->first;m->cursor=squ.nofr?255:squ.step;
    for(unsigned r=0;r<4 && m->first+r<SQ_BARS;r++) {
        unsigned bar=m->first+r;kint(m->row[r],bar+1,0);
        unsigned columns=squ.trk==SQ_DRUMS?sq_dsteps():sq_steps(p,squ.trk,bar);if(columns>32)columns=32;m->cols[r]=columns;
        if(squ.trk==SQ_DRUMS){m->bits[r]=p->hit[squ.lane%SQ_NLANE][bar];for(unsigned l=0;l<SQ_NLANE;l++)m->ends[r]|=p->hit[l][bar];}
        else for(unsigned e=0;e<p->nev;e++) {
            const sq_ev_t *v=&p->ev[e];if(v->trk!=squ.trk || !sq_ev_on(v))continue;
            uint32_t start=sq_ev_start(p,v),end=sq_ev_end(p,v),base=bar*SQ_U;
            for(unsigned c=0;c<columns;c++) {
                unsigned a=base+c*SQ_U/columns,b=base+(c+1)*SQ_U/columns;
                if(start<b && end>a)m->ends[r]|=1u<<c;
                if(start>=a && start<b)m->bits[r]|=1u<<c;
            }
        }
    }
    if(sq_playing_here()) {
        unsigned bar=sq_trk_bar(p,squ.trk,cf_sn.sq_n);
        if(bar>=m->first && bar<m->first+4){m->playrow=bar-m->first;m->playcol=cf_sn.sq_t*m->cols[m->playrow]/cf_sn.sq_bt;}
    }
    if(squ.trk==SQ_DRUMS)NP(detail,sq_lane_name(squ.lane));
    else {int at=sq_at(p,squ.trk,squ.bar,squ.step);if(at>=0)sq_ev_name(p,at,m->detail);else NP(detail,"Keys write the selected step");}
    if(squ.mv)NP(detail,"Moving chord / SELECT places");if(squ.cpy)NP(detail,"Erasing selected steps");
    if(squ.ocpy){kint(m->detail,squ.ofrom+1," copied to bar ");kint(m->detail+str_len(m->detail),squ.bar+1,0);}
    NP(hint,"SEL clears / SAVE back / GLO help");
}
static void cfn_page_model(cfn_page_t *m)
{
    memset(m,0,sizeof *m);m->skin=cf_skin;m->rows=4;m->kind=N_PARAMS;m->focus=m->cursor=m->playrow=m->playcol=255;m->dx=cfn_page_dx();m->grid_label_width=27;
    NP(title,SCR_NAME[cu.scr]);NP(status,cf_acc_transport());NP(hint,"HOME back / SELECT page");
    m->page=cu.page[cu.scr];m->npages=screen_pages(cu.scr);
    if(m->npages<2)NP(hint,"HOME back");
    for(unsigned k=0;k<4;k++){cf_knob_t o;memset(&o,0,sizeof o);ui_knob_info(k,&o);str_cpy(m->label[k],o.name,sizeof m->label[k]);str_cpy(m->value[k],o.val,sizeof m->value[k]);m->disabled[k]=o.dis;m->emphasis[k]=cfn_knob(k);}
    switch(cu.scr) {
    case SCR_HOME:
        if(home_view()==HV_DIAG){NP(title,"Diagnostics");cfn_diagnostics(m);break;}
        name_update();NP(title,home_view()==HV_KEYS?"Keyboard":"Live chord");NP(section,cu_name[0]?cu_name:"Play a chord");NP(detail,cu_name_live?(cf_sn.out.hold?"HOLD / sounding":"Sounding"):"Last chord");
        m->kind=N_KEYS;m->keys=cf_sn.keys_held;NP(hint,"ALGORITHM layout / PRESETS sound");break;
    case SCR_ENV:
        {static const char *const target[]={"Chord + bass","Chord","Bass","Layer"};NP(title,"Envelope");NP(section,target[cu.env_tgt%4]);m->kind=N_ENV;
         m->page=cu.env_tgt%4;m->npages=4;for(unsigned k=0;k<4;k++)m->env[k]=cf_env_get(env_part(),k);NP(hint,"SELECT sound / EDIT advanced");}break;
    case SCR_EDIT:
        NP(title,cf_snd[cu.edit_part].name);NP(section,cfe_page_title(cu.edit_part,cu.edit_sec,cu.page[SCR_EDIT]));NP(hint,"SELECT page / keys section or part");break;
    case SCR_SEL:
        NP(title,"Chord buttons");NP(section,cf_is_off(&cs.chord,cu.sel_btn)?"HOLD":ch.mode==CF_H_AUTO?cf_modifier_name(ch.slot[cu.sel_btn]):cf_type_name(sel_form()->type));NP(hint,"SELECT button / HOME back");break;
    case SCR_BASS:NP(title,"Linked bass");NP(section,cf_snd[CF_PART_BASS].name);break;
    case SCR_FX:
        cfx_sync();NP(title,fxp_scope?CGS_PART_NAME[fxp_scope-1u]:"Effects");
        if(fxpu.mode==FXP_MANAGE){
            m->kind=N_NAME;
            NP(section,fxpu.action==4?"Previous FX":fxpu.action==3?"Clear FX":fxpu.action==2?"Delete user FX":fxpu.action?"Replace user FX":"Save new FX");
            NP(detail,fxpu.action>=3?"Hold SAVE to confirm":fxpu.name);m->name_cursor=fxpu.cursor;
            NP(hint,"Hold SAVE 1 second / FX back");
            if(cu.save_hold)m->progress=(cu.now-cu.save_hold>=1000)?100:(cu.now-cu.save_hold)/10;
        } else {
            m->kind=N_FX;m->focus=fxpu.mode==FXP_EDIT;m->page=0;m->npages=1;
            NP(section,m->focus?"Individual":"Macros");
            NP(row[0],m->focus?CFX_NAME[cfx_sel()]:cfx_loaded_name());
            if(cfxu.edited)str_cpy(m->row[0]+str_len(m->row[0])," *",sizeof m->row[0]-str_len(m->row[0]));
            NP(row[1],m->focus?(cfx_is_on(cfx_sel())?"On":"Off"):cfxu.loaded<FXP_FACTORY_N?CFX_CAPTION[cfxu.loaded]:"Your effect settings");
            NP(row[2],fxp_pick_name());
            if(!m->focus && (fxpu.pick!=cfxu.loaded || cfxu.edited)) {
                NP(row[1],"Browse: ");str_cpy(m->row[1]+str_len(m->row[1]),fxp_pick_name(),sizeof m->row[1]-str_len(m->row[1]));
            }
            kint(m->right[0],fxpu.pick<FXP_FACTORY_N?fxpu.pick+1:fxpu.pick-FXP_FACTORY_N+1,fxpu.pick<FXP_FACTORY_N?" / 13":" / 16 user");
            if(m->focus)kint(m->right[0],cu.fx_pos+1," / 6");
            NP(detail,m->focus?"SELECT effect":"SELECT browse / SEL load");
            NP(hint,m->focus?"ALGO < Macros / SAVE manage":"ALGO > Individual / SAVE manage");
            m->keys=cfxu.vis_ms;m->env[0]=cfxu.level;m->cursor=(uint8_t)cfx_sel();
            const cf_fx_t *fx=fxp_active_chain();
            for(unsigned e=0;e<CFX_N;e++)m->bits[e]=fx->v[e][0] | (uint32_t)fx->v[e][1]<<8 | (uint32_t)fx->v[e][2]<<16 | ((fx->on>>e)&1u)<<24;
            if(fxp_active_bypass())NP(row[1],"Chain bypassed");
            if(fxp_pending())NP(row[1],"Saving / keep power on");
        }break;
    case SCR_SAVE:
        m->kind=N_NAME;NP(title,cu.save_del?"Delete sound":"Save sound");NP(section,cf_save_part()==2?"Layer":cf_save_part()?"Bass":"Chord");NP(detail,cu.save_name);m->name_cursor=cu.save_cur;NP(hint,cu.save_del?"Hold SAVE to delete":"Hold SAVE to save / HOME cancel");
        if(cu.save_hold)m->progress=(cu.now-cu.save_hold>=1000)?100:(cu.now-cu.save_hold)/10;break;
    case SCR_ARP:NP(title,"Arpeggiator");NP(section,cu.page[SCR_ARP]?"Feel":"Pattern");NP(status,cu.arp_on?"ON":"OFF");break;
    case SCR_SEQ:
        NP(title,"Metronome");NP(section,dm_seq_to?"Next: Drum grid":"Next: Recordings");NP(hint,"SEQ opens / PLAY starts or stops");break;
    case SCR_DRUM: {
        unsigned r=dm_cur();const dm_rhythm_t *p=&DM_RHY[r%DM_N];m->kind=N_GRID;m->rows=3;m->columns=dm_steps(p->beats);m->grid_label_width=53;NP(title,"Drum grid");NP(section,p->name);NP(hint,"EDIT makes recording / GLO help");
        unsigned bar=dm_plays(r)?cf_sn.dm_bar:0;m->rows=0;
        for(unsigned l=0;l<SQ_NLANE && m->rows<8;l++)if(p->lane[l]){unsigned at=m->rows++;NP(row[at],sq_lane_name(l));m->bits[at]=dm_mask(r,l,bar);}
        if(dm_plays(r) && cf_sn.sq_bt){m->playrow=254;m->playcol=cf_sn.dm_t*m->columns/cf_sn.sq_bt;}
        kint(m->detail,bar+1," / ");kint(m->detail+str_len(m->detail),p->bars," bars");break;}
    case SCR_SQ: {
        const sq_pat_t *p=sq_cur();m->kind=N_LIST;NP(title,"Recordings");kint(m->section,sq_number(squ.pat)," / 3");m->focus=squ.trk;
        for(unsigned t=0;t<4;t++){NP(row[t],SQ_TRK_NAME[t]);str_cpy(m->right[t],(p->mute>>t)&1?"Muted":sq_track_has(p,t)?"Recorded":t==SQ_BASS && sq_count(p,SQ_CHORDS)?"Follows":"Empty",sizeof m->right[t]);}
        NP(hint,"SELECT track / EDIT opens / GLO help");NP(detail,"ALGO recording / hold SAVE clears");
        if(squ.sel_down && sq_track_has(p,squ.trk)){unsigned t=cu.now-squ.sel_down;m->progress=t>=SQ_CLEAR_MS?100:t*100/SQ_CLEAR_MS;NP(detail,"Hold SEL to clear track");}break;}
    case SCR_SQED:NP(title,SQ_TRK_NAME[squ.trk%SQ_NTRK]);kint(m->section,sq_number(squ.pat)," / Recording");cfn_record_grid(m,sq_cur());break;
    case SCR_GLO: {
        unsigned p=cu.page[SCR_GLO]%CF_GLO_NPAGES;NP(title,"System");NP(section,CFN_GLOBAL[p]);
        if(p==6){cfn_diagnostics(m);break;}
        if(p==CF_GLO_RECORDINGS){m->kind=N_LINES;NP(row[0],"Keep the latest three");NP(row[1],"Newest recording is always 1");NP(row[2],sq_pref.protect?"Full: clear a recording first":"Finished take replaces oldest");NP(row[3],"Cancel keeps previous takes");}
        if(p==CF_GLO_BROWSE){m->kind=N_LINES;unsigned idea=cpb_target();NP(row[0],cf_progression_get(idea)->name);NP(row[1],cf_progression_get(idea)->context);NP(row[2],CPB_ORDER_NAME[cpb.mode]);kint(m->row[3],idea+1," / ");kint(m->row[3]+str_len(m->row[3]),cf_progression_count(),cf_browser_favorite(&cpb,idea)?" / Favorite":"");NP(hint,"SELECT browse / SAVE favorite");}
        if(p==CF_GLO_HARMONY)NP(detail,cf_harmony_supported(cs.chord.scale,cs.chord.quantize)?"HOLD sustains / Lock latches":"Auto requires a 7-note scale");
        if(p==CF_GLO_VARIATION){NP(detail,cvr.pending?"K4 cancels queued change":cvr.context==CV_FX || cvr.context==CV_SOUND?"SAVE keeps a preset":"Settings kept automatically");}
        if(p==4) {
            m->kind=N_LINES;NP(section,"Factory reset");NP(row[0],"Clears user sounds");NP(row[1],"Clears recordings");NP(row[2],"Resets settings");
            NP(row[3],cfr.go?"Resetting...":cfr.fin?"Press SAVE again to confirm":cfr.arm?"Hold SAVE for 3 seconds":"Turn ACTION to Reset");
            NP(hint,"HOME cancels / nothing erased yet");
            if(cfr.hold){unsigned elapsed=cu.now-cfr.hold;m->progress=elapsed>=CF_RESET_HOLD_MS?100:elapsed*100/CF_RESET_HOLD_MS;}
            if(cfr.fin){unsigned elapsed=cu.now-cfr.fin_ms;kint(m->detail,elapsed>=CF_RESET_ASK_MS?0:(CF_RESET_ASK_MS-elapsed+999)/1000," seconds to confirm");}
            if(cfr.go)NP(hint,"Resetting / keep power on");
        }break;}
    default:break;
    }
    char plaque[24];unsigned kind=dm_plaque(plaque,sizeof plaque);
    if(kind==DMP_REC || kind==DMP_RECIN){NP(status,plaque);m->rec=1;}
    if((cu.scr==SCR_SQ || cu.scr==SCR_SQED || cu.scr==SCR_DRUM) && (squ.help || squ.help_hold)) {
        m->kind=N_LINES;m->rows=8;NP(section,"Recording controls");
        NP(row[0],"SELECT moves / keys write");NP(row[1],"SEL clears / SAVE goes back");NP(row[2],"Hold EDIT + SELECT: move");NP(row[3],"Hold OCT+ + SELECT: copy bar");NP(row[4],"Hold OCT+ + ALGO: copy take");NP(row[5],"Hold SEL: clear track");NP(row[6],"Hold REC: cancel take");NP(row[7],"Hold SAVE: clear / GLO close");
        if(cu.scr==SCR_DRUM){m->rows=6;NP(section,"Drum controls");NP(row[0],"SELECT chooses a rhythm");NP(row[1],"SEL resets the rhythm");NP(row[2],"EDIT makes a recording");NP(row[3],"REC records over rhythm");NP(row[4],"Hold REC: cancel take");NP(row[5],"GLO closes help");}
    }
    m->focus_weight=cfn_focus(((unsigned)cu.scr<<16)|m->focus);
    m->cursor_width=cfn_cursor(((unsigned)cu.scr<<24)|((unsigned)m->playrow<<8)|m->playcol);
    if(cu.upd_left){m->kind=N_RESULT;m->npages=m->progress=0;NP(title,"Firmware update");NP(status,"");NP(section,"Keep holding");kint(m->detail,cu.upd_left," seconds");NP(hint,"Release to cancel");}
}
/* One shared response field: tails, repeats and modulation occupy the same
 * space. Post-MASTER energy controls its breath; this is not an oscilloscope. */
static unsigned cfn_fx_value(const cfn_page_t *m,unsigned e,unsigned k)
{ return (m->bits[e]>>(k*8))&255u; }
static int cfn_fx_on(const cfn_page_t *m,unsigned e)
{ return (int)((m->bits[e]>>24)&1u) && (!m->focus || m->cursor==e); }
static void cfn_fx_plot(const cfn_page_t *m,int dx)
{
    const int left=10+dx,width=220,bottom=169,height=59;
    unsigned energy=m->env[0],ms=m->keys;
    fb_hline(left,bottom,width,FL_BG3);
    if(cfn_fx_on(m,CFX_REVERB)) {
        unsigned mix=cfn_fx_value(m,CFX_REVERB,2),decay=cfn_fx_value(m,CFX_REVERB,1);
        int a=(12+(int)mix*47/100)*(150+(int)energy*105/255)/255;
        for(unsigned i=0;i<55;i++) {
            int h=a*(12+(int)((i*2654435761u)>>29))/16;
            if(h>height)h=height;
            if(h>0)fb_rect(left+(int)i*4,bottom-h,2,h,C_ACC(2));
            a=a*(205+(int)decay*45/100)/256;
        }
    }
    if(cfn_fx_on(m,CFX_DELAY)) {
        unsigned time=cfn_fx_value(m,CFX_DELAY,0),feedback=cfn_fx_value(m,CFX_DELAY,1),mix=cfn_fx_value(m,CFX_DELAY,2);
        unsigned period=xf_ms(time*127u/100u),step=10+time*30/100,n=(width-3)/step+1;
        int a=(8+(int)mix*47/100)*(150+(int)energy*105/255)/255;
        for(unsigned i=0;i<n;i++) {
            fb_rect(left+(int)(i*step),bottom-a,3,a,energy && i==(ms/period)%n?C_ACC(0):FL_INK2);
            a=a*(80+(int)feedback*17/10)/256;
        }
    }
    unsigned mod=cfn_fx_on(m,CFX_PHASER)?CFX_PHASER:CFX_CHORUS;
    if(cfn_fx_on(m,mod)) {
        unsigned rate=cfn_fx_value(m,mod,0),depth=cfn_fx_value(m,mod,1);
        uint32_t phase=mod==CFX_PHASER?(uint32_t)(((uint64_t)CFX_PRATE[rate]*FS/32000u)*ms):
            (uint32_t)((((uint64_t)LFO_HZ_X100[(8u+rate)&127u]<<32)/100000u)*ms);
        int amp=(7+(int)depth*22/100)*(150+(int)energy*105/255)/255,py=0;
        for(int x=0;x<width;x++) {
            int y=140-osc_sine(phase+(uint32_t)x*40000000u)*amp/32768;
            if(x)fb_line(left+x-1,py,left+x,y,C_ACC(0));py=y;
        }
    }
    if(cfn_fx_on(m,CFX_FILTER)) {
        unsigned type=cfn_fx_value(m,CFX_FILTER,0),cut=cfn_fx_value(m,CFX_FILTER,1),res=cfn_fx_value(m,CFX_FILTER,2);
        int corner=(int)cut*(width-1)/99,py=0;
        for(int x=0;x<width;x++) {
            int d=x-corner,ad=d<0?-d:d;
            int h=type==0?(d<0?40:40-d*height/28):type==2?(d>0?40:40+d*height/28):40-ad*height/28;
            if(ad<8)h+=(int)res*height/160*(8-ad)/8;
            if(h<0)h=0;if(h>height)h=height;
            if(x)fb_line(left+x-1,py,left+x,bottom-h,C_ACC(0));py=bottom-h;
        }
    } else if(cfn_fx_on(m,CFX_DIST)) {
        int py=0;unsigned drive=cfn_fx_value(m,CFX_DIST,0);
        for(int x=0;x<width;x++) {
            int32_t in=(x*2-width)*32768/width;
            int y=140-softclip((int32_t)(((int64_t)in*CFX_DRIVE[drive])>>8))*28/32768;
            if(x)fb_line(left+x-1,py,left+x,y,C_ACC(0));py=y;
        }
    }
    if(energy)fb_rect(left,bottom-(int)energy*height/255,2,(int)energy*height/255,C_ACC(0));
}
static void cfn_page_draw(void)
{
    cfn_page_t m;cfn_page_model(&m);
    if(cfn_page_valid && !fb_full && !memcmp(&m,&cfn_page_last,sizeof m))return;
    cfn_page_last=m;cfn_page_valid=1;cfn_page_draws++;
    cfn_page_header(m.title,m.status,m.rec?FL_C(FL_R_ROSE,0):C_ACC(0));
    const int rack=CF_STUDIO && !CF_SCENES;
    uint8_t panel=rack?CGU_METAL+1:FL_BG1, rule=rack?CGU_METAL+3:FL_BG3;
    int x=m.dx, right=230+(rack?0:x);
    fb_clip(rack?8:0,31,rack?232:240,184);
    cfn_fit(&CFN_16,10+x,49,m.section,m.npages>1?180:220,FL_INK);
    if(m.kind==N_FX) {
        cfn_fit(&CFN_32,10+x,83,m.row[0],220,FL_INK);
        cfn_fit(&CFN_12,10+x,103,m.row[1],220,FL_INK2);
        cfn_text(&CFN_12,right-cfn_width(&CFN_12,m.right[0]),49,m.right[0],FL_INK2);
        cfn_fx_plot(&m,x);
    }
    if(m.npages>1){char page[16];kint(page,m.page+1,"/");kint(page+str_len(page),m.npages,0);cfn_text(&CFN_12,right-cfn_width(&CFN_12,page),49,page,FL_INK2);}
    if(m.kind==N_PARAMS)for(unsigned k=0;k<4;k++) {
        int y=76+29*k;cfn_fit(&CFN_12,10+x,y,m.label[k],103,FL_INK2);
        cfn_fit(&CFN_16,116+x,y,m.value[k],114,m.disabled[k]?FL_INK2:FL_INK);fb_hline(10+x,y+7,220,panel);
    }
    if(m.kind==N_LINES)for(unsigned r=0;r<m.rows;r++)cfn_fit(m.rows>4?&CFN_12:&CFN_16,10+x,69+r*(m.rows>4?15:28),m.row[r],220,r?FL_INK2:FL_INK);
    if(m.kind==N_LIST)for(unsigned r=0;r<m.rows;r++) {
        int y=65+r*27;if(r==m.focus){fb_rect(10+x,y-1,220,25,panel);fb_rect(10+x,y-1,2+m.focus_weight,25,C_ACC(0));}
        cfn_text(&CFN_16,20+x,y+17,m.row[r],FL_INK);cfn_fit(&CFN_12,116+x,y+17,m.right[r],107,FL_INK2);
    }
    if(m.kind==N_ENV) {
        int xa=20+x+8+m.env[0]*40/99,xd=xa+8+m.env[1]*40/99,xs=xd+30,xr=xs+8+m.env[3]*40/99,ys=167-94*m.env[2]/99;
        fb_line(20+x,167,xa,73,C_ACC(0));fb_line(xa,73,xd,ys,C_ACC(0));fb_line(xd,ys,xs,ys,C_ACC(0));fb_line(xs,ys,xr,167,C_ACC(0));
        cfn_text(&CFN_11,10+x,179,"A        D          S          R",FL_INK2);
    }
    if(m.kind==N_GRID && m.columns)for(unsigned r=0;r<m.rows;r++) {
        int y=64+r*(m.rows>4?13:26);cfn_fit(&CFN_12,10+x,y+12,m.row[r],m.grid_label_width,FL_INK2);
        unsigned columns=m.cols[r]?m.cols[r]:m.columns;
        int left=13+m.grid_label_width,width=230-left;
        for(unsigned c=0;c<columns;c++) {
            int gx=left+x+c*width/columns,w=width/columns-2;
            fb_rect(gx,y,w,m.rows>4?8:12,(m.bits[r]>>c)&1?C_ACC(0):(m.ends[r]>>c)&1?rule:panel);
            if(r==m.focus && c==m.cursor)fb_frame(gx-1,y-2,w+2,16,FL_INK);
            if((r==m.playrow || m.playrow==254) && c==m.playcol)fb_hline(gx,y+(m.rows>4?10:18),w*m.cursor_width/100,C_ACC(0));
        }
    }
    if(m.kind==N_KEYS) {
        cfn_fit(&CFN_32,10+x,94,m.section,220,FL_INK);
        for(unsigned k=0;k<CF_NKEYS;k++){int kx,ky,kw,kh;key_box(k,&kx,&ky,&kw,&kh);fb_rect(10+x+kx*220/240,125+(ky-Y_KEYS)*40/40,kw*220/240-1,kh*40/40-1,(m.keys>>k)&1?C_ACC(0):FL_BG1);}
    }
    if(m.kind==N_NAME || m.kind==N_RESULT) {
        cfn_fit(&CFN_32,10+x,103,m.detail,220,FL_INK);
        if(m.kind==N_NAME){char pre[32];str_cpy(pre,m.detail,(unsigned)m.name_cursor+1);int a=10+x+cfn_width(&CFN_32,pre);if(a<225)fb_hline(a,112,12,C_ACC(0));}
    } else cfn_fit(&CFN_12,10+x,181,m.detail,220,FL_INK2);
    if(m.progress){fb_rect(10+x,172,220,4,FL_BG1);fb_rect(10+x,172,220*m.progress/100,4,C_ACC(0));}
    fb_unclip();cfn_hint(m.hint,FL_INK2);if(!cu.upd_left)cfn_controls(m.label,m.value,m.disabled,m.emphasis);if(!cu.pop || cu.upd_left)cf_present();
}
#undef NP
