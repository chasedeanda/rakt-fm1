/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include "usb_mock.h"
#define FELUCCA_UAC 1
#define FELUCCA_CDC 1
#include "../tools/emu/emu_fw.c"
static void ua_service_paced(uint32_t t, int isr) { (void)t; (void)isr; ua_service(); }
static unsigned checks, failures, test_ms;
#define CHECK(x) do { checks++; if (!(x)) { failures++; printf("FAIL %d: %s\n",__LINE__,#x); } } while (0)
static void advance(unsigned ms)
{
    int16_t out[HALF_FRAMES*2];
    for (unsigned t=0;t<ms;t+=3) {
        test_ms+=3;emu_fw_tick(test_ms);emu_fw_audio(out,HALF_FRAMES);
        if(test_ms%33==0)emu_fw_frame();
        cf_snapshot();cpa_ui_poll();sq_rec_poll();sq_home_poll();ua.cr=ua.cw;mo_r=mo_w;
    }
}
static void key(unsigned k, int down) { cf_post(CFE_KEY,k,0,down); advance(12); }
static void rec(void) { while(emu_stall_blocks)advance(3); ui_button(B_REC); fm1_in.buttons=0; cf_rec_poll(); advance(12); }
static void stop(void) { cf_post(CFE_PANIC,0,0,0); advance(12); }
static void empty(void)
{
    stop(); sq_defaults(); sq_unavailable=0; sq_dirty=0; sqk.on=0; sqk.last=0xFF;
    memset(emu_flash+0xE6000,0xFF,0x8000); sq_load(); cfs_restore_lock=0;
}
static void studio_home(void)
{
    cu.scr=SCR_HOME;home_boot_main=1;cf_home_bank=CF_HB_ENGINE;
    cf_scene_set(CF_SCENE_PROGRESSIONS);
    cgs_init(102);cgs.genre=CGS_DEFAULT_GENRE;cgs_prepare_start();
    cgu_home();
}
static void record_flow(unsigned rate)
{
    empty();sq_pref.protect=1;ua_reset();ua.cap_alt=1;ua.cap_hold=0;ua_rate_set(rate);
    cs.bpm=120;cs.beats=4;cs.clock_in=0;cs.chord.tonic=0;cs.chord.scale=CF_SC_MAJOR;
    cs.chord.quantize=CF_Q_BASIC;ch.mode=CF_H_AUTO;cu.arp_on=0;
    studio_home();
    cgs.arp=1u<<CGS_CHORDS;
    cs.midi_out=0;cf_send_config();cf_send_metro();advance(12);
    ui_button(B_PLAY);advance(120);
    CHECK(cgs_active() && cpa.on);
    unsigned grid=cf_grid.gen;
    rec();
    CHECK(cgu.tab==CGU_RECORD && cu.scr==SCR_HOME && cpa.on && sqk.on && sqk.performance);
    CHECK(sq_capture.state==SQ_PERF_ARMED);
    unsigned guard=0;
    while(sq_capture.state==SQ_PERF_ARMED && guard++<1200)advance(3);
    CHECK(sq_capture.state==SQ_PERF_ACTIVE && cf_grid.gen==grid);
    CHECK(crb_busy());
    advance(950);
    cgs.fill_held=1;
    key(7,1);advance(300);key(7,0);
    cgs.fill_held=0;
    rec();
    CHECK(!sqk.on && !cpa.on && sq_perf[0].bars==1 && sq_perf[0].n>0);
    const sq_perf_t *r=&sq_perf[0];
    CHECK(r->meta.beats==4 && r->meta.bpm==cfm.bpm);
    CHECK(sq_perf_valid(r,0,r->size) && crb_size_ok(CFB_TAKE,r->size));
    unsigned parts=0, pitched_off=0, kit=cpa.plan.kit, fill_hits=0;
    for(unsigned i=0;i<r->n;i++){
        sq_perf_ev_t e=r->ev[i];unsigned part=(e.when>>SQ_PERF_PART_SHIFT)&3u;
        if(part==SQ_DRUMS){
            CHECK(((e.data>>4)&31u)==kit);
            if((e.data&15u)>=5u && (e.data&15u)<=8u)fill_hits++;
            parts|=1u<<part;
        } else if(e.data>>7)parts|=1u<<part;
        else pitched_off|=1u<<part;
    }
    CHECK(parts==15u && (pitched_off&(1u<<SQ_CHORDS)) && fill_hits);
    unsigned hash=cfs_fnv(r,r->size);
    sq_save_write();sq_load();
    CHECK(sq_has(0) && cfs_fnv(r,r->size)==hash);
    csl_open();
    uint8_t rows[1+CSL_CAPACITY+SQ_NPAT];
    cslu.row=csl_rows(rows)-1;
    csl_execute();
    CHECK(cgs_home() && cgu.tab==CGU_RECORD && cgu.take_slot==0);
    ui_button(B_PLAY);advance(180);
    CHECK(sqp.on && sqp.pat==0 && sq_perf_cursor>0 && !cpa.on);
    CHECK(cfm.beats==r->meta.beats && cfm.bpm==r->meta.bpm);
    ui_button(B_PLAY);advance(30);
    CHECK(!sqp.on);
    for(unsigned part=0;part<SQ_DRUMS;part++)
        for(unsigned i=0;i<4;i++)CHECK(!sq_perf_held[part][i]);

    rec();CHECK(cgs_active() && sqk.on && sq_capture.state==SQ_PERF_ACTIVE);
    advance(200);sq_home_cancel();advance(30);
    CHECK(!sqk.on && cpa.on && !sq_has(1) && cfs_fnv(r,r->size)==hash);
    stop();
    sq_load();
    CHECK(!sq_has(1) && cfs_fnv(r,r->size)==hash);
}
static void capture_limit(void)
{
    empty();studio_home();
    rec();CHECK(sqk.on && cgs_active() && sq_capture.state==SQ_PERF_ACTIVE);
    unsigned end=cpa.plan.beats*SQ_PPQN*SQ_BARS;
    for(unsigned i=0;i<end;i++)cpa_tick1();
    sq_home_poll();
    const sq_perf_t *r=&sq_perf[0];
    CHECK(!sqk.on && cpa.on && r->bars==SQ_BARS && r->end==end);
    CHECK(sq_perf_valid(r,0,r->size));
    stop();
    memcpy(cfb_raw,r,r->size);
    crb.id=CFB_TAKE;crb.pos=crb.len=r->size;
    ((sq_perf_t *)cfb_raw)->ev[0].when=SQ_PERF_TICK;
    ((sq_perf_t *)cfb_raw)->check=sq_perf_check((const sq_perf_t *)cfb_raw);
    crb.crc=st_crc32(cfb_raw,crb.len);
    CHECK(crb_commit()==2);
    memcpy(cfb_raw,r,r->size);
    crb.crc=st_crc32(cfb_raw,crb.len);
    CHECK(crb_commit()==0);
    sq_load();cfs_restore_lock=0;
    CHECK(sq_has(0) && sq_perf_valid(&sq_perf[0],0,sq_perf[0].size));
}
static void synthetic_take(unsigned note)
{
    cu.scr=SCR_HOME;sq_home_rec_r(DM_N,0);advance(12);CHECK(sqk.on);
    sq_pat_t *p=&sq_st.pat[sqk.pat];
    p->ev[0]=(sq_ev_t){.trk=SQ_CHORDS,.n=1,.note={note},.rootkey=0xFF,.len=24};p->nev=1;
    sqk.rolled=sqk.seen=1;sqk.nbar=0;sqk.t=24;sqk.bt=96;
    sq_home_end(0);stop();sq_save_write();
    CHECK(sq_number(sqk.pat)==1 && sq_st.pat[sq_view_slot(0)].ev[0].note[0]==note);
}
static void history(void)
{
    empty();CHECK(!sq_pref.protect);
    cpg.idea=0;cpg.rhythm=dm_nth(4,0);cs.beats=4;cs.bpm=120;
    synthetic_take(61);synthetic_take(62);synthetic_take(63);
    CHECK(sq_st.pat[sq_view_slot(0)].ev[0].note[0]==63 &&
          sq_st.pat[sq_view_slot(1)].ev[0].note[0]==62 &&
          sq_st.pat[sq_view_slot(2)].ev[0].note[0]==61);
    unsigned oldest=sq_view_slot(2),before=cfs_fnv(&sq_st,sizeof sq_st);
    cfps_set(oldest,0,&cf_snd[0]);cf_sref_t ref=cfps.r[oldest][0];
    cu.scr=SCR_HOME;sq_home_rec_r(DM_N,0);advance(12);CHECK(sqk.on && sqk.pat==oldest && sq_number(oldest)==1);
    CHECK(crb_busy());unsigned writes=emu_flash_writes;sq_save_write();CHECK(emu_flash_writes==writes);
    sq_home_cancel();advance(90);
    CHECK(!crb_busy());CHECK(cfs_fnv(&sq_st,sizeof sq_st)==before && !memcmp(&cfps.r[oldest][0],&ref,sizeof ref));
    synthetic_take(64);sq_load();
    CHECK(sq_st.pat[sq_view_slot(0)].ev[0].note[0]==64 &&
          sq_st.pat[sq_view_slot(1)].ev[0].note[0]==63 &&
          sq_st.pat[sq_view_slot(2)].ev[0].note[0]==62);
    cu.scr=SCR_SQ;squ.pat=sq_view_slot(0);sq_algo(1);CHECK(sq_number(squ.pat)==2);
    sq_algo(1);CHECK(sq_number(squ.pat)==3);sq_algo(-2);CHECK(sq_number(squ.pat)==1);
    cu.scr=SCR_GLO;cu.page[SCR_GLO]=CF_GLO_RECORDINGS;ui_knob_turn(0,-1);
    CHECK(sq_pref.protect && sq_pref_dirty);sq_save_write();sq_pref_load();CHECK(sq_pref.protect);
    before=cfs_fnv(&sq_st,sizeof sq_st);cu.scr=SCR_HOME;sq_home_rec_r(DM_N,0);advance(12);
    CHECK(!sqk.on && cfs_fnv(&sq_st,sizeof sq_st)==before);
    cu.scr=SCR_GLO;cu.page[SCR_GLO]=CF_GLO_RECORDINGS;ui_knob_turn(0,1);
    sq_save_write();sq_pref_load();CHECK(!sq_pref.protect);
    synthetic_take(65);sq_load();CHECK(sq_st.pat[sq_view_slot(0)].ev[0].note[0]==65);
}
static void legacy(void)
{
    empty(); static sq_legacy_store_t old;
    memset(&old,0,sizeof old);old.magic=SQ_MAGIC;old.size=sizeof old;
    old.song.n=2;old.song.pat[0]=7;old.song.pat[1]=0;old.song.mute=SQ_MUTE_OWN;
    for(unsigned i=0;i<SQ_LEGACY_NPAT;i++) {
        sq_fresh(&old.pat[i],4);old.pat[i].hit[0][0]=1u<<i;old.pat[i].dlev[0]=80+i;
        old.pat[i].ev[0]=(sq_ev_t){.trk=SQ_CHORDS,.n=3,.note={60,64,67},.rootkey=0xFF,.len=24};old.pat[i].nev=1;
    }
    old.check=cfs_fnv(&old.song,sizeof old-__builtin_offsetof(sq_legacy_store_t,song));
    for(unsigned k=0;k<SQ_NCHUNK;k++)CHECK(!st_save(OBJ_CFSEQ0+k,(uint8_t *)&old+k*SQ_CHUNK,sq_chunk_len(k)));
    unsigned archive=st_crc32(emu_flash+0xB8000,0xC000);
    sq_load();CHECK(!sq_st.song.n && sq_st.pat[2].hit[0][0]==4 && sq_st.pat[0].ev[0].note[1]==64);
    sq_save_write();CHECK(st_crc32(emu_flash+0xB8000,0xC000)==archive);
    sq_clear_take(0);sq_save_write();sq_load();CHECK(!sq_has(0) && sq_has(1));
    CHECK(st_crc32(emu_flash+0xB8000,0xC000)==archive);
    /* Malformed records cannot silently resurrect a cleared legacy take. */
    sq_take_rec_t bad=*sq_take_image(0);bad.version++;
    CHECK(!st_save(OBJ_CFTAKE0,&bad,sizeof bad));sq_load();CHECK((sq_unavailable&1u) && !sq_has(0));
    sq_clear_take(0);sq_save_write();sq_load();CHECK(!sq_unavailable && !sq_has(0));
    /* Older CFQ5 inputs migrate A..C while their untouched D..H remain exportable. */
    memset(emu_flash+0xE6000,0xFF,0x8000);
    static uint8_t v5[48+SQ_LEGACY_NPAT*SQ_PAT5];memcpy(v5,&old,48);
    for(unsigned i=0;i<SQ_LEGACY_NPAT;i++)memcpy(v5+48+i*SQ_PAT5,&old.pat[i],SQ_PAT5);
    uint32_t head[3]={SQ_MAGIC5,sizeof v5,cfs_fnv(v5+12,sizeof v5-12)};memcpy(v5,head,sizeof head);
    for(unsigned k=0;k<SQ_NCHUNK;k++){
        unsigned off=k*SQ_CHUNK,n=sizeof v5-off;if(n>SQ_CHUNK)n=SQ_CHUNK;
        CHECK(!st_save(OBJ_CFSEQ0+k,v5+off,n));
    }
    archive=st_crc32(emu_flash+0xB8000,0xC000);sq_load();
    CHECK(sq_st.pat[2].hit[0][0]==4 && sq_st.pat[2].dlev[0]==100);
    sq_save_write();CHECK(st_crc32(emu_flash+0xB8000,0xC000)==archive);
    cfk_rec_t archived;cfk_build(0,&archived);archived.edited=1;archived.pat1=8;
    archived.check=cfs_fnv(&archived,__builtin_offsetof(cfk_rec_t,check));
    CHECK(!st_save(OBJ_CFTWK7,&archived,sizeof archived));cfk_load();
    CHECK(cfk_st[CFK_NST-1].edited && cfk_st[CFK_NST-1].pat1==8 && !cfk_st[CFK_NST-1].drop);
    for(unsigned i=3;i<CFK_NST;i++){cfk_st[i].edited=1;cfk_st[i].pat1=8;}
    cfk_own[0]=1;CHECK(cfk_store_for(0)<0); /* archived tweaks cannot be reclaimed for new takes */
}
int main(void)
{
    emu_fw_options(NULL,1,0,1);emu_hal.ready=2;emu_fw_init(0);advance(1600);
    CHECK(sizeof sq_st==7656 && SQ_NPAT==3 && sizeof(sq_take_rec_t)==2556);
    record_flow(44100);record_flow(48000);capture_limit();history();legacy();
    printf("recordings: %u checks, %u failures\n",checks,failures);return failures!=0;
}
