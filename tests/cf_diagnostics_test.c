/* SPDX-License-Identifier: GPL-3.0-only */
/* Measurement contracts and read-only wire protocol; no device access. */
#include <stdio.h>
#include "usb_mock.h"
static uint8_t reply[512];static uint32_t reply_n;
#define CRB_SEND(p,n) do {memcpy(reply,(p),(n));reply_n=(n);}while(0)
#define FELUCCA_UAC 1
#define FELUCCA_CDC 1
#include "../tools/emu/emu_fw.c"
static void ua_service_paced(uint32_t t,int isr){(void)t;(void)isr;ua_service();}
static unsigned checks,fails;
#define CHECK(x) do {checks++;if(!(x)){fails++;printf("FAIL %d: %s\n",__LINE__,#x);}}while(0)
static uint32_t r32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<7|(uint32_t)p[2]<<14|(uint32_t)p[3]<<21|(uint32_t)p[4]<<28;}
int main(int argc,char **argv)
{
    emu_fw_options(NULL,1,0,1);emu_hal.ready=2;emu_fw_init(0);
    CHECK(sizeof cfd+sizeof cf_perf<=288 && D_N==51 && CRP_N==6);
    felucca_dbg.late=17;cr_dbg.stale=3;shed_count=4;voice_steals=9;voice_kills=13;
    ua.cap_underruns=5;ua.cap_overruns=6;ua.missed_frames=7;
    cf_diag_reset(100);
    CHECK(felucca_dbg.late==17 && cr_dbg.stale==3 && shed_count==4 && !cf_perf.halves);
    /* Independent maxima must not be added to invent a nonexistent worst half. */
    cf_perf_half(900,1000*FM1_TICKS_PER_US,100*FM1_TICKS_PER_US);
    cf_perf_half(50,100*FM1_TICKS_PER_US,1000*FM1_TICKS_PER_US);
    CHECK(cf_perf.pressure==1100*FM1_TICKS_PER_US && cf_perf.work==900 && cf_perf.halves==2);
    for(uint32_t p=0;p<NSND;p++)for(uint32_t i=0;i<NVOICE;i++)trk[p].v[i].active=i<=p;
    felucca_dbg.late+=2;shed_count++;voice_steals+=3;ua.cap_overruns+=2;
    uint32_t v[D_N];cf_diag_sample(v,600);
    CHECK(v[D_SESSION_MS]==500 && v[D_PEAK]==900 && v[D_WALL]==1000 && v[D_LAG]==1000 && v[D_PRESSURE]==1100);
    CHECK(v[D_LATE]==2 && v[D_SHED]==1 && v[D_STEAL]==3 && v[D_OVER]==2);
    CHECK(v[D_CHORD]==1 && v[D_BASS]==2 && v[D_LAYER]==3 && v[D_DRUMS]==4 && v[D_TAIL]==5);
    CHECK(!v[D_HARDWARE] && !v[D_RAM] && !v[D_RAM_CAP]);
    cfd.base[0]=0xFFFFFFFEu;felucca_dbg.late=1;cf_diag_sample(v,700);CHECK(v[D_LATE]==3);
    cf_diag_sample(cfd.v,700);cfd.hold=1;uint32_t held=cfd.v[D_PEAK];
    cf_perf_half(2100,2200*FM1_TICKS_PER_US,30*FM1_TICKS_PER_US);
    cpu_last.t0=700;cpu_last.ui_last=700;cpu_last.ui_n=0;aw_reset=0;
    aw_sum_us=12000;aw_n=10;aw_max_us=1800;aw_max_all=2000*FM1_TICKS_PER_US;
    cf_diag_poll(1800);
    CHECK(cfd.v[D_PEAK]==held && cpu_last.avg_us==1200 && cpu_last.max_us==1800 && aw_reset==1);
    cf_diag_sample(v,1800);CHECK(v[D_PEAK]==2100 && v[D_PRESSURE]==2230);
    /* Reading command 80 neither clears peaks/counters nor unfreezes the display. */
    uint8_t req[]={CRB_HDR0,CRB_HDR1,CRB_HDR2,80};fm1_ms=1800;
    CHECK(crb_handle(req,sizeof req));CHECK(reply_n==8u+5u*(D_N+2u*CRP_N));
    CHECK(reply[4]==80 && reply[5]==1 && reply[6]==D_N+2u*CRP_N);
    CHECK(r32(reply+7+D_PEAK*5u)==2100 && r32(reply+7+D_CHORD*5u)==1);
    CHECK(cfd.hold && cfd.v[D_PEAK]==held && cf_perf.work==2100 && felucca_dbg.late==1);
    if(argc>1){FILE *f=fopen(argv[1],"wb");CHECK(f);if(f){fwrite(reply,1,reply_n,f);fclose(f);}}
    uint8_t bad[]={CRB_HDR0,CRB_HDR1,CRB_HDR2,80,1};CHECK(!crb_handle(bad,sizeof bad));
    /* GLO controls stay bounded, hold preserves collection, reset leaves transport/sounds alone. */
    CHECK(cgs_home());
    ui_button(B_GLO);CHECK(cgu_glow.active);
    ui_button(B_SAVE);CHECK(!cgu_glow.active && cu.scr==SCR_GLO);
    ui_select(6-(int)cu.page[SCR_GLO]);CHECK(cu.page[SCR_GLO]==6);
    cu.now=1850;
    ui_knob_turn(0,20);CHECK(cfd.page==DP_USB);ui_knob_turn(0,-20);CHECK(cfd.page==DP_OVERVIEW);
    ui_knob_turn(1,1);CHECK(cfd.hold);ui_knob_turn(1,-1);CHECK(!cfd.hold);
    for(unsigned i=0;i<5;i++)ui_knob_turn(3,1);CHECK(cfd.rate==2);
    for(unsigned i=0;i<5;i++)ui_knob_turn(3,-1);CHECK(cfd.rate==0);
    uint32_t engine=trk[0].engine,bpm=cs.bpm,seq=sqp.on;
    ui_knob_turn(2,1);CHECK(cfd.start==1850 && !cf_perf.work && !cfd.hold);
    CHECK(trk[0].engine==engine && cs.bpm==bpm && sqp.on==seq && shed_count==5);
    /* Extreme numeric formats and all pages must stay within their buffers. */
    for(unsigned i=0;i<D_N;i++)cfd.v[i]=0xFFFFFFFFu;
    cfd.v[D_HARDWARE]=1;cfd.v[D_BUDGET]=2902;cfd.v[D_RAM]=94940;cfd.v[D_RAM_CAP]=98304;
    cfd.v[D_POOL]=329756;cfd.v[D_POOL_CAP]=344064;
    for(cfd.page=0;cfd.page<DP_N;cfd.page++)cf_ui_draw();
    cfd.page=0;cfd.hold=0;cf_diag_reset(1900);
    CHECK(cf_perf.work==0 && felucca_dbg.late==1 && shed_count==5 && cfd.start==1900);
    ui_button(B_HOME);CHECK(cgs_home());
    printf("diagnostics: %u checks, %u failures\n",checks,fails);return fails!=0;
}
