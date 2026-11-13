/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include "usb_mock.h"
#define FELUCCA_UAC 1
#define FELUCCA_CDC 1
#include "../tools/emu/emu_fw.c"
static void ua_service_paced(uint32_t t0, int isr) { (void)t0; (void)isr; ua_service(); }
static unsigned checks, failures;
#define CHECK(x) do { checks++; if (!(x)) { failures++; if (failures < 20) printf("FAIL %d: %s\n", __LINE__, #x); } } while (0)
static void blocks(unsigned n) { int32_t out[CTL*2]; while(n--) audio_block(out,CTL); cf_snapshot(); }
static void quiet(void)
{
    cf_boot_go=0; cfb.on=0; cf_intro_on=0;
    cf_post(CFE_PANIC,0,0,0); cf_post(CFE_SEQ,0,0,0); cf_post(CFE_DRUM,0,0,0);
    cfm.running=0; cu.keys=0; blocks(6000);
}
static void properties(void)
{
    cv_value_t base,a,b;
    cvr_capture(&base,0); CHECK(cvr_value_valid(&base));
    for(unsigned seed=1;seed<=2000;seed++) for(unsigned ctx=0;ctx<CV_N;ctx++) for(unsigned level=0;level<3;level++) {
        a=b=base;
        int ok=cvr_generate(&a,ctx,level,seed);
        CHECK(ok); CHECK(cvr_value_valid(&a));
        CHECK(cvr_generate(&b,ctx,level,seed)==ok && !memcmp(&a,&b,sizeof a));
        if(ctx==CV_DRUMS) {
            CHECK(b.bpm==base.bpm && b.beats==base.beats);
            if(level) {
                CHECK(b.rhythm!=base.rhythm);
                CHECK(b.kit==DM_RHY[b.rhythm].kit && b.swing==DM_RHY[b.rhythm].swing);
                CHECK(DM_RHY[b.rhythm].bpm >= (level==1?84:72) && DM_RHY[b.rhythm].bpm <= (level==1?116:128));
            } else CHECK(b.rhythm==base.rhythm && b.kit==base.kit && abs((int)b.swing-(int)base.swing)<=2);
        }
        if(ctx==CV_ARP) { CHECK(b.arp.latch==base.arp.latch); if(level==1)CHECK(b.arp.mode!=CF_ARP_RAND && !CF_ARP_STRUMMING(b.arp.mode)); }
        if(ctx==CV_SCALE) { CHECK(!base.auto_on || cf_harmony_supported(b.scale,b.quantize)); if(level<2)CHECK(b.tonic==base.tonic); }

    }
    for(unsigned scale=0;scale<CF_NSCALES;scale++) for(unsigned quant=0;quant<=CF_Q_FULL;quant++) {
        base.scale=scale;base.quantize=quant;
        for(unsigned ctx=0;ctx<CV_N;ctx++) { a=base;if(cvr_generate(&a,ctx,2,42)) CHECK(cvr_value_valid(&a)); }
    }
    /* Chaos can reach the complete explorer catalog, including entries beyond
     * the old 64-element temporary choice array. It does not alter key/scale. */
    uint8_t seen[CF_P_NONE]={0}; unsigned reached=0;
    for(unsigned seed=1;seed<=4000;seed++) {
        a=base; CHECK(cvr_generate(&a,CV_HARMONY,2,seed));
        CHECK(a.tonic==base.tonic && a.scale==base.scale);
        if(!seen[a.idea]) { seen[a.idea]=1; reached++; }
    }
    CHECK(reached==cf_progression_count());
    a=base; CHECK(!cvr_generate(&a,99,0,1)); CHECK(!cvr_generate(&a,CV_FX,3,1));
}
static void controls(void)
{
    cf_set_t original=cs;cf_harmony_t harmony=ch;
    uint32_t patterns=cfs_fnv(&sq_st,sizeof sq_st),bank=cfs_fnv(&up_bank,sizeof up_bank),flash=emu_flash_writes;
    CHECK(sizeof(cs)==188 && sizeof(sq_ev_t)==22);
    cu.scr=SCR_ARP;ui_button(B_GLO);
    if(cgu_glow.active)ui_button(B_SAVE);
    CHECK(cu.scr==SCR_GLO && cvr.context==CV_ARP);
    cu.page[SCR_GLO]=CF_GLO_VARIATION;cvr.level=1;
    cf_arp_t before=cs.arp;
    ui_knob_turn(2,1);cf_arp_t generated=cs.arp;CHECK(cvr.have_previous);
    ui_knob_turn(3,1);CHECK(!memcmp(&cs.arp,&before,sizeof before));
    ui_knob_turn(3,1);CHECK(!memcmp(&cs.arp,&generated,sizeof generated));
    CHECK(emu_flash_writes==flash && !memcmp(&ch,&harmony,sizeof harmony));
    CHECK(cfs_fnv(&sq_st,sizeof sq_st)==patterns && cfs_fnv(&up_bank,sizeof up_bank)==bank);
    /* Presets keep the existing save/confirmation UI. No random action saves a bank. */
    cvr.context=CV_FX;fxp_bank_t fxbank=fxp_bank;ui_button(B_SAVE);
    CHECK(cu.scr==SCR_FX && fxpu.mode==FXP_MANAGE && !cu.save_hold);
    CHECK(!memcmp(&fxbank,&fxp_bank,sizeof fxbank));
    cu.scr=SCR_GLO;cu.page[SCR_GLO]=CF_GLO_VARIATION;cvr.context=CV_SOUND;cvr.part=1;
    ui_button(B_SAVE);CHECK(cu.scr==SCR_SAVE && cu.page[SCR_SAVE]==1 && cu.save_new);
    CHECK(cfs_fnv(&up_bank,sizeof up_bank)==bank && emu_flash_writes==flash);
    cu.scr=SCR_GLO;cu.page[SCR_GLO]=CF_GLO_VARIATION;
    for(unsigned ctx=0;ctx<CV_N;ctx++) for(unsigned level=0;level<3;level++) {
        cvr.context=ctx;cvr.level=level;
        for(int k=0;k<4;k++){cf_knob_t knob={0};ui_knob_info(k,&knob);}cf_ui_draw();
    }
    ui_knob_turn(0,1000);CHECK(cvr.context==CV_N-1);ui_knob_turn(1,-1000);CHECK(cvr.level==0);
    ui_select(CF_GLO_NPAGES-CF_GLO_VARIATION);CHECK(cu.page[SCR_GLO]==0 && screen_pages(SCR_GLO)==CF_GLO_NPAGES);
    cs=original;ch=harmony;cf_send_config();quiet();
}
static void sound(void)
{
    for(unsigned part=0;part<3;part++) {
        cv_value_t before,after,undo;
        cvr_capture(&before,part); cvr.context=CV_SOUND;cvr.part=part;cvr.level=2;
        cvr_new(); cvr_capture(&after,part); CHECK(memcmp(before.env,after.env,30));
        cvr_undo(); cvr_capture(&undo,part);CHECK(!memcmp(&before,&undo,sizeof before));
        cvr_undo(); cvr_capture(&undo,part);CHECK(!memcmp(&after,&undo,sizeof after));
        /* Never apply a saved envelope to an unrelated patch. */
        cvr.previous.sound.name[0]^=1;unsigned q=cf_evq_w;cvr_undo();CHECK(cf_evq_w==q);
    }
#if FELUCCA_VA
    CHECK(cf_snd_find(0,ENGI_VA,"SOFT LEAD"));
    cv_value_t before,after;cvr_capture(&before,0);CHECK(before.env_kind==2);
    cvr.part=0;cvr_new();cvr_undo();cvr_capture(&after,0);CHECK(!memcmp(&before,&after,sizeof before));
#endif
}
static void boundaries(void)
{
    quiet();cvr.context=CV_DRUMS;cvr.level=2;cvr.part=0;
    dm_rec_t old=dmr;uint8_t view=dm_view;uint16_t bpm=cs.bpm;
    cvr_new();dm_rec_t generated=dmr;CHECK(cs.bpm==bpm);cvr_undo();CHECK(!memcmp(&old,&dmr,sizeof dmr)&&dm_view==view);
    cvr_undo();CHECK(!memcmp(&generated,&dmr,sizeof dmr));
    /* The actual ISR retains the current groove until its next bar. */
    dm_play(dm_cur());blocks(1);CHECK(dmp.on);unsigned playing=dmp.r;
    cvr_new();unsigned chosen=dm_cur();blocks(1);
    CHECK(dmp.r==playing && dmp.next==chosen);
    dmp_next_bar();CHECK(dmp.r==chosen && dmp.next==0xFF && dmp.t==0);
    cvr_undo();blocks(1);CHECK(dmp.r==chosen);dmp_next_bar();CHECK(dmp.r==playing);
    /* Recording cannot be randomized; no saved sequencer data changes. */
    cf_snapshot();uint32_t pat=cfs_fnv(&sq_st,sizeof sq_st);cf_arp_t arp=cs.arp;
    sqk.on=1; /* a take, rather than independent drum playback */
    cvr.context=CV_ARP;unsigned seed=cvr.seed;cvr_new();CHECK(cvr.pending==1 && seed==cvr.seed&&!memcmp(&arp,&cs.arp,sizeof arp));
    cvr_undo();CHECK(!cvr.pending && !memcmp(&arp,&cs.arp,sizeof arp));
    CHECK(cfs_fnv(&sq_st,sizeof sq_st)==pat);sqk.on=0;
    cf_sn.dm_rin=1;cvr.context=CV_DRUMS;seed=cvr.seed;cvr_new();
    CHECK(cvr.pending==1 && seed==cvr.seed);cvr_undo();cf_sn.dm_rin=0;
    quiet();
}
static void queued_controls(void)
{
    quiet(); cvr_init(); cu.scr=SCR_GLO;cu.page[SCR_GLO]=CF_GLO_VARIATION;
    cvr.context=CV_ARP;cvr.level=1;cu.arp_on=0;cf_send_config();blocks(1);
    /* An enabled but stopped click and long FX tails must not block settings. */
    cfm.on=1;cf_snapshot();cfx_quiet=0;CHECK(cfs_sounding());
    cvr_new();CHECK(!cvr.pending && cvr.have_previous);cfm.on=0;
    /* Queue under a real held chord; no settings, seed, undo or audio changes. */
    cf_post(CFE_KEY,1,0,1);cf_post(CFE_KEY,1,0,0);cf_post(CFE_KEY,7,0,1);blocks(1);
    dm_play(dm_cur());blocks(1);CHECK(cf_sn.dm_on);
    CHECK(cf_sn.out.sounding);cf_arp_t old=cs.arp;unsigned seed=cvr.seed;
    cvr_new();CHECK(cvr.pending==1 && seed==cvr.seed && !memcmp(&old,&cs.arp,sizeof old));
    cf_knob_t knob={0};ui_knob_info(3,&knob);CHECK(!strcmp(knob.name,"CANCEL")&&!knob.dis);
    unsigned flash=emu_flash_writes;cvr_poll();CHECK(cvr.pending && !memcmp(&old,&cs.arp,sizeof old));
    /* Manual edits made while waiting remain the baseline, not a stale snapshot. */
    cs.arp.gate=83;cv_value_t expected;cvr_capture(&expected,0);cf_arp_t baseline=cs.arp;
    CHECK(cvr_generate(&expected,CV_ARP,1,seed));
    cf_post(CFE_KEY,7,0,0);blocks(1);cfx_quiet=0;cvr_poll();
    CHECK(!cvr.pending && cvr.seed==seed+1 && !memcmp(&expected.arp,&cs.arp,sizeof cs.arp));
    CHECK(emu_flash_writes==flash && dmp.on);cvr_undo();CHECK(!memcmp(&baseline,&cs.arp,sizeof baseline));
    /* Queued Undo can be canceled, then requested again and applied once idle. */
    cf_post(CFE_KEY,7,0,1);blocks(1);old=cs.arp;cvr_undo();CHECK(cvr.pending==2);
    cvr_undo();CHECK(!cvr.pending && !memcmp(&old,&cs.arp,sizeof old));
    cvr_undo();CHECK(cvr.pending==2);cf_post(CFE_KEY,7,0,0);blocks(1);cvr_poll();
    CHECK(!cvr.pending && !memcmp(&expected.arp,&cs.arp,sizeof cs.arp));
    /* Leaving, changing amount, and replacing the sound cancel pending work. */
    cf_post(CFE_KEY,7,0,1);blocks(1);cvr_new();CHECK(cvr.pending);
    cu.scr=SCR_HOME;cvr_poll();CHECK(!cvr.pending);
    cu.scr=SCR_GLO;cvr_new();CHECK(cvr.pending);ui_knob_turn(1,1);cvr_poll();CHECK(!cvr.pending);
    cvr.context=CV_SOUND;cvr.part=0;cvr_new();CHECK(cvr.pending);
    cf_snd_gen[0]++;cvr_poll();CHECK(!cvr.pending);cf_snd_gen[0]--;
    cf_post(CFE_KEY,7,0,0);quiet();
    /* Sound edits still wait for that part's release-stage voices. */
    trk[0].v[0].active=1;cvr_new();CHECK(cvr.pending);trk[0].v[0].active=0;cvr_poll();CHECK(!cvr.pending);
    /* Protected state never queues work which could unexpectedly apply later. */
    cr_safe=1;cvr_new();CHECK(!cvr.pending);cr_safe=0;
}
/* Queued changes while playing must produce bit-identical physical and all
 * six USB channels, including a held Auto chord, bass, arp, rhythm and FX. */
