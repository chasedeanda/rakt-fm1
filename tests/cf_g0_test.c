/* SPDX-License-Identifier: GPL-3.0-only; G0 real renderer/MIDI acceptance. */
#define CF_ENGINE_HELPERS_ONLY
#include "cf_engine_test.c"

static void midi_enable(void)
{usb.up=usb.config=1;cfp_cfg.midi_out=1;cfp_cfg.ch_chord=0;cfp_cfg.ch_bass=1;mo_r=mo_w;}
static unsigned note_ons(unsigned channel)
{
    unsigned count=0;
    while(mo_r!=mo_w){uint32_t q=midi_out_q[mo_r++%MQ];unsigned s=q>>8&255;
        count+=((s&0xf0)==0x90 && (s&15)==channel && (q>>24&127));}
    return count;
}
static void arp_controls(void)
{
    /* Ownership stays independent of the live-key switch, in every stage. */
    for(unsigned stage=0;stage<4;stage++){
        start();cge.feel=0;cge.arp_request=CGE_ARP_ON;
        cs.arp.mode=CF_ARP_DOWN;cs.arp.rate=4;cs.arp.oct=2;cs.arp.gate=45;cs.arp.swing=63;
        cf_send_config();blocks(1);ticks(stage*4*384+768);midi_enable();
        CHECK(cge.arp && !cu.arp_on && !cfa_k.on);
        cf_arp_t original=cfp_cfg.arp;int pushed=cge_arp_push();
        CHECK(cfp_cfg.arp.rate==4 && cfp_cfg.arp.oct==2 && cfp_cfg.arp.gate==45 && cfp_cfg.arp.swing==63 && cfp_cfg.arp.mode==CF_ARP_DOWN);
        cge_arp_pop(pushed);CHECK(!memcmp(&original,&cfp_cfg.arp,sizeof original));
        blocks(FS/CTL);CHECK(note_ons(0)>=2);
        cge.arp_request=CGE_ARP_OFF;CHECK(cge_arp_pending());ticks(768);
        CHECK(!cge.arp && sqp.n && !cfa_sq[0].n);mo_r=mo_w;blocks(FS/CTL);CHECK(!note_ons(0));
        cge.feel=100;cge.arp_request=CGE_ARP_AUTO;ticks(768);CHECK(cge.arp);
        cge.feel=0;ticks(768);CHECK(!cge.arp && sqp.n);
    }
    /* UI does not silently repurpose the live-key state. */
    start();cu.scr=SCR_ARP;cu.page[SCR_ARP]=0;unsigned live=cu.arp_on;
    ui_knob_turn(0,1);CHECK(cge.arp_request==CGE_ARP_ON && cu.arp_on==live);
    ui_knob_turn(0,1);CHECK(cge.arp_request==CGE_ARP_OFF);
    ui_knob_turn(0,-1);ui_knob_turn(0,-1);CHECK(cge.arp_request==CGE_ARP_AUTO);
    cu.page[SCR_ARP]=2;ui_knob_turn(0,1);blocks(2);CHECK(cu.arp_on && cfa_k.on && cge.arp_request==CGE_ARP_AUTO);
    ui_knob_turn(0,-1);blocks(2);CHECK(!cu.arp_on && !cfa_k.on);
    cpa_stop();cpg.idea=0;ui_knob_turn(0,1);CHECK(cu.arp_on);
    ui_knob_turn(0,-1);CHECK(!cu.arp_on);
}
static void control_boundaries(void)
{
    start();cge.feel=0;cpa_realtime(0xFA);ticks(123);uint32_t t=cpa.elapsed;
    cge_knob_turn(1,100);CHECK(cge.feel_applied==0 && cge_arp_pending());
    ticks(768-t-1);CHECK(!cge.arp);ticks(1);CHECK(cge.arp && cge.feel_applied==100);
    cge.arp_request=CGE_ARP_OFF;ticks(768);CHECK(!cge.arp && cge_gated());
    ticks(134);CHECK(!sqp.n);blocks(2);CHECK(!sqp.n); /* service must not refill a gate gap */
    ticks(192-134);CHECK(sqp.n);
    /* No early Energy saturation, fixed ramp length, no player-mix edits. */
    unsigned mix[]={cs.level_chord,cs.bass_level,cs.level_drums};
    for(unsigned stage=0;stage<4;stage++){
        ticks(4*384);uint16_t previous[3]={0};
        for(unsigned energy=0;energy<=100;energy+=10){
            cge_knob_turn(0,(int)energy-cge.energy);blocks((FS/20+CTL-1)/CTL+2);
            for(unsigned k=0;k<3;k++){CHECK(cge.gain[k]==cge.target[k]);CHECK(cge.gain[k]>=previous[k]);previous[k]=cge.gain[k];}
        }
    }
    CHECK(cs.level_chord==mix[0] && cs.bass_level==mix[1] && cs.level_drums==mix[2]);
    cf_fx_t baseline=cs.fx;uint8_t locks=cge.locks;
    cge_knob_turn(2,-100);blocks(FS/4/CTL);CHECK(!cfx_space);
    CHECK(cfx.wg[CFX_REVERB]==0 && cfx.wg[CFX_DELAY]==0 && cfx.wg[CFX_CHORUS]==0);
    CHECK(!memcmp(&baseline,&cs.fx,sizeof baseline) && cge.locks==locks);
    cge_knob_turn(2,100);blocks(FS/4/CTL);CHECK(cfx_space==32768);
    CHECK(!memcmp(&baseline,&cs.fx,sizeof baseline));
    cpa_stop();cs.chord.tonic=7;cs.chord.scale=CF_SC_DORIAN;cs.chord.octave=-1;cs.bpm=137;cs.bass_on=0;
    cf_chord_cfg_t cfg=cs.chord;cge_load();CHECK(cs.bpm==137 && !cs.bass_on && !memcmp(&cfg,&cs.chord,sizeof cfg));
    cs.beats=6;cge_load();cpa_plan_t p;CHECK(cs.beats==6 && !cge_prepare(&p,cs.chord,cpg.rhythm,CPA_BOTH));cs.beats=4;
}
static void pending_takes(void)
{
    start();cge_take_t first=cge.current;ticks(6*384);cge_regenerate(0);cge_regenerate(0);cge_take_t latest=cge.next;
    CHECK(cge.pending && !cge.undo);ticks(9*384);CHECK(cge.pending && cge.current.id==first.id);
    ticks(384);CHECK(!cge.pending && !memcmp(&cge.current,&latest,sizeof latest) && cge.undo);
    cge_regenerate(1);ticks(16*384);CHECK(!memcmp(&cge.current,&first,sizeof first));
    cge_regenerate(0);cs.fx.v[CFX_DELAY][2]^=1;cf_fx_apply(&cs.fx);cge_poll();cf_fx_t fx=cs.fx;
    CHECK(cge.locks&CGE_FX);ticks(16*384);CHECK(!memcmp(&fx,&cs.fx,sizeof fx));
    cge.locks=0;cge_regenerate(0);cu.scr=SCR_HOME;cf_home_bank=CF_HB_TEXTURE;
    ui_knob_turn(0,1);fx=cs.fx;CHECK((cge.locks&CGE_FX) && !(cge.pending&CGE_FX));
    ticks(16*384);CHECK(!memcmp(&fx,&cs.fx,sizeof fx));
    cf_perf_queue(CFPERF_DROP);ticks(384);CHECK(cf_moment.dropped);
    ticks(32*384);CHECK(cf_moment.dropped);cf_perf_queue(CFPERF_BACK);ticks(384);CHECK(!cf_moment.dropped);
}
static void screen(const char *name)
{
    cf_snapshot();cu.pop=0;cf_intro_on=0;cf_motion=0;fb_full=1;cf_ui_draw();
    char path[128];snprintf(path,sizeof path,"build/g0/%s.ppm",name);FILE *f=fopen(path,"wb");CHECK(f);if(!f)return;
    fprintf(f,"P6\n240 240\n255\n");for(unsigned i=0;i<FB_W*FB_H;i++){uint32_t rgb=cf_pal[fb[i]];uint8_t v[]={rgb>>16,rgb>>8,rgb};fwrite(v,1,3,f);}fclose(f);
}
static void ui_images(void)
{
    start();ticks(6*384);cu.scr=SCR_HOME;cf_home_bank=CF_HB_ENGINE;cge_knob_turn(1,58);screen("home-feel-queued");
    cge_regenerate(0);screen("home-take-queued");cu.scr=SCR_ARP;cu.page[SCR_ARP]=0;cge.arp_request=CGE_ARP_ON;screen("arp-queued");
    ticks(768);screen("arp-on");cu.page[SCR_ARP]=2;screen("arp-live-keys");
}

