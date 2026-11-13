/* SPDX-License-Identifier: GPL-3.0-only */
#define CF_STUDIO 0
#include <stdio.h>
#include "../tools/emu/emu_fw.c"
static unsigned checks,failures;
#define CHECK(x) do{checks++;if(!(x)){failures++;if(failures<30)printf("FAIL %d: %s\n",__LINE__,#x);}}while(0)
static void blocks(unsigned n){int32_t out[CTL*2];while(n--)audio_block(out,CTL);cf_snapshot();cpa_ui_poll();}
static void setup(unsigned idea)
{
    cpa_stop();sqp_stop();dmp_stop();cfp_panic();cu.keys=0;cfds.pending=0;memset(&cfwd,0,sizeof cfwd);cfd_view_update();memset(&cfdu,0,sizeof cfdu);
    cu.scr=SCR_HOME;cu.page[SCR_PEDIT]=CFD_HARMONY;home_boot_main=1;cu.pop=POP_NONE;cpg.idea=idea;cpg.rhythm=CPA_OFF;cpg.mode=CPA_BOTH;cpg.repeat_off=0;
    const cf_progression_t *p=cf_progression_get(idea);cs.chord.tonic=p->tonic;cs.chord.scale=p->scale;cs.chord.quantize=CF_Q_BASIC;cs.clock_in=0;cs.beats=p->beats;cs.bpm=120;cs.bass_on=1;cf_send_config();cf_send_metro();blocks(2);cf_motion=0;
}
static void controls(void)
{
    setup(0);cf_progression_t source=*cf_progression_get(0);unsigned writes=emu_flash_writes;ui_button(B_EDIT);CHECK(cu.scr==SCR_PEDIT && cfwd.have && !cfwd.dirty);
    ui_select(2);cf_song_t before=cfwd.song;ui_knob_turn(1,2);CHECK(cfwd.dirty && cfwd.can_undo && cfwd.song.slot[2].quality==2 && cfds.pending && emu_flash_writes==writes);
    CHECK(!memcmp(&cfwd.song.slot[0],&before.slot[0],sizeof before.slot[0]) && !memcmp(&source,cf_progression_get(0),sizeof source));
    for(unsigned tab=0;tab<CFD_NTAB;tab++){cu.page[SCR_PEDIT]=tab;cf_ui_draw();unsigned hash=cfs_fnv(fb,FB_W*FB_H),draws=cfd_draws,tiles=fb_tiles_sent;cu.now+=2000;cf_ui_draw();CHECK(hash==cfs_fnv(fb,FB_W*FB_H) && draws==cfd_draws && tiles==fb_tiles_sent);}
    ui_button(B_HOME);cf_progression_choose(1);ui_button(B_EDIT);CHECK(cfdu.conflict && !cfdu.choice && cfwd.song.source_id==source.id);
    ui_button(B_SCL);CHECK(!cfdu.conflict && cpg.idea==CF_P_DRAFT && cfwd.song.source_id==source.id);
    cu.page[SCR_PEDIT]=CFD_PHRASE;cfdu.action=1;unsigned n=cfwd.song.n;ui_button(B_SCL);CHECK(cfwd.song.n==n+1 && cfdu.focus==3);
    cfdu.action=2;ui_button(B_SCL);CHECK(cfdu.confirm && !cfdu.choice);ui_button(B_SCL);CHECK(!cfdu.confirm && cfwd.song.n==n+1);
    ui_button(B_SCL);ui_knob_turn(0,1);ui_button(B_SCL);CHECK(cfwd.song.n==n);
    unsigned tempo=cs.bpm;cpa_tempo(1);blocks(2);cfd_ui_poll();CHECK(cfwd.can_undo && cfwd.song.bpm==tempo+1);
    cfdu.action=3;ui_button(B_SCL);CHECK(cfwd.song.n==n+1 && !cfwd.can_undo && cfwd.song.bpm==cs.bpm);
    while(cfwd.song.n<CF_SONG_MAX){cfdu.action=0;ui_button(B_SCL);}before=cfwd.song;ui_button(B_SCL);CHECK(!memcmp(&before,&cfwd.song,sizeof before));
}
static void scheduler(void)
{
    setup(0);ui_button(B_EDIT);cf_song_t next=cfwd.song;next.n=2;next.slot[0].unit=next.slot[1].unit=CF_SONG_QUARTERS;next.slot[0].quarters=next.slot[1].quarters=1;next.slot[0].gate=50;CHECK(cfd_commit(&next));
    ui_button(B_PLAY);blocks(2);CHECK(cpa.on && cpa.plan.idea==CF_P_DRAFT && cpa.plan.n==2 && cpa.plan.slot[0].ticks==24);
    for(unsigned i=0;i<12;i++)cpa_tick1();CHECK(!sqp.n && !sqp.nb && cpa.slot==0);
    for(unsigned i=0;i<12;i++)cpa_tick1();CHECK(cpa.slot==1 && sqp.n && cpa.t==24);
    /* Local edit waits for the next chord onset; grid/bar phase does not restart. */
    cu.page[SCR_PEDIT]=CFD_HARMONY;cfdu.focus=1;unsigned root=cpa.plan.slot[1].root;ui_knob_turn(0,1);CHECK(cpa_edit.flags&CPA_PHRASE && cpa.plan.slot[1].root==root);
    for(unsigned i=0;i<24;i++)cpa_tick1();CHECK(cpa.slot==0 && cpa.t==48 && cpa.plan.degree[1]==cfwd.song.slot[1].degree);
    /* Groove requests remain bar-bound even when the chord boundary is mid-bar. */
    cpa_ui_rhythm(2);unsigned r=cpa.plan.rhythm;next=cfwd.song;next.slot[0].quality=CF_SONG_MINOR;cfd_commit(&next);
    cf_snapshot();char pending[48],setup_text[48];cf_acc_pending(pending,sizeof pending,setup_text,sizeof setup_text);CHECK(strstr(pending,"Chord next / bar:")!=0);
    for(unsigned i=0;i<24;i++)cpa_tick1();CHECK(cpa.plan.rhythm==r && (cpa_edit.flags&CPA_GROOVE));
    while(cpa.t)cpa_tick1();CHECK(cpa.plan.rhythm!=r && !(cpa_edit.flags&CPA_GROOVE));
    cpa_home_key(1,1);while(cpa_edit.flags&CPA_NOTES)cpa_tick1();CHECK(cfc.cfg.tonic==cpa.plan.tonic);
    cpa_stop();
    /* Identical tied voicing crosses the real slot boundary without retrigger. */
    next=cfwd.song;next.n=2;next.slot[1]=next.slot[0];next.slot[0].tie=1;next.slot[0].gate=100;next.repeat_off=0;cfd_commit(&next);cpg.idea=CF_P_DRAFT;cpg.repeat_off=0;cpg.rhythm=CPA_OFF;
    cu.scr=SCR_PEDIT;ui_button(B_PLAY);blocks(2);unsigned serial=sqp.dser;for(unsigned i=0;i<24;i++)cpa_tick1();CHECK(cpa.slot==1 && sqp.dser==serial && sqp.n);
    cpa_stop();next=cfwd.song;while(next.n<16)CHECK(cf_song_insert(&next,next.n,0));next.repeat_off=1;cfd_commit(&next);cpg.repeat_off=1;
    ui_button(B_PLAY);blocks(2);unsigned total=0;for(unsigned i=0;i<cpa.plan.n;i++)total+=cpa.plan.slot[i].ticks;CHECK(cpa.plan.n==16 && cpa_valid(&cpa.plan));
    for(unsigned i=0;i<total;i++)cpa_tick1();CHECK(!cpa.on && !sqp.n && !sqp.nb);
}
static void duration_catalogue(void)
{
    for(unsigned beats=2;beats<=7;beats++)for(unsigned q=1;q<=128;q++) {
        setup(0);ui_button(B_EDIT);cf_song_t next=cfwd.song;next.n=1;next.beats=beats;next.repeat_off=1;
        next.slot[0].unit=CF_SONG_QUARTERS;next.slot[0].quarters=q;next.slot[0].gate=100;next.slot[0].tie=0;
        CHECK(cfd_commit(&next));ui_button(B_PLAY);blocks(2);unsigned ticks=q*(beats==6?48u:24u);
        CHECK(cpa.on && cpa.plan.slot[0].ticks==ticks);
        for(unsigned i=0;i+1<ticks;i++)cpa_tick1();CHECK(cpa.on);cpa_tick1();CHECK(!cpa.on && !sqp.n && !sqp.nb);
    }
    setup(0);ui_button(B_EDIT);cf_song_t next=cfwd.song;while(next.n<16)cf_song_insert(&next,next.n,0);
    for(unsigned i=0;i<16;i++){next.slot[i].degree=0;next.slot[i].mask=0;}
    CHECK(cfd_commit(&next));const cf_progression_t *p=cf_progression_current();uint32_t mask=cf_song_mask(&next,0,next.scale);
    CHECK(p->n==16 && cf_progression_highlight(p,next.scale,next.tonic,next.tonic,mask)==65535u);
    cfdu.focus=15;cfd_model_t model;cfd_model(&model);CHECK(model.first==12 && model.focus==15 && model.rail[3][0] && model.slot==255);
}
static void live_ownership(void)
{
    setup(0);cf_post(CFE_KEY,7,0,1);cf_post(CFE_KEY,25,0,1);cf_post(CFE_KEY,25,0,0);cf_post(CFE_KEY,7,0,0);blocks(2);
    uint8_t held[CF_MAX_NOTES];unsigned n=cfp.n;memcpy(held,cfp.note,n);cf_harmony_t harmony=ch;CHECK(cfc.out.hold && n);
    ui_button(B_EDIT);ui_knob_turn(1,2);ui_button(B_PLAY);blocks(2);CHECK(cpa.on && cfp.n==n && !memcmp(held,cfp.note,n) && !memcmp(&ch,&harmony,sizeof ch));
    ui_knob_turn(0,2);for(unsigned i=0;i<384;i++)cpa_tick1();CHECK(cfp.n==n && !memcmp(held,cfp.note,n) && cfc.out.hold);
    unsigned writes=emu_flash_writes;CHECK(!cfd_checkpoint() && emu_flash_writes==writes);cpa_stop();CHECK(!cfd_checkpoint() && cfp.n==n);
    cfp_panic();blocks(2);
}
static void external_status(void)
{
    setup(0);ui_button(B_EDIT);ui_knob_turn(1,1);cs.clock_in=1;cf_send_config();blocks(2);ui_button(B_PLAY);blocks(2);
    cfd_model_t model;cfd_model(&model);cf_stage_model_t home;cf_stage_model(&home);
    CHECK(!strcmp(model.status,"EXT WAIT") && model.slot==255 && home.slot==255 && !strstr(model.position,"playing"));
    cpa_tick(0,1);cf_snapshot();blocks(FS/CTL);cfd_model(&model);CHECK(!strcmp(model.status,"EXT LOST") && strstr(model.position,"paused") && !strstr(model.position,"playing"));
    cpa_realtime(0xFB);cpa_tick(0,1);cf_snapshot();cfd_model(&model);CHECK(strstr(model.position,"playing") && cpa.on && !cpa.paused);
    cpa_stop();cs.clock_in=0;cf_send_config();blocks(2);
}
static void recording_guard(void)
{
    setup(0);sq_defaults();sq_unavailable=0;sqk.on=0;sqk.last=255;ui_button(B_EDIT);ui_knob_turn(1,1);cf_song_t saved=cfwd.song;
    while(emu_stall_blocks)blocks(3);ui_button(B_REC);fm1_in.buttons=0;cf_rec_poll();blocks(12);CHECK(sqk.on && sqk.performance && cpa.on && sq_capture.state==SQ_PERF_ACTIVE && cu.scr==SCR_PEDIT);
    sq_home_poll();
    cfd_model_t model;cfd_model(&model);CHECK(model.rec && !strncmp(model.status,"REC",3));
    cu.page[SCR_PEDIT]=CFD_PHRASE;unsigned beats=cs.beats,bpm=cs.bpm;ui_knob_turn(2,1);cfdu.action=1;ui_button(B_SCL);
    CHECK(!memcmp(&saved,&cfwd.song,sizeof saved) && cs.beats==beats);
    cf_knob_t knob={0};cfd_knob_info(2,&knob);CHECK(knob.dis);
    CHECK(cs.bpm==bpm && cs.beats==beats && cpa_commanded);
    unsigned writes=emu_flash_writes;CHECK(!cfd_checkpoint() && writes==emu_flash_writes);
    sq_home_cancel();blocks(3);sq_home_poll();CHECK(!sqk.on && !memcmp(&saved,&cfwd.song,sizeof saved));
    cpa_stop();blocks(2);cf_sn.sq_on=1;cfd_model(&model);CHECK(!strcmp(model.status,"REPLAY"));ui_knob_turn(2,1);CHECK(!memcmp(&saved,&cfwd.song,sizeof saved));cf_sn.sq_on=0;
}
static void checkpoint(void)
{
    setup(0);ui_button(B_EDIT);ui_knob_turn(1,1);unsigned writes=emu_flash_writes;ui_button(B_PLAY);blocks(2);CHECK(!cfd_checkpoint() && emu_flash_writes==writes && cfds.pending);
    cpa_stop();blocks(FS/CTL*3);cu.keys=0;cf_sn.running=cf_sn.metro_on=0;cfx_quiet=FS/CTL*2;
    CHECK(cfd_checkpoint() && !cfds.pending && emu_flash_writes>writes);
    cf_song_t saved=cfwd.song;cfd_load();CHECK(cfwd.have && cfwd.dirty && cfds.checkpoint==CFD_RECOVERED && !cfds.pending && !memcmp(&saved,&cfwd.song,sizeof saved));
    /* Valid storage envelope but unsupported future codec is preserved, not overwritten. */
    uint8_t raw[CF_SONG_WIRE];CHECK(cf_song_encode(&cfwd.song,raw));raw[4]++;CHECK(!st_save(OBJ_CFDRAFT,raw,sizeof raw));writes=emu_flash_writes;cfd_load();CHECK(cfds.unavailable && !cfwd.have && emu_flash_writes==writes);
    ui_button(B_HOME);ui_button(B_EDIT);CHECK(cu.scr!=SCR_PEDIT && cfds.unavailable && emu_flash_writes==writes);
    CHECK(st_sector(OBJ_CFDRAFT,0)==0xEE000 && st_sector(OBJ_CFDRAFT,1)==0xEF000 && ST_LMAX(OBJ_CFDRAFT)>=CF_SONG_WIRE);
}
int main(void){emu_fw_options(0,1,0,1);emu_hal.ready=2;emu_fw_init(0);controls();scheduler();duration_catalogue();live_ownership();external_status();recording_guard();checkpoint();printf("native editor: %u checks, %u failures\n",checks,failures);return failures!=0;}
