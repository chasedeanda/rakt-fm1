/* SPDX-License-Identifier: GPL-3.0-only
 * Draw the active workspace in the shared indexed framebuffer. Only the body
 * pans and scales. Tabs and controls are drawn at their fixed coordinates. */
static void cgu_take_model(cf_stage_model_t *m)
{
    int live=sqk.on && sqk.performance;
    unsigned slot=cf_sn.sq_on?cf_sn.sq_pat:sq_view_slot(cgu.take_slot);
    const sq_perf_t *take=live?&sq_perf_draft:&sq_perf[slot];
    unsigned bar=live?sqk.nbar:cf_sn.sq_on && take->bars?cf_sn.sq_n%take->bars:0;
    unsigned first=bar?bar-1:0;
    unsigned bt=take->meta.beats*SQ_PPQN;
    m->rec=live;
    m->phrase_steps=CGS_STEPS;
    if(!bt)return;
    unsigned n=take->n;
    RING_PUBLISH();
    for(unsigned i=0;i<n;i++){
        sq_perf_ev_t e=take->ev[i];
        unsigned tick=e.when&SQ_PERF_TICK, part=(e.when>>SQ_PERF_PART_SHIFT)&3u;
        unsigned b=tick/bt, step=(tick%bt)*CGS_STEPS/bt;
        if(b>=first && b<first+2 && (part==SQ_DRUMS || e.data>>7))
            m->steps[part][b-first]|=(uint16_t)(1u<<step);
    }
    if((live && sqk.rolled) || (!live && cf_sn.sq_on))
        m->beat=(uint8_t)(1+(bar-first)*CGS_STEPS+(live?sqk.t:cf_sn.sq_t)*CGS_STEPS/bt);
}

static void cgu_theory(cf_stage_model_t *m,int active)
{
    unsigned scale=active?cf_sn.ac_scale:cs.chord.scale;
    unsigned tonic=active?cf_sn.ac_tonic:cs.chord.tonic;
    unsigned meter=active?cf_sn.ac_beats:cgs.meter;
    char key[8],sig[8],tempo[12];
    kpc(key,(int)tonic);
    kint(sig,meter,meter==6u?"/8":"/4");
    kint(tempo,cs.clock_in==1u?cf_sn.ext_bpm:cs.bpm,cs.clock_in==1u?" EXT":" BPM");
    str_cpy(m->theory,key,sizeof m->theory);
    cf_stage_append(m->theory,sizeof m->theory," ");
    const cf_scale_t *mode=&CF_SCALES[scale%CF_NSCALES];
    const char *scale_name=mode->name;
    if(cfn_width(&CFN_11,key)+cfn_width(&CFN_11,scale_name)+
        cfn_width(&CFN_11,sig)+cfn_width(&CFN_11,tempo)+5*cfn_width(&CFN_11," ")>134)scale_name=mode->shrt;
    cf_stage_append(m->theory,sizeof m->theory,scale_name);
    cf_stage_append(m->theory,sizeof m->theory,"  ");
    cf_stage_append(m->theory,sizeof m->theory,sig);
    cf_stage_append(m->theory,sizeof m->theory,"  ");
    cf_stage_append(m->theory,sizeof m->theory,tempo);

    if(cf_sn.out.sounding && cf_sn.out.mask){
        cf_lab_numeral(scale,tonic,cf_sn.out.root_pc,cf_sn.out.mask,m->degree);
    } else if(active && cf_sn.ac_slot<cf_sn.ac_n){
        unsigned slot=cf_sn.ac_slot;
        cf_lab_numeral(scale,tonic,cf_sn.ac_root[slot]%12,cf_sn.ac_mask[slot],m->degree);
    } else if(cgs.ready && cpa_setup.plan.studio){
        const cpa_slot_t *s=&cpa_setup.plan.slot[0];
        cf_lab_numeral(scale,tonic,s->root%12,s->mask,m->degree);
    }
}

static unsigned cgu_workspace_tile(const cf_stage_model_t *m)
{ return cgu_rack_tile(m->workspace,m->focus,m->bank); }

static void cgu_phrase_model(cf_stage_model_t *m,unsigned parts,int active)
{
    uint8_t cells[CGS_NPART][2][CGS_STEPS], gaps[2][CGS_STEPS];
    uint8_t kicks[2][CGS_STEPS], bass_feel[2][CGS_STEPS], backbeat[2];
    int layer_gaps=0;
    cgs_clip_t layer;
    fm1_irq_off();
    uint32_t tick=cpa.t, bar=cgs.bar-cgs.origin_bar;
    unsigned meter=active?cpa.plan.beats:cgs.meter;
    for(unsigned i=0;i<CGS_NPART;i++)
        if(parts&(1u<<i))memcpy(cells[i],cgs.active[i].event,sizeof cells[i]);
    if((parts&(1u<<CGS_LAYER)) && !cgs_band_arp(CGS_LAYER) &&
       cgs.active[CGS_LAYER].responds){
        layer=cgs.active[CGS_LAYER];
        layer_gaps=cgs_layer_gaps(0)!=0;
        for(unsigned b=0;b<2;b++){
            if(layer_gaps)memcpy(gaps[b],cgs_layer_gaps(b),sizeof gaps[b]);
            backbeat[b]=(uint8_t)cgs_layer_backbeat(b,0);
        }
        m->layer_reply=layer.responds;
        if(m->layer_reply==CGS_LAYER_REPLY_GAPS && !layer_gaps)m->layer_reply=0;
        if(m->layer_reply==CGS_LAYER_REPLY_DREAM && layer_gaps)m->layer_reply=CGS_LAYER_REPLY_GAPS;
    }
    if((parts&(1u<<CGS_BASS)) && cgs.active[CGS_BASS].responds){
        memcpy(bass_feel,cgs.active[CGS_BASS].feel,sizeof bass_feel);
        for(unsigned b=0;b<2;b++)memcpy(kicks[b],cgs_bass_kicks(b,0),sizeof kicks[b]);
        m->bass_reply=cgs.active[CGS_BASS].responds;
    }
    fm1_irq_on();
    m->phrase_steps=(uint8_t)cgs_steps(meter);
    if(active && !cf_sn.ac_paused && !cf_sn.ac_waiting){
        unsigned tick_step=meter==6u?CGS_TICKS*2u:CGS_TICKS;
        m->beat=(uint8_t)(1+(bar&1u)*CGS_STEPS+tick/tick_step);
    }
    for(unsigned i=0;i<CGS_NPART;i++){
        if(!(parts&(1u<<i)))continue;
        for(unsigned b=0;b<2;b++){
            uint16_t events=i==CGS_CHORDS?1u:0u;
            if(i==CGS_LAYER && (m->layer_reply==CGS_LAYER_REPLY_HARMONY ||
                                m->layer_reply==CGS_LAYER_REPLY_DREAM)){
                unsigned feel=0, delay=(m->layer_reply==CGS_LAYER_REPLY_HARMONY?4u:2u)*CGS_TICKS;
                m->steps[i][b]=cgs_first_fragment(&layer,b,&feel)?(uint16_t)(1u<<(delay/cgs_step_ticks(meter))):0;
                continue;
            }
            uint8_t pulled=0;
            for(unsigned j=0;j<m->phrase_steps;j++){
                unsigned feel=0, note=cells[i][b][j];
                if(i==CGS_LAYER && m->layer_reply==CGS_LAYER_REPLY_GAPS)
                    note=cgs_gap_reply(&layer,gaps[b],b,j,&feel);
                else if(i==CGS_LAYER && m->layer_reply==CGS_LAYER_REPLY_SNARE)
                    note=cgs_snare_reply(&layer,layer_gaps?gaps[b]:0,backbeat[b],b,j,&feel);
                else if(i==CGS_BASS && m->bass_reply){
                    feel=bass_feel[b][j];
                    note=cgs_kick_bass(m->bass_reply,cells[i][b],bass_feel[b],kicks[b],j,m->phrase_steps,&feel,&pulled);
                    if(m->bass_reply==CGS_BASS_REPLY_LATE && feel!=bass_feel[b][j])
                        m->bass_delays[b]|=(uint16_t)(1u<<j);
                }
                if(note)events|=(uint16_t)(1u<<j);
            }
            m->steps[i][b]=events;
        }
    }
}

