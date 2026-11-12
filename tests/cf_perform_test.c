/* SPDX-License-Identifier: GPL-3.0-only
 * Actual native players, MIDI, UI snapshots and DSP. No device access. */
#include <stdio.h>
#include <math.h>
#include "usb_mock.h"
#define FELUCCA_UAC 1
#define FELUCCA_CDC 1
/* Exercise classic HOME bank navigation. Native Studio routes have their own suite. */
#define CF_STUDIO 0
#include "../tools/emu/emu_fw.c"
static void ua_service_paced(uint32_t t0, int isr) { (void)t0; (void)isr; ua_service(); }
static unsigned checks, failures;
#define CHECK(x) do { checks++; if (!(x)) { failures++; printf("FAIL %d: %s\n", __LINE__, #x); } } while (0)

static void clean(void)
{
    cpa_stop(); dmp_stop(); sqp_stop(); cfp_panic(); cf_perf_reset();
    cf_evq_r=cf_evq_w; mo_r=mo_w; cfx_in.active=0;
    for (unsigned i=0;i<NSND;i++) { memset(trk[i].v,0,sizeof trk[i].v); trk[i].tail=trk[i].nmono=trk[i].xp_n=0; }
}
static void action(unsigned a)
{
    cf_ev_in_t e={.op=CFE_PERFORM,.a=a}; cf_apply(&e);
}
static unsigned notes(void)
{
    unsigned hits=0;
    while (mo_r!=mo_w) {
        uint32_t e=midi_out_q[mo_r++%MQ];
        if (((e>>8)&255)==(0x90u|cfp_ch(CF_PART_DRUM)) && (e>>24))
            for (unsigned l=0;l<SQ_NLANE;l++) if (((e>>16)&127)==SQ_LANE_NOTE[l]) hits|=1u<<l;
    }
    return hits;
}
static void tick(int seq) { if(seq)sqp_tick1();else dmp_tick1(); }
static unsigned pos(int seq) { return seq?sqp.t:dmp.t; }
static void boundary(int seq)
{
    unsigned bt=sqp_bar_ticks();
    do { tick(seq); } while (pos(seq));
    CHECK(bt==sqp_bar_ticks());
}
static unsigned score(unsigned r,unsigned bar,unsigned t)
{
    const dm_rhythm_t *y=&DM_RHY[r];
    unsigned sd=dm_steps(y->beats), bt=y->beats*SQ_PPQN, hits=0;
    for(unsigned h=0;h<sd;h++) if(t==h*bt/sd+(h%2?y->swing*(bt/sd)/200:0))
        for(unsigned l=0;l<SQ_NLANE;l++) if((dm_mask(r,l,bar)>>h)&1u)hits|=1u<<l;
    return hits;
}
static void start(unsigned r,int seq)
{
    clean(); const dm_rhythm_t *y=&DM_RHY[r];
    cs.beats=cfm.beats=sq_beats=y->beats; cfm.bpm=y->bpm;
    if(seq){sq_fresh(&sq_st.pat[0],y->beats);dm_fill(&sq_st.pat[0],r,0);sqp_start(0,0);}
    else dmp_start(r,y->kit,y->swing);
    notes();
}
static void all_grooves(void)
{
    dm_defaults(); usb.config=1;cfp_cfg.midi_out=1;cfp_cfg.ch_drums=9;cfp_cfg.clock_out=0;
    for(unsigned r=0;r<DM_N;r++)for(int seq=0;seq<2;seq++) {
        start(r,seq); unsigned bt=sqp_bar_ticks();
        sq_pat_t before=sq_st.pat[0];
        for(unsigned t=1;t<bt/3;t++){tick(seq);CHECK(notes()==score(r,0,t));}
        action(CFPERF_FILL); CHECK(cf_moment.pending==CFPERF_FILL&&!cf_moment.fill);
        unsigned phase=pos(seq); action(CFPERF_FILL); CHECK(pos(seq)==phase);
        while(pos(seq)!=bt-1){tick(seq);CHECK(notes()==score(r,0,pos(seq)));}
        tick(seq); CHECK(cf_moment.fill&&!cf_moment.pending&&pos(seq)==0);notes();
        unsigned style=seq?cf_perf_kit_style(DM_RHY[r].kit):dmp_fill_style(r);
        CHECK(cf_moment.style==style);
        for(unsigned t=1;t<bt;t++) {
            tick(seq); unsigned actual=notes();
            if(t<bt/2)CHECK(actual==score(r,1,t));
            /* Known 4/4 turnaround: loose low tom on last sixteenth;
             * tight hat + snare roll. Independent of the helper under test. */
            if(DM_RHY[r].beats==4 && t==15*24+DM_RHY[r].swing*24/200)
                CHECK(actual==(style==CFPERF_TIGHT?((1u<<DM_H)|(1u<<DM_S)):(1u<<DM_LT)));
        }
        action(CFPERF_FILL);CHECK(!cf_moment.pending); /* active fill cannot be extended */
        cf_now_ms=1234;tick(seq);CHECK(!cf_moment.fill&&cf_moment.bloom==1&&cf_moment.bloom_ms==1234);
        CHECK(notes()==score(r,0,0));
        for(unsigned t=1;t<bt/2;t++){tick(seq);CHECK(notes()==score(r,0,t));}
        action(CFPERF_DROP);CHECK(!cf_moment.dropped);boundary(seq);notes();
        CHECK(cf_moment.dropped&&!cf_moment.fill);
        for(unsigned t=0;t<bt*2+bt/3;t++){tick(seq);CHECK(notes()==0);}
        action(CFPERF_FILL);CHECK(!cf_moment.pending); /* no hidden unmute */
        phase=pos(seq);action(CFPERF_BACK);CHECK(pos(seq)==phase&&cf_moment.dropped);
        boundary(seq);CHECK(!cf_moment.dropped&&cf_moment.bloom==2);notes();
        for(unsigned t=1;t<bt;t++) {
            tick(seq);unsigned b=seq?sqp.sb[SQ_DRUMS]:dmp.bar;
            CHECK(notes()==score(r,b,t));
        }
        CHECK(!memcmp(&before,&sq_st.pat[0],sizeof before));
        clean();CHECK(!cf_moment.pending&&!cf_moment.fill&&!cf_moment.dropped&&!cf_moment.bloom);
    }
}
static void transitions(void)
{
    start(0,0);action(CFPERF_FILL);action(CFPERF_DROP);boundary(0);
    CHECK(cf_moment.dropped&&!cf_moment.fill&&!cf_moment.bloom);
    action(CFPERF_DROP);action(CFPERF_BACK);boundary(0);CHECK(!cf_moment.dropped&&cf_moment.bloom==1);
    action(CFPERF_FILL);boundary(0);action(CFPERF_DROP);boundary(0);
    CHECK(cf_moment.dropped&&!cf_moment.fill&&cf_moment.bloom==1);
    action(CFPERF_BACK);boundary(0);
    action(CFPERF_FILL);dmp_queue(4,DMK_808,0);boundary(0);
    CHECK(dmp.r==4&&cf_moment.fill&&cf_moment.style==CFPERF_TIGHT);
    /* Counted MIDI pulses advance the same state, never a UI/animation timer. */
    start(0,0);cfx_in.active=1;action(CFPERF_DROP);
    dmp_tick(0,0);CHECK(!cf_moment.dropped&&dmp.t==0);
    dmp_tick(0,95);CHECK(!cf_moment.dropped&&dmp.t==380);
    dmp_tick(0,1);CHECK(cf_moment.dropped&&dmp.t==0);
    action(CFPERF_BACK);dmp_tick(0,96);CHECK(!cf_moment.dropped&&cf_moment.bloom==1);
    /* Stop/panic and recorder ownership clear transient actions. */
    action(CFPERF_FILL);dmp_rec_in(0,SQ_CHORDS);CHECK(!cf_moment.pending);
    action(CFPERF_DROP);CHECK(!cf_moment.pending);
    clean();action(CFPERF_FILL);CHECK(!cf_moment.pending);
    start(0,1);sqp.cin=1;action(CFPERF_DROP);CHECK(!cf_moment.pending);sqp.cin=0;
    sqp_rec_set(SQ_CHORDS);action(CFPERF_FILL);CHECK(!cf_moment.pending);sqp_rec_stop();
    sq_st.pat[0].mute|=1u<<SQ_DRUMS;action(CFPERF_FILL);CHECK(!cf_moment.pending);
    clean();
}
static void melody_and_controls(void)
{
    start(0,1);sq_pat_t *p=&sq_st.pat[0];
    sq_ev_t chord={.trk=SQ_CHORDS,.n=3,.note={60,64,67},.len=SQ_U*4,.mask=0x91,.nb=1,.bass={36},.rootkey=0xFF};
    CHECK(sq_put(p,&chord)>=0);cfp_cfg.bass_on=1;
    sqp_start(0,0);CHECK(sqp.n==3&&sqp.nb==1);unsigned serial=sqp.dser;
    sq_pat_t before=*p;
    action(CFPERF_DROP);boundary(1);CHECK(sqp.n==3&&sqp.nb==1&&sqp.dser==serial&&sqp.left[0]>0);
    for(unsigned i=0;i<3;i++)CHECK(sqp.note[i]==chord.note[i]);
    CHECK(sqp.bass[0]==36&&p->mute==before.mute);
    action(CFPERF_BACK);boundary(1);CHECK(sqp.n==3&&sqp.nb==1&&sqp.dser==serial);
    CHECK(!memcmp(p,&before,sizeof before));
    cf_snapshot();cu.scr=SCR_HOME;home_boot_main=1;cf_home_bank=CF_HB_PERFORM;
    CHECK(home_stage()&&cf_home_bank==CF_HB_PERFORM);
    cf_chord_cfg_t cfg=cs.chord;uint16_t bpm=cs.bpm;
    ui_knob_turn(0,9);CHECK(cf_evq[cf_evq_r%CF_EVQ].op==CFE_PERFORM);cf_audio_block(0);
    CHECK(cf_moment.pending==CFPERF_FILL&&!memcmp(&cfg,&cs.chord,sizeof cfg)&&cs.bpm==bpm);
    ui_knob_turn(1,-3);cf_audio_block(0);CHECK(cf_moment.pending==CFPERF_DROP);
    ui_knob_turn(2,1);cf_audio_block(0);CHECK(cf_moment.pending==CFPERF_BACK);
    cf_snapshot();cf_knob_t k={0};ui_knob_info(2,&k);CHECK(!strcmp(k.name,"BACK")&&!strcmp(k.val,"Queued"));
    ui_knob_turn(3,1);CHECK(cs.bpm==bpm+1);cs.clock_in=1;ui_knob_turn(3,10);CHECK(cs.bpm==bpm+1);cs.clock_in=0;
    /* Navigation never consumes note keys; scene encoder leaves the transient view. */
    ui_button(B_HOME);CHECK(home_stage()&&cf_home_bank==CF_HB_LOOP);
    ui_button(B_HOME);CHECK(home_stage()&&cf_home_bank==CF_HB_MIX);
    ui_button(B_HOME);CHECK(home_stage()&&cf_home_bank==CF_HB_VARIATION);
    ui_button(B_HOME);CHECK(home_stage()&&cf_home_bank==CF_HB_ENGINE);
    ui_button(B_HOME);CHECK(home_stage()&&cf_home_bank==CF_HB_FLOW);
    ui_button(B_HOME);CHECK(home_stage()&&cf_home_bank==CF_HB_LEARN);
    ui_button(B_HOME);CHECK(home_stage()&&cf_home_bank==CF_HB_CORE);
    clean();cf_snapshot();cf_home_bank=CF_HB_PERFORM;ui_knob_turn(0,1);CHECK(cf_evq_w==cf_evq_r);
    cf_home_bank=CF_HB_CORE;
}