static void audio(int attempt,unsigned rate,const char *path)
{
    FILE *file=fopen(path,"wb");CHECK(file);if(!file)return;
    int32_t out[CTL*2];cf_intro_on=0;cf_boot_go=0;
    ch.mode=CF_H_AUTO;cu.arp_on=1;cf_send_config();blocks(64);
    cf_post(CFE_KEY,1,0,1);cf_post(CFE_KEY,1,0,0);cf_post(CFE_KEY,7,0,1);
    dm_play(dm_cur());blocks(2);
    ua_reset();ua.cap_alt=1;ua.cap_hold=0;ua_rate_set(rate);
    cvr.context=CV_SCALE;cu.scr=SCR_GLO;cu.page[SCR_GLO]=CF_GLO_VARIATION;uint32_t flash=emu_flash_writes;
    for(unsigned b=0;b<FS*2u/CTL;b++) {
        if(attempt&&b%17==0) { cvr_new();cvr_poll();CHECK(cvr.pending && !cvr.have_previous); }
        uint32_t w=ua.cw;audio_block(out,CTL);fwrite(out,sizeof out,1,file);
        for(unsigned i=w;i<ua.cw;i++)fwrite(ua.cap+(i&(UA_CAP_RING-1u))*6u,6*sizeof(int16_t),1,file);
        ua.cr=ua.cw;
    }
    CHECK(flash==emu_flash_writes && cfc.out.sounding && dmp.on);fclose(file);
}
static void home_controls(void)
{
    quiet();cvr_init();cu.scr=SCR_HOME;home_boot_main=1;cf_home_bank=CF_HB_VARIATION;
    cf_scene_set(CF_SCENE_PROGRESSIONS);cf_motion=0;cu.pop=0;
    for(int k=0;k<4;k++) {
        cf_knob_t a={0},b={0};ui_knob_info(k,&a);
        cu.scr=SCR_GLO;cu.page[SCR_GLO]=CF_GLO_VARIATION;ui_knob_info(k,&b);
        CHECK(!strcmp(a.name,b.name)&&!strcmp(a.val,b.val)&&a.dis==b.dis);
        cu.scr=SCR_HOME;
    }
    CHECK(CF_HB_VARIATION+1==CF_HB_ENGINE && CF_HB_ENGINE+1==CF_HB_FLOW && CF_HB_FLOW+1==CF_HB_LEARN && CF_HB_LEARN+1==CF_HB_N);
    cvr.context=CV_FX;cvr.level=0;cf_fx_t before=cs.fx;unsigned seed=cvr.seed;
    ui_knob_turn(2,1);CHECK(cvr.seed==seed+1&&cvr.have_previous&&cu.scr==SCR_HOME);
    ui_knob_turn(3,1);CHECK(!memcmp(&before,&cs.fx,sizeof before));
    cvr.context=CV_ARP;cf_post(CFE_KEY,7,0,1);blocks(4);
    ui_knob_turn(2,1);CHECK(cvr.pending==1);cvr_poll();CHECK(cvr.pending==1);
    ui_knob_turn(3,1);CHECK(!cvr.pending);
    ui_knob_turn(2,1);CHECK(cvr.pending==1);cf_home_bank=CF_HB_CORE;cvr_poll();CHECK(!cvr.pending);
    quiet();cf_home_bank=CF_HB_VARIATION;cvr.context=CV_FX;cu.pop=0;
    ui_button(B_SAVE);CHECK(cu.scr==SCR_FX&&fxpu.mode==FXP_MANAGE);
}
static void shared_playback(void)
{
    cpa_stop();quiet();cvr_init();cu.scr=SCR_HOME;home_boot_main=1;cf_home_bank=CF_HB_VARIATION;
    cs.chord.tonic=0;cs.chord.scale=CF_SC_MAJOR;cs.chord.quantize=CF_Q_BASIC;
    cpg.idea=0;cf_send_config();blocks(4);
    cpa_plan_t plan;CHECK(cpa_prepare(&plan,0,cs.chord,dm_nth(4,0),CPA_BOTH));
    cpa_publish(&plan,0,1);cf_post(CFE_ACCOMP,1,0,0);blocks(4);
    cvr.context=CV_HARMONY;cvr.level=0;cv_value_t expected;
    cvr_capture(&expected,0);CHECK(cvr_generate(&expected,CV_HARMONY,0,cvr.seed));
    unsigned original=cf_sn.ac_idea;
    cvr_new();CHECK(cpa_setup.flags && cpa_setup.plan.idea==expected.idea);
    CHECK(cpa.plan.idea==original); /* no early audible change */
    for(unsigned i=0;i<384;i++)cpa_tick1();cf_snapshot();cpa_ui_poll();
    CHECK(cf_sn.ac_idea==expected.idea && cpg.idea==expected.idea);
    cvr_undo();CHECK(cpa_setup.flags && cpa_setup.plan.idea==original);
    for(unsigned i=0;i<384;i++)cpa_tick1();cf_snapshot();cpa_ui_poll();
    CHECK(cf_sn.ac_idea==original);
    cvr.context=CV_DRUMS;cvr.level=1;cvr_capture(&expected,0);
    CHECK(cvr_generate(&expected,CV_DRUMS,1,cvr.seed));
    unsigned rhythm=cf_sn.ac_rhythm,slot=cf_sn.ac_slot;
    cvr_new();CHECK((cpa_edit.flags&CPA_GROOVE) && cpa_edit.plan.rhythm==expected.rhythm);
    CHECK(cpa.plan.rhythm==rhythm && cpa.slot==slot);
    for(unsigned i=0;i<384;i++)cpa_tick1();cf_snapshot();cpa_ui_poll();
    CHECK(cf_sn.ac_rhythm==expected.rhythm && dmp.r==expected.rhythm);
    CHECK(cf_sn.ac_kit==expected.kit && cf_sn.ac_swing==expected.swing);
    cvr_undo();CHECK(cpa_edit.plan.rhythm==rhythm);
    for(unsigned i=0;i<384;i++)cpa_tick1();cf_snapshot();
    CHECK(cf_sn.ac_rhythm==rhythm && dmp.r==rhythm);
    cpa_stop();quiet();
}
int main(int argc,char **argv)
{
    emu_fw_options(NULL,1,0,1);emu_hal.ready=2;emu_fw_init(0);
    if(argc==5)audio(atoi(argv[2]),(unsigned)atoi(argv[3]),argv[4]);
    else { quiet();properties();controls();sound();boundaries();queued_controls();home_controls();shared_playback(); }
    printf("variations: %u checks, %u failures\n",checks,failures);
    return failures!=0;
}