static void cgu_model(cf_stage_model_t *m)
{
    int active=cgs_active();
    unsigned idea=active?cf_sn.ac_idea:cpa_setup.plan.idea;
    const cf_progression_t *p=(active || cgs.ready)?cf_progression_get(idea):0;
    m->engine_page=3;
    m->workspace=cgu.tab;
    m->zoom=(uint8_t)cgu_zoom();
    int detail=cgs.zoom || (cgu.tab<=CGU_SOUND && m->zoom>38);
    m->signal_peak=(uint8_t)cfn_motion.energy;
    m->bank=cgu.tab==CGU_STUDIO?cgu.band_panel:cgu.rack_controls;
    m->bank_dx=(int16_t)cf_rack_arrival(cgu.tab_ms,RAKT_W*cgu.tab_dir);
    if(detail)m->control_dx=(int8_t)cf_rack_arrival(cgu.page_ms,16*cgu.page_dir);
    m->shaping=cgu.band_panel==CGU_BAND_ARP?cgu_arp_page(detail):(detail?cgu.part_page:CGU_PART);
    m->sound_page=cgu_tone_page(detail);
    m->genre=cgs.genre;
    m->held_parts=cgs.hold;
    m->solo=cgs.solo|cgs.held_solo;
    m->muted=cgs.muted;
    m->focus=cgu.tab==CGU_HELP?cgu.topic:cgs.focus;
    m->live=cgs.zoom;
    m->pin_mask=(uint8_t)cgs_mute_mask();
    memcpy(m->part_level,cgs.level,sizeof m->part_level);
    m->activity=cf_sn.visual_parts & ~m->pin_mask;
    str_cpy(m->status,sqk.on || cf_sn.sq_rec!=255?"Recording":
        active && (cf_sn.ac_waiting || cf_sn.ac_paused)?"MIDI wait":
        active || cf_sn.sq_on || cf_sn.dm_on?"Playing":
        cgu.tab==CGU_STUDIO?"PLAY to start":"Ready",sizeof m->status);
    static const char *const feel[]={"Low Tide","Glass Chorus","Night Drive"};
    str_cpy(m->context,p?p->name:idea>=CF_P_COMPOSED && idea<CF_P_COMPOSED+NELEM(feel)?
        feel[idea-CF_P_COMPOSED]:"Press PLAY",sizeof m->context);
    if(idea==CF_P_DRAFT)str_cpy(m->context,cgs_custom_harmony.name,sizeof m->context);
    cgu_theory(m,active);
    m->rack_scroll=(uint16_t)cgu_rack_scroll();
    if(cgu.tab==CGU_STUDIO){
        int running=active && !cf_sn.ac_waiting && !cf_sn.ac_paused;
        int rack=m->zoom<100 && m->rack_scroll+CGU_BAY_H>cgu_rack_offset(CGU_STUDIO,CGS_NPART);
        if(rack || m->bank==CGU_BAND_HARMONY){
            m->rack_meter=active?cf_sn.ac_beats:cgs.meter;
            if(cf_motion && running && cf_sn.ac_beat<m->rack_meter)m->rack_beat=cf_sn.ac_beat+1u;
        }
        if(rack || m->bank==CGU_BAND_SHAPING)
            m->rack_level=(uint8_t)((m->signal_peak+31u)/32u);
        if((rack || m->bank==CGU_BAND_ARP) && (m->focus==CGS_CHORDS || m->focus==CGS_LAYER)){
            m->rack_arp_mode=(uint8_t)cgu_arp_mode();
            if(cf_motion && m->rack_arp_mode)m->rack_arp_step=cf_sn.studio_arp_step;
        }
        if(rack || m->bank==CGU_BAND_PROGRESSION){
            m->rack_draft=(uint8_t)(active?idea==CF_P_DRAFT:cgs.custom);
            if(active){
                m->count=cf_sn.ac_n;
                if(cf_sn.ac_slot<cf_sn.ac_n)m->slot=cf_sn.ac_slot+1u;
            }
        }
        if(m->bank)m->rack_lens=(uint8_t)cgu_rack_lens();
        if(active && !cf_sn.ac_waiting && !cf_sn.ac_paused){
            m->section=cgs.section&CGS_FORM_LAYER_REST;
            if(cf_sn.perf_fill==CFPERF_FILL_AUTO)m->section|=CGS_FORM_FILL;
        }
        if(m->bank==CGU_BAND_PROGRESSION)
            str_cpy(m->applied,(active?idea==CF_P_DRAFT:cgs.custom)?cgs_custom_harmony.name:"Genre bank",sizeof m->applied);
        if(active && cpa_setup.flags)str_cpy(m->pending,
            cpa_setup.plan.idea==CF_P_DRAFT?"Draft next bar":cpa.plan.idea==CF_P_DRAFT?"Bank next bar":
            cgs.genre!=cgs.playing_genre?"Genre next bar":"Take next bar",sizeof m->pending);
        else if(cgs.pending)str_cpy(m->pending,"Part queued",sizeof m->pending);
        else if(cgu.action && cu.now-cgu.action_ms<1300)
            str_cpy(m->pending,cgu.action==1?"Part regenerated":"New take ready",sizeof m->pending);
    }
    if(cgu.tab==CGU_STUDIO || cgu.tab==CGU_SOUND){
        for(unsigned i=0;i<3;i++)str_cpy(m->rail[i],cf_snd[i].name,sizeof m->rail[i]);
        str_cpy(m->rail[CGS_DRUMS],SQ_KITS[cgu_kit()%SQ_NKIT].name,sizeof m->rail[0]);
        unsigned drum_tile=cgu_part(CGS_DRUMS);
        int drum_top=cgu_rack_offset(m->workspace,drum_tile)-(int)m->rack_scroll;
        if(cf_motion && !(m->pin_mask&(1u<<CGS_DRUMS)) &&
           (m->zoom==100?m->focus==CGS_DRUMS && !m->bank:drum_top<CGU_BAY_H && drum_top+cgu_rack_height(m->workspace,drum_tile)>0)){
            for(unsigned lane=0;lane<SQ_NLANE;lane++)
                if(cf_sn.sq_lane_t[lane] && cf_sn.now_ms-cf_sn.sq_lane_t[lane]<120u)
                    m->drum_groups|=(uint8_t)(1u<<cgs_drum_group(lane));
        }
    }
    if(cgu.tab==CGU_SOUND){
        if(!m->bank && m->sound_page==CGU_PRESETS){
            cfx_sync();
            str_cpy(m->applied,cfx_loaded_name(),sizeof m->applied);
            str_cpy(m->next_setup,fxp_pick_name(),sizeof m->next_setup);
        } else if(detail && !m->bank && m->sound_page==CGU_SENDS){
            const cf_fx_t *fx=fxp_track_chain(m->focus);
            memcpy(m->fx_send_shape[1],fx->v[CFX_CHORUS],2);
            memcpy(m->fx_send_shape[2],fx->v[CFX_DELAY],2);
            memcpy(m->fx_send_shape[3],fx->v[CFX_REVERB],2);
        } else if(detail && !m->bank && m->sound_page==CGU_TONE && m->focus==CGS_DRUMS){
            for(unsigned lane=0;lane<SQ_NLANE;lane++)
                if(cf_sn.sq_lane_t[lane] && cf_sn.now_ms-cf_sn.sq_lane_t[lane]<120u)
                    m->drum_hits|=(uint16_t)(1u<<lane);
        }
        if(active && (cpa_edit.flags&CPA_GROOVE))str_cpy(m->pending,"Kit queued",sizeof m->pending);
    }
    if(cgu.tab==CGU_RECORD && ((sqk.on && sqk.performance) ||
       (cf_sn.sq_on?sq_perf[cf_sn.sq_pat].bars:!active && sq_perf[sq_view_slot(cgu.take_slot)].bars)))
        cgu_take_model(m);
    else if(cgu.tab==CGU_RECORD)cgu_phrase_model(m,(1u<<CGS_NPART)-1u,active);
    else if(cgu.tab==CGU_STUDIO && detail && !m->bank && m->shaping==CGU_PART_EVOLVE)
        cgu_phrase_model(m,1u<<m->focus,active);
    if(cgu.tab==CGU_HELP && m->focus==CGU_HARMONY && m->zoom>82)cf_learn_model(&m->learn);
    if(cgu.tab==CGU_STUDIO && cgs.focus==CGS_DRUMS){
        m->moment=cf_sn.perf_pending;
        m->build=cf_sn.perf_dropped;
        m->bloom=cf_sn.perf_fill;
    }
    if(cgu.tab==CGU_SOUND)m->build=cgu.all_sounds;
    if(cgu.tab==CGU_RECORD){
        m->slot=cgu.take_slot;
        for(unsigned i=0;i<SQ_NPAT;i++){
            unsigned slot=sq_view_slot(i);
            if(sq_has(slot))m->count|=(uint8_t)(1u<<i);
            if(sq_unavailable&(1u<<slot))m->total|=(uint8_t)(1u<<i);
        }
        if(cgu.clear_down)m->pulse=(uint8_t)clampi((int)(cu.now-cgu.clear_down)/30,0,100);
    }
    if(m->workspace==CGU_STUDIO && m->zoom<100 && m->rack_scroll+CGU_BAY_H>cgu_rack_offset(CGU_STUDIO,CGS_NPART)){
        for(unsigned band=CGU_BAND_HARMONY;band<CGU_BAND_PANELS;band++){
            if(band==m->bank)continue;
            for(unsigned i=0;i<4;i++){
                cf_knob_t knob={.bar=-1};
                cgu_knob_read(cgu_band_knob_panel(band),(int)i,&knob,0);
                if(knob.bar>=0)m->rack_knob[band-1u][i]=(uint8_t)(knob.bar*100/255);
            }
        }
        if(m->bank!=CGU_BAND_PROGRESSION)m->rack_knob[CGU_BAND_PROGRESSION-1u][0]=cgs.custom?100:0;
    }
    if(m->workspace<=CGU_SOUND && m->zoom<100){
        unsigned selected=cgu_workspace_tile(m);
        for(unsigned tile=0;tile<CGS_NPART;tile++){
            int top=cgu_rack_offset(m->workspace,tile)-(int)m->rack_scroll;
            if(tile==selected || top+cgu_rack_height(m->workspace,tile)<=0 || top>=CGU_BAY_H)continue;
            unsigned part=cgu_part(tile);
            for(unsigned i=0;i<4;i++){
                cf_knob_t knob={.bar=-1};
                if(m->workspace==CGU_SOUND)cgu_sound_info(i,&knob,part,CGU_TONE);
                else cgs_knob_info(i,&knob,part,1);
                m->amp_knob[part][i]=knob.name[0]?(knob.bar<0?0:(uint8_t)(knob.bar*100/255)):255u;
            }
        }
    }
    if(m->rack_lens || (cgu.turn_ms && cu.now-cgu.turn_ms<180u))m->emphasis[cgu.turn_knob]=1;
    for(unsigned i=0;i<4;i++){
        cf_knob_t knob={.bar=-1};
        cgu_knob_view(i,&knob,detail);
        if(knob.bar>=0)m->knob[i]=(uint8_t)(knob.bar*100/255);
        m->disabled[i]=knob.dis;
        str_cpy(m->label[i],knob.name,sizeof m->label[i]);
        str_cpy(m->value[i],knob.val,sizeof m->value[i]);
    }
}