static void shared_start(unsigned r)
{
    clean();cs.clock_in=0;cfp_cfg.clock_in=0;cs.chord.tonic=0;cs.chord.scale=CF_SC_MAJOR;
    cs.chord.quantize=CF_Q_BASIC;cs.bass_on=cfp_cfg.bass_on=1;
    CHECK(cpa_prepare(&cpa_setup.plan,0,cs.chord,CPA_OFF,CPA_BOTH));
    cpa_setup.plan.beats=DM_RHY[r].beats;cpa_setup.plan.rhythm=r;
    cpa_setup.plan.kit=DM_RHY[r].kit;cpa_setup.plan.swing=DM_RHY[r].swing;
    for(unsigned i=0;i<cpa_setup.plan.n;i++)cpa_setup.plan.slot[i].ticks=DM_RHY[r].beats*SQ_PPQN;
    CHECK(cpa_valid(&cpa_setup.plan));cpa_start(0);notes();
}
static void shared(void)
{
    for(unsigned r=0;r<DM_N;r++) {
        shared_start(r);unsigned bt=sqp_bar_ticks();cpa_plan_t before=cpa.plan;
        for(unsigned t=1;t<bt/3;t++){cpa_tick1();CHECK(notes()==score(r,0,t));}
        action(CFPERF_FILL);CHECK(cf_moment.pending==CFPERF_FILL);
        while(cpa.t<bt-1){cpa_tick1();CHECK(notes()==score(r,0,cpa.t));}
        cpa_tick1();notes();CHECK(cf_moment.fill&&cpa.t==0&&cpa.slot==1);
        for(unsigned i=0;i<bt;i++){cpa_tick1();notes();}
        CHECK(!cf_moment.fill&&cf_moment.bloom==1&&cpa.slot==2&&cpa.t==0);
        action(CFPERF_DROP);for(unsigned i=0;i<bt;i++){cpa_tick1();notes();}
        CHECK(cf_moment.dropped&&cpa.slot==3&&sqp.n&&sqp.nb);
        for(unsigned i=0;i<bt;i++){cpa_tick1();CHECK(!notes());}
        CHECK(cpa.slot==0&&sqp.n&&sqp.nb&&!memcmp(&before,&cpa.plan,sizeof before));
        action(CFPERF_BACK);for(unsigned i=0;i<bt;i++){cpa_tick1();if(i+1<bt)CHECK(!notes());else notes();}
        CHECK(!cf_moment.dropped&&cf_moment.bloom==2&&cpa.slot==1);
        /* Clock loss / Stop removes pending requests; Continue never replays a missed fill. */
        action(CFPERF_FILL);cpa_realtime(0xFC);CHECK(!cf_moment.pending&&cpa.paused);
        action(CFPERF_DROP);CHECK(!cf_moment.pending);cpa_realtime(0xFB);CHECK(!cf_moment.fill&&!cf_moment.dropped);
    }
    shared_start(0);cfp_cfg.clock_in=1;cpa.clock_external=1;cpa.clock_wait=1;action(CFPERF_FILL);CHECK(!cf_moment.pending);
    cpa.clock_wait=0;action(CFPERF_DROP);cpa_tick(0,95);CHECK(!cf_moment.dropped&&cpa.t==380);
    cpa_tick(0,1);CHECK(cf_moment.dropped&&cpa.t==0);
    cpa.clock_ms=cf_now_ms;cf_now_ms+=301;cpa_tick(0,0);CHECK(cpa.clock_lost&&!cf_moment.dropped);
    /* Latest queued groove supplies fill style on the same downbeat. */
    shared_start(0);cpa_plan_t next=cpa.plan;next.rhythm=4;next.kit=DMK_808;next.swing=0;cpa_publish(&next,CPA_GROOVE,0);
    action(CFPERF_FILL);for(unsigned i=0;i<384;i++)cpa_tick1();
    CHECK(cpa.plan.rhythm==4&&cf_moment.fill&&cf_moment.style==CFPERF_TIGHT);
    cf_snapshot();cu.now=cf_sn.now_ms;cu.scr=SCR_HOME;home_boot_main=1;cf_home_bank=CF_HB_PERFORM;cu.pop=0;cf_intro_on=0;
    cf_motion=1;cfn_motion.active=cfn_motion.fresh=1;cfn_motion.energy=128;cf_stage_model_t m;cf_stage_model(&m);CHECK(m.build&&m.pulse>=34&&!strcmp(m.status,"FILL")&&m.mascot.face==CFM_REACT);
    cf_motion=0;cf_stage_model(&m);CHECK(!m.pulse&&!m.build&&!m.bloom&&!strcmp(m.status,"FILL"));
    cf_ui_draw();unsigned draws=cf_stage_draws,tiles=fb_tiles_sent;
    for(unsigned i=0;i<20;i++){cu.now+=50;cf_ui_draw();}CHECK(draws==cf_stage_draws&&tiles==fb_tiles_sent);
    action(CFPERF_DROP);for(unsigned i=0;i<384;i++)cpa_tick1();cf_snapshot();cf_stage_model(&m);CHECK(!strcmp(m.status,"DRUMS OFF")&&!m.pulse);
    action(CFPERF_BACK);cf_now_ms=5000;for(unsigned i=0;i<384;i++)cpa_tick1();cf_snapshot();
    cf_motion=1;cu.now=cf_sn.now_ms+123456; /* UI clock has a distinct device boot origin */
    cf_sn.visual_muted=0;cf_sn.visual_peak=16000;cf_sn.visual_ms=cf_sn.now_ms;
    cfn_motion_frame();cu.now+=60;cfn_motion_frame();cf_stage_model(&m);CHECK(m.bloom&&m.pulse);
    cf_motion=0;cfn_motion_frame();cf_stage_model(&m);CHECK(!m.bloom&&!m.pulse);
    cf_motion=1;cfn_motion_frame();cf_stage_model(&m);CHECK(!m.bloom); /* Off->On cannot replay old bloom */
    /* A committed return while hidden must be consumed without replay on HOME. */
    cf_sn.perf_bloom++;cu.scr=SCR_FX;cfn_motion_frame();cu.scr=SCR_HOME;
    cfn_motion.active=1;cf_stage_model(&m);CHECK(!m.bloom);
    cf_sn.visual_muted=1;cfn_motion_frame();cf_stage_model(&m);CHECK(!m.pulse&&!m.build&&!m.bloom);
    clean();
}