/* One fresh process per capture: deterministic DSP initialization and same
 * harmony/groove/FX/transport. Only the named control or exact patch differs. */
static void capture(const char *kind,unsigned value,const char *raw,const char *events)
{
    start();cpa_stop();blocks(30*FS/CTL);
    cge.feel=0;cge.space=100;
    if(!strcmp(kind,"patch") || !strcmp(kind,"patchdry")){
        if(!strcmp(kind,"patchdry"))cge.space=0;
        CHECK(value<2);unsigned e=cge_sound_engine[0][0][value],f=cge_sound_factory[0][0][value];
        cf_pool_load(0,e,pool_pos_factory(e,f));cs.snd[0]=cf_snd_pos(0);
        blocks(2); /* settle the stopped patch load before the captured onset */
    }else if(!strcmp(kind,"energy")){cge.energy=value;cge_gain_prepare();}
    else if(!strcmp(kind,"space"))cge.space=value;
    else if(!strcmp(kind,"feel"))cge.feel=value;
    else if(!strcmp(kind,"arp")){
        cge.arp_request=CGE_ARP_ON;cs.arp.mode=CF_ARP_UP;cs.arp.rate=value;
        cs.arp.gate=70;cs.arp.oct=1;cs.arp.swing=50;cf_send_config();blocks(1);
    }else if(!strcmp(kind,"manual")){
        cge.arp_request=CGE_ARP_ON;cs.arp.mode=value&1?CF_ARP_DOWN:CF_ARP_UP;
        cs.arp.rate=4;cs.arp.gate=value&2?30:80;cs.arp.oct=value&4?2:1;cs.arp.swing=value&8?65:50;
        cf_send_config();blocks(1);
    }
    FILE *r=fopen(raw,"wb"),*f=fopen(events,"w");CHECK(r && f);if(!r || !f)return;
    fprintf(f,"sample,cycle,bar,tick,stage,status,note,velocity,feel,feel_applied,play_it,play_it_applied,space,chord_gain,bass_gain,drum_gain,planned_notes\n");
    cpa_ui_play();midi_enable();cpa_realtime(0xFA);
    int32_t out[CTL*2];unsigned sample=0,held[2][128]={{0}},overlap=0,ons[16]={0},last=999;
    while(cge.cycle<4){
        unsigned position=cge.cycle*16+cge.bar;
        /* Interactive trace explicitly exercises requests at non-boundaries. */
        if(!strcmp(kind,"live") && position!=last){
            last=position;
            if(position==1){cge.feel=100;cge.arp_request=CGE_ARP_ON;}
            if(position==6)cge.arp_request=CGE_ARP_OFF;
            if(position==10)cge.arp_request=CGE_ARP_AUTO;
            if(position==18){cge.feel=35;cge_knob_turn(0,20);cge_knob_turn(2,-75);}
            if(position==22)cge_regenerate(0);
            if(position==33)cf_perf_queue(CFPERF_DROP);
            if(position==36)cge_regenerate(1);
            if(position==44)cf_perf_queue(CFPERF_BACK);
        }
        while(mo_r!=mo_w){
            uint32_t q=midi_out_q[mo_r++%MQ];unsigned st=q>>8&255,n=q>>16&127,v=q>>24&127,ch=st&15;
            if((st&0xf0)==0x90 || (st&0xf0)==0x80){
                char planned[48]={0};const cpa_slot_t *p=&cpa.plan.slot[cpa.slot];for(unsigned i=0;i<p->n;i++){char text[8];snprintf(text,sizeof text,"%s%u",i?" ":"",p->note[i]);strcat(planned,text);}
                fprintf(f,"%u,%u,%u,%u,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%s\n",sample,cge.cycle,cge.bar+1,cpa.t,cge_stage_name(),st,n,v,cge.feel,cge.feel_applied,cge.arp_request,cge.arp_applied,cge.space,cge.gain[0],cge.gain[1],cge.gain[2],planned);
                if(ch<2){if((st&0xf0)==0x90 && v){overlap+=held[ch][n];held[ch][n]=1;if(!ch)ons[cge.bar]++;}else held[ch][n]=0;}
            }
        }
        audio_block(out,CTL);fwrite(out,sizeof out,1,r);sample+=CTL;ua.cr=ua.cw;
        if(sample%(8*CTL)==0){cf_snapshot();cpa_ui_poll();cge_poll();}
    }
    CHECK(!overlap);CHECK(sample>0);cpa_stop();
    while(mo_r!=mo_w){uint32_t q=midi_out_q[mo_r++%MQ];unsigned st=q>>8&255,n=q>>16&127;
        if((st&15)<2 && (st&0xf0)==0x80)held[st&15][n]=0;}
    unsigned stale=0;for(unsigned ch=0;ch<2;ch++)for(unsigned n=0;n<128;n++)stale+=held[ch][n];CHECK(!stale);
    if(!strcmp(kind,"arp") || !strcmp(kind,"manual"))for(unsigned b=0;b<16;b++)CHECK(ons[b]>0);
    fclose(r);fclose(f);printf("G0_CAPTURE,%s,%u,samples=%u,overlap=%u,stale=%u\n",kind,value,sample,overlap,stale);
}
int main(int argc,char **argv)
{
    emu_fw_options(NULL,1,0,1);emu_hal.ready=2;emu_fw_init(0);
    if(argc==6 && !strcmp(argv[1],"--capture"))capture(argv[2],atoi(argv[3]),argv[4],argv[5]);
    else {arp_controls();control_boundaries();pending_takes();ui_images();}
    printf("RAKT G0: %u checks, %u failures\n",checks,failures);return failures!=0;
}