static void cgu_grid(const cf_stage_model_t *m,unsigned part,int x,int y,int w,int detail)
{
    int steps=m->phrase_steps?m->phrase_steps:CGS_STEPS;
    int left=x+(detail?36:8), width=w-(detail?44:16);
    cf_rack_window(x,y,w,30,RAKT_GLASS);
    for(int bar=0;bar<2;bar++){
        int row=y+4+bar*12;
        if(detail)cfn_text(&CFN_11,x+5,row+8,bar?"Bar 2":"Bar 1",FL_INK2);
        for(int i=0;i<steps;i++){
            int start=left+i*width/steps, end=left+(i+1)*width/steps;
            int playing=m->beat==1+bar*CGS_STEPS+i;
            int on=(m->steps[part][bar]>>i)&1u;
            fb_rect(start,row,end-start-2,8,playing?FL_INK:on?CGU_INK_BASE+part:RAKT_LED_OFF);
        }
    }
}

static void cgu_help_draw(const cf_stage_model_t *m)
{
    static const char *const lines[4][3]={
        {"PLAY starts or stops the band.","SELECT switches the top tab.","ALGORITHM picks a musician."},
        {0,0,0},
        {"SEL zooms in or out.","SELECT turns tabs or zoomed pages.","GLO opens live controls."},
        {"REC captures the four parts.","PLAY replays a saved take.","USB audio captures the mix."}
    };
    if(m->focus==CGU_HARMONY){
        cf_learn_keyboard(&m->learn,16,208);
        cfn_fit(&CFN_14,26,183,m->learn.title,188,FL_INK);
        cfn_fit(&CFN_11,26,194,m->learn.help,188,FL_INK2);
    } else {
        for(unsigned i=0;i<3;i++)cfn_fit(&CFN_12,27,112+i*20,lines[m->focus][i],186,FL_INK2);
    }
}