static const char *preview_name="performance";
static void shot(const char *dir,const char *name)
{
    char path[1024];snprintf(path,sizeof path,"%s/%s-%s.ppm",dir,preview_name,name);
    cf_snapshot();cu.now=123456+cf_sn.now_ms;cu.scr=SCR_HOME;home_boot_main=1;cf_home_bank=CF_HB_PERFORM;cu.pop=POP_NONE;cf_intro_on=0;cf_ui_draw();
    FILE *f=fopen(path,"wb");CHECK(f);if(!f)return;
    fprintf(f,"P6\n240 240\n255\n");
    for(unsigned i=0;i<FB_W*FB_H;i++){uint32_t c=cf_pal[fb[i]];uint8_t rgb[]={c>>16,c>>8,c};fwrite(rgb,3,1,f);}fclose(f);
}
static void wav_header(FILE *f,unsigned frames)
{
    uint32_t sz=36+frames*4, sub=16,sr=FS,rate=FS*4,data=frames*4;uint16_t fmt=1,ch=2,align=4,bits=16;
    fwrite("RIFF",1,4,f);fwrite(&sz,4,1,f);fwrite("WAVEfmt ",1,8,f);fwrite(&sub,4,1,f);
    fwrite(&fmt,2,1,f);fwrite(&ch,2,1,f);fwrite(&sr,4,1,f);fwrite(&rate,4,1,f);fwrite(&align,2,1,f);fwrite(&bits,2,1,f);
    fwrite("data",1,4,f);fwrite(&data,4,1,f);
}
static void render(const char *dir,unsigned r,const char *name)
{
    shared_start(r);cf_ready=1;cfp_cfg.midi_out=0;cf_visual_want=cf_motion=1;
    cs.bpm=cfm.bpm=DM_RHY[r].bpm;cf_now_ms=1000;preview_name=name;
    cu.scr=SCR_HOME;home_boot_main=1;cf_home_bank=CF_HB_PERFORM;cu.pop=0;cf_intro_on=0;
    cpg.idea=0;cpg.rhythm=r;cpg.mode=CPA_BOTH;cu_spell_update();
    cf_fx_t fx=CFX_DEFAULT;fx.on=0;cf_fx_apply(&fx);fx_output_set(0,1);
    char path[1024];snprintf(path,sizeof path,"%s/%s.wav",dir,name);FILE *f=fopen(path,"wb");CHECK(f);if(!f)return;
    unsigned frames=(unsigned)((uint64_t)FS*60*4*8/DM_RHY[r].bpm);wav_header(f,frames);
    unsigned stage=0,clips=0,bloom_frame=0,last_shot=0;int32_t mix[CTL*2];
    for(unsigned n=0;n<frames;){
        unsigned count=frames-n<CTL?frames-n:CTL;
        cf_snapshot();cu.now=123456+cf_sn.now_ms;cfn_motion_frame();
        if(stage==0&&dmp.t>sqp_bar_ticks()/3){action(CFPERF_FILL);shot(dir,"01-fill-queued");stage++;}
        if(stage==1&&cf_moment.fill&&dmp.t>sqp_bar_ticks()*3/4){shot(dir,"02-fill-building");stage++;}
        if(stage==2&&cf_moment.bloom){shot(dir,"03-auto-return");action(CFPERF_DROP);stage++;}
        if(stage==3&&cf_moment.dropped){shot(dir,"04-drums-dropped");stage++;}
        if(stage==4&&n>frames*9u/16u){action(CFPERF_BACK);shot(dir,"05-back-queued");stage++;}
        if(stage==5&&cf_moment.bloom==2&&dmp.t>sqp_bar_ticks()/16){shot(dir,"06-back-bloom");stage++;}
        if(stage==6 && cf_now_ms-cf_moment.bloom_ms<650u && cf_now_ms-last_shot>=50u) {
            char frame[24];snprintf(frame,sizeof frame,"bloom-%02u",bloom_frame++);shot(dir,frame);last_shot=cf_now_ms;
        }
        audio_block(mix,count);int16_t pcm[CTL*2];
        for(unsigned i=0;i<count*2;i++){int32_t v=mix[i]>>8;clips+=v>=32767||v<=-32768;pcm[i]=ua_sat(v);}fwrite(pcm,2,count*2,f);n+=count;
    }
    fclose(f);CHECK(stage==6&&clips==0);cf_ready=1;clean();
}
int main(int argc,char **argv)
{
    emu_fw_options(NULL,1,0,1);emu_hal.ready=2;emu_fw_init(0);
    all_grooves();transitions();melody_and_controls();shared();
    if(argc==3&&!strcmp(argv[1],"--render")){render(argv[2],0,"lofi-fill-drop-back");render(argv[2],4,"trap-fill-drop-back");}
    printf("performance: %u checks, %u failures\n",checks,failures);return failures!=0;
}