static void cgu_metal_disc(int x,int y,int r,int top,int mid,int bottom)
{
    int span=r;
    for(int row=0;row<=r;row++){
        while(span*span+row*row>r*r)span--;
        fb_hline(x-span,y-row,2*span+1,CGU_METAL+mid+(top-mid)*row/r);
        if(row)fb_hline(x-span,y+row,2*span+1,CGU_METAL+mid+(bottom-mid)*row/r);
    }
}

static void cgu_dial_mark(int x,int y,int r,unsigned value,unsigned ink)
{
    static const int8_t xy[][2]={{-7,7},{-10,2},{-9,-5},{-5,-9},{0,-10},{5,-9},{9,-5},{10,2},{7,7},{7,7}};
    unsigned phase=value*8u, step=phase/100u, mix=phase%100u;
    int dx=xy[step][0]*(int)(100u-mix)+xy[step+1u][0]*(int)mix;
    int dy=xy[step][1]*(int)(100u-mix)+xy[step+1u][1]*(int)mix;
    if(r>8){
        for(unsigned i=0;i<9;i++)fb_pset(x+xy[i][0]*(r+3)/10,y+xy[i][1]*(r+3)/10,CGU_METAL+5);
    }
    fb_line(x+dx/350,y+dy/350,x+dx*(r-2)/1000,y+dy*(r-2)/1000,ink);
}

static void cgu_dial(int x,int y,int r,unsigned value,unsigned ink)
{
    cgu_metal_disc(x,y+1,r+1,10,10,10);
    cgu_metal_disc(x,y,r,7,4,1);
    cgu_metal_disc(x,y,r-2,4,2,1);
    cgu_dial_mark(x,y,r,value,ink);
}

static void cgu_pages(int x,int y,int w,unsigned tab,unsigned part,unsigned selected)
{
    static const char *const studio[]={"Part","Harmony","Evolve"};
    static const uint8_t pitched[]={18,28,20,34}, drums[]={25,0,25,50};
    const uint8_t *width=part==CGS_DRUMS?drums:pitched;
    unsigned accent=CGU_INK_BASE+part;
    unsigned count=tab==CGU_SOUND?CGU_SOUND_PAGES:NELEM(studio);
    for(unsigned i=0;i<count;i++){
        int cell=tab==CGU_SOUND?w*width[i]/100:w/(int)NELEM(studio);
        if(!cell)continue;
        const char *name=tab==CGU_SOUND?CGU_SOUND_NAMES[i]:studio[i];
        fb_rbox(x,y,cell-3,16,1,i==selected?RAKT_EDGE:RAKT_GLASS);
        int label=cfn_width(&CFN_11,name);
        cfn_fit(&CFN_11,x+clampi((cell-3-label)/2,2,cell-4),y+12,name,cell-7,
            i==selected?FL_INK:FL_INK2);
        if(i==selected)fb_hline(x+1,y+15,cell-5,accent);
        x+=cell;
    }
}

static void cgu_send_graph(unsigned effect,unsigned amount,const uint8_t shape[2],int x,int y,int w,int h,unsigned ink)
{
    fb_rect(x,y,w,h,FL_BG);
    int mid=y+h/2, base=y+h-2;
    fb_hline(x+2,effect<2?mid:base,w-4,FL_BG3);
    if(!amount)return;
    if(effect<2){
        int previous=mid;
        for(int i=0;i<w-4;i++){
            int v;
            if(effect==0){
                int in=(i*2-(w-5))*4096/(w-5);
                unsigned drive=amount*127u/100u;
                v=softclip(in*(4096+(int)drive*(int)drive*2)/4096);
            } else {
                uint32_t phase=(uint32_t)i*(15000000u+(uint32_t)(8u+shape[0])*1200000u);
                v=osc_sine(phase)*shape[1]*(int)amount/(100*100);
            }
            int now=mid-clampi(v*(h/2-2)/32768,2-h/2,h/2-2);
            if(i)fb_line(x+i+1,previous,x+i+2,now,ink);
            previous=now;
        }
        return;
    }
    int height=(h-3)*(int)amount, step=effect==2?5+shape[0]/6:3;
    int feedback=effect==2?shape[1]*246/100:150+shape[1];
    for(int i=3;i<w-2 && height>=100;i+=step){
        int bar=height/100;
        if(effect==3 && i&1)bar=bar*3/4;
        fb_rect(x+i,base-bar,2,bar,ink);
        height=height*feedback/256;
    }
}

static void cgu_amp_shell(int x,int y,int w,int h,int zoom,unsigned ink)
{
    cf_rack_plate(x,y,w,h,w>=RAKT_W?RAKT_EAR:8,FL_BG2);
    fb_hline(x+12,y+4,14,ink);
    int grille_top=zoom>72?43:23;
    cgu_grille(x+12,y+grille_top,w-24,zoom>72?12:h-grille_top-4);
}

static void cgu_lamp(int x,int y,int on,unsigned ink)
{
    cgu_metal_disc(x,y,3,6,3,0);
    cgu_metal_disc(x,y,2,0,0,0);
    if(on){
        fb_rect(x-1,y-1,3,3,ink);
        fb_pset(x-1,y-1,CGU_METAL+9);
    }
}

static unsigned cgu_module_ink(unsigned module)
{
    if(module<CGS_NPART)return CGU_INK_BASE+module;
    return module==CGS_NPART+CGU_BAND_PROGRESSION-1u?CGU_PROGRESSION_INK:CGU_BAND_INK_BASE+module-CGS_NPART;
}

static const char *cgu_module_value(const cf_stage_model_t *m,unsigned band,unsigned part,char summary[20])
{
    switch(band){
    case CGU_BAND_HARMONY:
        kpc(summary,cs.chord.tonic);
        cf_stage_append(summary,20," ");
        cf_stage_append(summary,20,CF_SCALES[cs.chord.scale].shrt);
        return summary;
    case CGU_BAND_SHAPING:
        str_cpy(summary,"Energy ",20);
        kint(summary+7,cgs.energy,"%");
        return summary;
    case CGU_BAND_GENRE:
        return CGS_PROFILES[m->genre].name;
    case CGU_BAND_ARP:
        return part==CGS_CHORDS || part==CGS_LAYER?CGU_ARP_NAMES[cgu_arp_mode()]:"Chords / Layer";
    default:
        if(m->workspace==CGU_STUDIO && (m->held_parts&(1u<<part)))return "Held";
        return m->workspace==CGU_SOUND?m->rail[part]:CGS_PROFILES[m->genre].style[part][cgs.style[part]];
    }
}

static void cgu_context_plate(int x,int y,int w,int h,unsigned ink)
{
    fb_rbox(x,y,w,h,3,CGU_METAL+4);
    fb_rbox(x+1,y+1,w-2,h-2,2,FL_BG);
    fb_hline(x+5,y+2,14,ink);
}

static void cgu_phrase_window(const cf_stage_model_t *m,unsigned part,int x,int y,int w,unsigned ink)
{
    cgu_context_plate(x,y,w,54,ink);
    const char *title="Phrase";
    if(part==CGS_LAYER && m->layer_reply){
        title=m->layer_reply==CGS_LAYER_REPLY_GAPS?"Chord gaps":
            m->layer_reply==CGS_LAYER_REPLY_SNARE?"Snare replies":"Chord-change motif";
    }
    else if(part==CGS_BASS && m->bass_reply)
        title=m->bass_reply==CGS_BASS_REPLY_LATE?"Behind the kick":"Kick accents";
    cfn_text(&CFN_11,x+8,y+16,title,FL_INK);
    unsigned steps=m->phrase_steps;
    if(!steps)return;
    for(unsigned bar=0;bar<2;bar++){
        int row=y+23+(int)bar*14;
        cfn_text(&CFN_11,x+8,row+7,bar?"2":"1",FL_INK2);
        for(unsigned i=0;i<steps;i++){
            int left=x+23+(w-31)*(int)i/(int)steps;
            int right=x+23+(w-31)*(int)(i+1)/(int)steps;
            int hit=(m->steps[part][bar]>>i)&1u;
            fb_rect(left,row,right-left-1,7,hit?ink:FL_BG3);
            if(part==CGS_BASS && (m->bass_delays[bar]&(1u<<i)))fb_vline(right-2,row+1,5,FL_INK);
            if(m->beat==1+bar*CGS_STEPS+i)fb_hline(left,row+9,right-left-1,FL_INK);
        }
    }
}

static void cgu_knob_caption(const cf_stage_model_t *m,unsigned k,int col,int y,int w,int show_value)
{
    int left=col-w/2+4, width=w-6;
    cfn_fit(&CFN_11,left,y+25,m->label[k],width,FL_INK2);
    if(show_value){
        const cfn_font_t *face=cfn_width(&CFN_12,m->value[k])<=width?&CFN_12:&CFN_11;
        cfn_fit(face,left,y+37,m->value[k],width,m->disabled[k]?FL_INK2:FL_INK);
    }
}

static void cgu_drum_hit_lights(const cf_stage_model_t *m,int col,int y,unsigned ink)
{
    for(unsigned lane=0;lane<SQ_NLANE;lane++){
        int left=col-17+6*(int)(lane%6), top=y-7+9*(int)(lane/6);
        unsigned lit=(m->drum_hits>>lane)&1u;
        fb_rbox(left,top,5,5,1,lit?ink:CGU_METAL+2);
    }
    cfn_center(&CFN_11,col,y+21,"Hits",FL_INK2);
}

static const char *cgu_drum_status(const cf_stage_model_t *m)
{
    static const char *const queued[]={"","Fill next bar","Drop next bar","Return next bar"};
    return m->moment?queued[m->moment]:m->build?"Drums dropped":
        m->bloom==CFPERF_FILL_AUTO?"Band turnaround":m->bloom?"Fill active":m->status;
}

static void cgu_output_meter(int x,int y,unsigned peak)
{
    static const uint8_t marks[]={0,3,7,13,22,35,52,76,112,168,255};
    cf_rack_window(x,y,71,23,RAKT_GLASS);
    for(unsigned i=0;i<NELEM(marks);i++){
        unsigned ink=i==10?RAKT_RED:i>=8?RAKT_AMBER:RAKT_GREEN;
        fb_rect(x+4+6*(int)i,y+7,4,9,peak && peak>=marks[i]?ink:RAKT_LED_OFF);
    }
}

static void cgu_display(const cf_stage_model_t *m)
{
    cf_rack_window(12,24,141,23,FL_BG);
    cfn_fit(&CFN_12,17,33,m->theory,134,FL_INK);
    cfn_fit(&CFN_12,17,45,m->degree,49,FL_INK);
    const char *context=m->workspace==CGU_SOUND?CGS_PROFILES[m->genre].name:
        m->workspace==CGU_STUDIO && m->live && !m->bank && m->focus==CGS_DRUMS?cgu_drum_status(m):m->context;
    if(context[0])cfn_fit(&CFN_11,53,45,context,99,FL_INK2);
    cgu_output_meter(156,24,m->signal_peak);
}

static void cgu_keys_footer(void)
{
    unsigned part=cgs.focus, ink=CGU_INK_BASE+part;
    fb_rect(151,228,79,12,RAKT_GLASS);
    fb_rect(151,229,2,10,ink);
    cfn_text(&CFN_11,156,237,"KEYS",FL_INK2);
    cfn_fit(&CFN_11,188,237,CGS_PART_NAME[part],39,ink);
}

static void cgu_mascot_sticker(int x,int y)
{
    const uint8_t *pixels=CFM_STICKERS[cf_skin%CF_SKIN_N].pixels;
    fb_rbox(x+1,y+2,36,36,5,CGU_METAL+1);
    fb_rbox(x,y,36,36,5,CGU_METAL+8);
    fb_rbox(x+1,y+1,34,34,4,CGU_METAL+9);
    for(int row=0;row<CFM_STICKER_SIZE;row++)for(int col=0;col<CFM_STICKER_SIZE;col++){
        unsigned ink=*pixels++;
        if(ink)fb_pset(x+4+col,y+4+row,(uint8_t)(CFM_PAL_BASE+ink-1));
    }
}

static void cgu_library_rack(const cf_stage_model_t *m,int x,int y,int w,int h,int zoom)
{
    cgu_amp_shell(x,y,w,h,zoom,C_ACC(0));
    cfn_fit(zoom>62?&CFN_16:&CFN_12,x+10,y+(zoom>62?24:18),"Sound Library",w-20,FL_INK);
    if(zoom<=62){
        cfn_text(&CFN_11,x+10,y+30,m->value[0],C_ACC(0));
        return;
    }
    int center=x+w/8, knob=y+h-47;
    cgu_dial(center,knob,zoom*15/100,m->knob[0],m->emphasis[0]?C_ACC(0):FL_INK);
    if(zoom<=82)return;
    cgu_knob_caption(m,0,center,knob,w/4,1);
    int left=x+w/4+4, top=knob-17, width=x+w-6-left;
    cgu_context_plate(left,top,width,54,C_ACC(0));
    for(unsigned i=0;i<CGS_NPART;i++){
        unsigned part=cgu_part(i);
        int line=top+14+(int)i*11;
        char count[20];
        kint(count,cgu_sound_count(part,m->build)," ");
        cf_stage_append(count,sizeof count,part==CGS_DRUMS?"kits":CGS_PART_NAME[part]);
        fb_rect(left+8,line-5,3,3,CGU_INK_BASE+part);
        cfn_fit(&CFN_11,left+15,line,count,width-63,FL_INK);
    }
    cgu_mascot_sticker(left+width-44,top+8);
}

static void cgu_mix_rack(const cf_stage_model_t *m,int x,int y,int w,int h)
{
    cf_rack_plate(x,y,w,h,RAKT_EAR,FL_BG2);
    cfn_text(&CFN_16,x+24,y+25,"Mix",FL_INK);
    cfn_fit(&CFN_11,x+24,y+42,"Chords / Bass / Drums / Layer",w-48,FL_INK2);
    int top=y+h-86;
    fb_hline(x+RAKT_EAR,top,w-2*RAKT_EAR,RAKT_COPPER);
    for(unsigned i=0;i<CGS_NPART;i++){
        unsigned part=cgu_part(i), bit=1u<<part;
        int cx=x+36+47*(int)i;
        unsigned level=m->part_level[part];
        uint8_t ink=CGU_INK_BASE+part;
        if(i)fb_vline(cx-24,top+6,70,RAKT_SHADE);
        cfn_center(&CFN_11,cx,top+19,CGS_PART_NAME[part],ink);
        cf_rack_channel(cx,top+25,(m->pin_mask&bit)?0:(level*6+99)/100,top+51-(int)level*24/100);
        cfn_center(&CFN_12,cx,top+77,m->value[i],m->emphasis[i]?ink:FL_INK);
    }
}

static void cgu_record_controls(const cf_stage_model_t *m,int x,int y,int w,int h,int zoom)
{
    cf_rack_plate(x,y,w,h,RAKT_EAR,FL_BG3);
    cfn_text(&CFN_16,x+17,y+23,"Saved Takes",FL_INK);
    if(zoom>82){
        int knob=y+h-47, center=x+w/8, cards=w*3/4;
        cfn_text(&CFN_11,x+10,y+40,"Hold SAVE 3s to clear",FL_INK2);
        cgu_dial(center,knob,zoom*15/100,m->knob[0],m->emphasis[0]?C_ACC(0):FL_INK);
        cgu_knob_caption(m,0,center,knob,w/4,0);
        for(unsigned slot=0;slot<SQ_NPAT;slot++){
            int left=x+w/4+cards*(int)slot/SQ_NPAT+3, width=cards/SQ_NPAT-6;
            unsigned bit=1u<<slot, ink=slot==m->slot?C_ACC(0):CGU_METAL+4;
            cgu_context_plate(left,knob-17,width,54,ink);
            char number[8];
            kint(number,slot+1u,0);
            cfn_center(&CFN_16,left+width/2,knob+8,number,slot==m->slot?ink:FL_INK);
            cfn_center(&CFN_11,left+width/2,knob+24,
                m->total&bit?"Unread":m->count&bit?"Saved":"Empty",FL_INK2);
            if(slot==m->slot && m->pulse)fb_hline(left+4,knob+32,(width-8)*m->pulse/100,ink);
        }
        return;
    }
    int panel_y=zoom>62?y+61:y+32;
    int slot_w=(w-20)/SQ_NPAT;
    for(unsigned slot=0;slot<SQ_NPAT;slot++){
        unsigned mask=1u<<slot;
        int left=x+16+(int)slot*slot_w;
        char text[14];
        kint(text,slot+1u,(m->total&mask)?" Unread":(m->count&mask)?" Saved":" Empty");
        cf_rack_window(left,panel_y,slot_w-3,19,slot==m->slot?FL_BG1:RAKT_GLASS);
        cfn_fit(&CFN_11,left+5,panel_y+13,text,slot_w-11,slot==m->slot?FL_INK:FL_INK2);
        if(slot==m->slot)fb_hline(left+2,panel_y+18,slot_w-7,C_ACC(0));
    }
    if(m->pulse)fb_hline(x+16+(int)m->slot*slot_w,panel_y+21,(slot_w-3)*m->pulse/100,CGU_INK_BASE+CGS_DRUMS);
}

typedef struct { int x,y,w,h; } cgu_face_t;
#include "cf_rack_units.c"
enum { CGU_FACE_CONTROLS_H=24, CGU_RACK_CONTROLS_Y=191 };

static cgu_face_t cgu_rack_face(unsigned tab,unsigned tile)
{
    return (cgu_face_t){RAKT_X,CGU_RACK_Y+cgu_rack_offset(tab,tile),RAKT_W,cgu_rack_height(tab,tile)};
}

static void cgu_rack_unit(const cf_stage_model_t *m,unsigned tile,cgu_face_t face,int zoom,int selected)
{
    int x=face.x,y=face.y,w=face.w,h=face.h;
    unsigned tab=m->workspace;
    if(tab<=CGU_SOUND && tile<CGS_NPART){
        cgu_amp_head(m,cgu_part(tile),face,zoom,selected);
    } else if(tab==CGU_STUDIO){
        cgu_band_unit(m,tile-CGS_NPART+1u,face,zoom,selected);
    } else if(tab==CGU_RECORD){
        if(tile==CGS_NPART)cgu_record_controls(m,x,y,w,h,zoom);
        else {
            unsigned part=cgu_part(tile), ink=CGU_INK_BASE+part;
            cf_rack_plate(x,y,w,h,RAKT_EAR,(uint8_t)(RAKT_CHORDS+part));
            cfn_text(&CFN_16,x+17,y+21,CGS_PART_NAME[part],FL_INK);
            cgu_lamp(x+w-20,y+16,(m->activity&(1u<<part))!=0,ink);
            if(zoom>62){
                cfn_text(&CFN_12,x+17,y+43,m->rec?"Manual take":"Generated phrase",FL_INK2);
                cfn_text(&CFN_11,x+17,y+106,"REC starts or finishes a take",FL_INK2);
            }
            cgu_grid(m,part,x+16,y+cgu_unit_blend(25,56,zoom),w-32,zoom>62);
        }
    } else if(tab==CGU_HELP){
        static const char *const detail[]={"Transport and workspace controls","Notes, degrees and chord choices",
            "Rack pages and live controls","Recordings and USB audio"};
        cf_rack_plate(x,y,w,h,RAKT_EAR,FL_BG2);
        cfn_text(&CFN_16,x+17,y+24,CGU_TOPICS[tile],FL_INK);
        if(zoom<=82){
            cf_rack_window(x+16,y+30,w-32,19,RAKT_GLASS);
            cfn_fit(&CFN_11,x+22,y+43,detail[tile],w-44,FL_INK2);
        }
    } else if(tile==CGS_NPART+1u && zoom>62){
        cgu_mix_rack(m,x,y,w,h);
    } else if(tile==CGS_NPART && zoom>62){
        cgu_library_rack(m,x,y,w,h,zoom);
    } else {
        int mix=tile==CGS_NPART+1u;
        cf_rack_plate(x,y,w,h,RAKT_EAR,FL_BG2);
        cfn_text(&CFN_16,x+17,y+24,mix?"Mix":"Sound Library",FL_INK);
        cf_rack_window(x+16,y+30,w-32,19,RAKT_GLASS);
        cfn_text(&CFN_11,x+22,y+43,mix?"Part levels":m->build?"All sounds":"Genre",FL_INK2);
    }
}

static void cgu_face_controls(const cf_stage_model_t *m,unsigned selected)
{
    static const uint8_t part[]={66,57,37,52}, tone[]={78,43,49,42},
        harmony[]={36,60,63,53}, shaping[]={46,60,51,55}, arp[]={59,54,50,49}, genre[]={116,96,0,0};
    static const uint8_t mix[]={53,53,53,53};
    const uint8_t *width=part;
    if(m->workspace==CGU_SOUND)width=m->bank==CGU_RACK_MIX?mix:tone;
    if(m->workspace==CGU_STUDIO){
        if(m->bank==CGU_BAND_HARMONY)width=harmony;
        if(m->bank==CGU_BAND_SHAPING)width=shaping;
        if(m->bank==CGU_BAND_ARP)width=arp;
        if(m->bank==CGU_BAND_GENRE)width=genre;
    }
    unsigned ink=m->workspace==CGU_STUDIO && selected>=CGS_NPART?cgu_module_ink(selected):
        selected<CGS_NPART && m->workspace!=CGU_HELP?CGU_INK_BASE+cgu_part(selected):C_ACC(0);
    int y=CGU_RACK_CONTROLS_Y;
    cf_rack_plate(7,y+1,226,CGU_FACE_CONTROLS_H-1,7,FL_BG2);
    fb_rect(10,y+4,2,CGU_FACE_CONTROLS_H-7,ink);
    int left=14;
    int single=!m->label[1][0];
    const cfn_font_t *font=m->workspace==CGU_STUDIO && m->bank?&CFN_12:&CFN_11;
    for(unsigned k=0;k<4;k++){
        int w=single?212:width[k];
        if(m->label[k][0]){
            unsigned active=m->emphasis[k] && !m->disabled[k];
            const char *value=k==0 && m->workspace==CGU_STUDIO && m->bank==CGU_BAND_GENRE?
                CGS_PROFILES[m->genre].name:m->value[k];
            if(active)fb_rbox(left,y+2,w-2,20,2,CGU_METAL);
            char label[12];
            const char *name=cgu_unit_label(m->label[k],label);
            cfn_fit(font,left+3,y+10,name,w-6,active?ink:FL_INK2);
            cfn_fit(font,left+3,y+21,value,w-6,m->disabled[k]?FL_INK2:active?ink:FL_INK);
        }
        if(single)break;
        left+=w;
    }
}

static void cgu_cabinet(int bottom)
{
    cf_rack_cabinet(10,50,220,bottom-48,53,bottom-53);
    for(int x=RAKT_X;x<226;x+=205){
        fb_rect(x,53,7,bottom-53,RAKT_RAIL);
        for(int y=58;y+4<bottom;y+=11)fb_rbox(x+2,y,4,4,1,FL_BLACK);
    }
}

static void cgu_unit_bay(const cf_stage_model_t *m)
{
    unsigned tab=m->workspace, selected=cgu_workspace_tile(m);
    int zoom=m->zoom;
    cgu_face_t from=cgu_rack_face(tab,selected);
    from.y-=m->rack_scroll;
    int bottom=185+zoom*10/100;
    cgu_cabinet(bottom);
    unsigned first=cgu_rack_first(tab,selected);
    int settled=m->rack_scroll==cgu_rack_offset(tab,first);
    int count=(int)cgu_rack_tiles(tab);
    int gap=(bottom-61)/count;
    for(int i=0;i<count;i++){
        int top=cgu_rack_offset(tab,(unsigned)i);
        int visible=settled?(unsigned)i>=first && (unsigned)i<first+2:
            top+cgu_rack_height(tab,(unsigned)i)>m->rack_scroll &&
            top<m->rack_scroll+bottom-CGU_RACK_Y;
        fb_rect(234,57+i*gap,3,gap-3,(unsigned)i==selected?C_ACC(0):visible?RAKT_EDGE:RAKT_RAIL);
    }
    fb_clip(RAKT_X,53,RAKT_X+RAKT_W,bottom);
    if(zoom<100){
        for(unsigned tile=0;tile<cgu_rack_tiles(tab);tile++){
            if(tile==selected || (settled && (tile<first || tile>=first+2)))continue;
            cgu_face_t unit=cgu_rack_face(tab,tile);
            unit.y-=m->rack_scroll;
            unit.x+=((tile-selected)&1u)?-m->bank_dx:m->bank_dx;
            if(unit.y<185 && unit.y+unit.h>53)cgu_rack_unit(m,tile,unit,0,0);
        }
    }
    cgu_face_t face={from.x+m->bank_dx,cgu_unit_blend(from.y,53,zoom),from.w,cgu_unit_blend(from.h,142,zoom)};
    cgu_rack_unit(m,selected,face,zoom,1);
    unsigned ink=tab==CGU_STUDIO && selected>=CGS_NPART?cgu_module_ink(selected):
        tab!=CGU_HELP && selected<CGS_NPART?CGU_INK_BASE+cgu_part(selected):C_ACC(0);

    if(zoom<=38){
        fb_rframe(face.x,face.y,face.w,face.h,ink);
        fb_clip(0,53,240,bottom);
        int arrow=clampi(face.y+face.h/2,58,bottom-5);
        if(!m->bank_dx)for(int column=0;column<6;column++)fb_vline(4+column,arrow-5+column,11-2*column,ink);
    }
    fb_clip(0,51,240,218);
    if(zoom<=38 && m->label[0][0])cgu_face_controls(m,selected);
}

static void cgu_workspace_draw(const cf_stage_model_t *m)
{
    unsigned tab=m->workspace;
    int studio=tab==CGU_STUDIO;
    cf_rack_plate(1,1,238,22,7,FL_BG1);
    cf_rack_plate(1,219,238,21,7,FL_BG1);
    for(unsigned i=0;i<CGU_NTAB;i++){
        int x=10+(int)i*55;
        if(i==tab)fb_rbox(x,3,53,15,1,RAKT_GLASS);
        cfn_center(&CFN_12,x+26,15,CGU_TABS[i],i==tab?FL_INK:FL_INK2);
        if(i==tab)fb_hline(x,18,53,C_ACC(0));
    }
    cgu_display(m);
    fb_clip(0,51,240,218);
    cgu_unit_bay(m);
    if(tab==CGU_HELP && m->zoom>82 && !m->bank_dx){
        fb_clip(0,51,240,197);
        cgu_help_draw(m);
    }
    fb_unclip();
    int overview=m->zoom<=38;
    int fx_presets=tab==CGU_SOUND && m->live && !m->bank && m->sound_page==CGU_PRESETS;
    int palette=tab==CGU_SOUND && m->live && m->bank==CGU_RACK_LIBRARY && !m->build;
    if(studio && m->bank){
        int progression=m->live && m->bank==CGU_BAND_PROGRESSION;
        int footer=overview?226:208;
        cfn_text(&CFN_12,10,footer,overview?"ALGO Browse":progression?"EDIT Draft":"SEQ Next",FL_INK);
        cfn_text(&CFN_12,100,footer,progression?"SEL Apply":m->live?"SEL Back":"SEL Zoom",FL_INK);
        cfn_text(&CFN_11,167,footer,!overview && m->bank==CGU_BAND_ARP?"SELECT Page":"SELECT Tab",FL_INK2);
        if(m->pending[0])cfn_fit(&CFN_11,10,237,m->pending,137,FL_INK2);
        else if(progression)cfn_text(&CFN_11,10,237,"SEQ Next unit",FL_INK2);
        else if(!overview && m->bank==CGU_BAND_ARP)cfn_text(&CFN_11,10,237,"ALGO Keys part",FL_INK2);
        cgu_keys_footer();
        return;
    }
    int mix=tab==CGU_SOUND && m->bank==CGU_RACK_MIX;
    int apply=fx_presets || palette;
    if(!overview)fb_hline(10,197,220,RAKT_EDGE);
    const char *hint=m->pending[0]?m->pending:
        m->section&CGS_FORM_FILL?"Band turnaround":m->section&CGS_FORM_LAYER_REST?"Band breather":
        studio?(m->live && !m->bank?"EDIT New part":"EDIT Shuffle take"):
        tab==CGU_SOUND?(!m->live?"EDIT Sound + FX":mix?"K1-4 Levels":m->bank || m->sound_page==CGU_TONE?"EDIT Sound + FX":""):
        tab==CGU_RECORD?(m->rec?"REC Finish take":"REC Record band"):"";
    if(fx_presets)hint="EDIT FX controls";
    int footer=overview?227:207;
    cfn_fit(&CFN_11,10,footer,hint,101,FL_INK2);
    cfn_text(&CFN_11,114,footer,apply?"SEL Apply":m->live?"SEL Back":"SEL Zoom",apply?FL_INK:FL_INK2);
    cfn_text(&CFN_11,167,footer,m->live && cgu_has_detail_pages()?"SELECT Page":"SELECT Tab",FL_INK2);
    if(overview){
        cfn_text(&CFN_11,10,238,"ALGO Browse",FL_INK);
        if(studio)cfn_fit(&CFN_11,82,238,"SEQ Units",66,FL_INK2);
    } else if(tab==CGU_SOUND && !m->bank){
        const char *sound_hint=m->sound_page==CGU_PRESETS?"K1 Browse   SAVE Manage":"FX Presets";
        cfn_text(&CFN_11,10,221,sound_hint,FL_INK2);
        if(m->focus!=CGS_DRUMS)cfn_text(&CFN_11,158,221,"LFO Voice",FL_INK2);
        char page[12];
        kint(page,1u+cgu_sound_page_rank(m->focus,m->sound_page),"/");
        kint(page+str_len(page),cgu_sound_page_count(m->focus),0);
        cfn_text(&CFN_11,10,237,"SELECT Pages",FL_INK);
        cfn_text(&CFN_11,91,237,page,CGU_INK_BASE+m->focus);
    } else if(tab==CGU_SOUND && m->bank==CGU_RACK_LIBRARY){
        cfn_text(&CFN_11,10,221,"K1 Library   HOME Back",FL_INK2);
    } else if(studio && m->live){
        if(!m->bank){
            char page[12];
            kint(page,1u+m->shaping,"/");
            kint(page+str_len(page),CGU_PART_PAGES,0);
            cfn_text(&CFN_11,10,221,"SELECT Pages",FL_INK);
            cfn_text(&CFN_11,91,221,page,CGU_INK_BASE+m->focus);
        }
        if(!m->bank && m->focus==CGS_DRUMS)cfn_fit(&CFN_11,10,237,"PRESETS Fill / Drop / Return",139,FL_INK2);
    } else if(tab==CGU_HELP && m->live && m->focus==CGU_HARMONY){
        cfn_text(&CFN_11,10,222,m->value[0],FL_INK);
        cfn_text(&CFN_11,100,222,"EDIT Chord Lab",FL_INK2);
    }
    cgu_keys_footer();
}
