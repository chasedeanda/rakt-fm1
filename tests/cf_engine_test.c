/* SPDX-License-Identifier: GPL-3.0-only; common Engine real-render regressions. */
#include <stdio.h>
#include <time.h>
#include "usb_mock.h"
#ifndef CF_STUDIO
#define CF_STUDIO 0
#endif
#define FELUCCA_UAC 1
#define FELUCCA_CDC 1
#include "../tools/emu/emu_fw.c"
static void ua_service_paced(uint32_t t0,int isr){(void)t0;(void)isr;ua_service();}
static unsigned checks,failures;
static FILE *engine_raw;
#define CHECK(x) do{checks++;if(!(x)){failures++;if(failures<35)fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);}}while(0)
static void blocks(unsigned n){int32_t out[CTL*2];while(n--){audio_block(out,CTL);ua.cr=ua.cw;}cf_snapshot();cpa_ui_poll();cge_poll();}
static void ticks(unsigned n){while(n--){cpa_tick1();mo_r=mo_w;}cf_snapshot();cpa_ui_poll();cge_poll();}
static void start(void)
{
 cpa_stop();sqp_stop();dmp_stop();cfp_panic();cu.keys=0;cf_boot_go=0;cfb.on=0;cf_intro_on=0;blocks(4);
 cge_init();cge_seed(0x434f5244);cfs_loaded=0;cu.scr=SCR_HOME;home_boot_main=1;cf_scene_set(CF_SCENE_PROGRESSIONS);cf_home_bank=CF_HB_ENGINE;
 cs.chord.tonic=4;cs.chord.scale=CF_SC_MAJOR;cs.chord.octave=0;cs.bpm=104;cs.beats=4;cs.bass_on=1;cu.arp_on=0;
 cge_load();cge.direction=cge.direction_request=CGE_REPEAT;blocks(64);cpa_ui_play();blocks(1);CHECK(cge_active());
}
static void plan(void)
{
 start();CHECK(cpa.plan.n==8 && cpa.plan.beats==4 && cs.bpm==104);CHECK(cge.energy==52 && cge.feel==35 && cge.space==55);
 CHECK(!strcmp(cf_snd[0].name,"GHOST CHOIR") && !strcmp(cf_snd[1].name,"TAPE SUB"));CHECK(DM_RHY[cpa.plan.rhythm].id==56);
 for(unsigned i=0;i<8;i++){CHECK(cpa.plan.slot[i].ticks==768);if(i>=4)CHECK(!memcmp(&cpa.plan.slot[i],&cpa.plan.slot[i-4],sizeof(cpa_slot_t)));}
 cf_song_t s;CHECK(cge_snapshot(&s));CHECK(s.source_id==cge.current.id && s.n==8);uint8_t wire[CF_SONG_WIRE];cf_song_t restored;
 CHECK(cf_song_encode(&s,wire) && cf_song_decode(&restored,wire,sizeof wire));CHECK(restored.rhythm==56);
 for(unsigned i=0;i<8;i++){cf_song_chord_t o;CHECK(cf_song_resolve(&restored,i,&cs.chord,0,0,&o));CHECK(o.mask==cpa.plan.slot[i].mask && o.bass_pc==cpa.plan.slot[i].bass_pc);}
}
static void controls(void)
{
 start();uint16_t id=cge.current.id;cge_regenerate(0);CHECK(cge.pending&CGE_HARMONY);ticks(15*384);CHECK(cge.current.id==id && cge.pending);
 ticks(384);CHECK(cge.current.id!=id && !cge.pending);cge_regenerate(1);ticks(16*384);CHECK(cge.current.id==id && !cge.pending);
 cge.locks=31;uint32_t rng=cge.rng[0];cge_regenerate(0);CHECK(cge.rng[0]==rng && !cge.pending);cge.locks=0;
 cpa_home_key(1,1);ticks(768);CHECK(cpa.plan.tonic==5);cpa_home_key(2,1);ticks(768);CHECK(cpa.plan.scale==CF_SC_DORIAN);
 ticks(100);uint32_t elapsed=cpa.elapsed;cpa_tempo(3);blocks(2);CHECK(cs.bpm==107 && cpa.elapsed>=elapsed && cge_active());
 cge_knob_turn(0,-100);blocks(100);CHECK(!cge.target[1] && !cge.target[2] && cge.target[0]);
 cge_knob_turn(0,100);blocks(100);for(unsigned k=0;k<3;k++)CHECK(cge.gain[k]==32768);
}
static void actual_trace(const char *path,int interactive)
{
 start();cge.feel=0;cge.current.feel=0;cge.locks|=CGE_FEEL;
 usb.up=usb.config=1;cfp_cfg.midi_out=1;cfp_cfg.ch_chord=0;cfp_cfg.ch_bass=1;mo_r=mo_w;cpa_realtime(0xFA);
 FILE *f=fopen(path,"w");CHECK(f);if(!f)return;
 fprintf(f,"sample,cycle,bar,tick,stage,progression_id,seed,status,note,velocity,groove_id,arp,sound_chord,sound_bass,fx,vibe,direction,surprise,pins,pending\n");
 unsigned samples=0,last=999,peak=0,clips=0,held[2][128]={{0}},overlap=0;double squares=0;int32_t out[CTL*2];clock_t begin=clock();
 while(cge.cycle<4){
  unsigned position=cge.cycle*16+cge.bar;
  if(interactive && position!=last){last=position;if(position==5)cge_knob_turn(0,20);if(position==9)cge_knob_turn(1,100);
   if(position==11)cge_knob_turn(2,-55);if(position==16)cf_perf_queue(CFPERF_DROP);if(position==18)cge_regenerate(0);
   if(position==34)cf_perf_queue(CFPERF_BACK);if(position==36)cge_regenerate(1);if(position==40)cf_perf_queue(CFPERF_FILL);if(position==44)cpa_home_key(1,1);}
  while(mo_r!=mo_w){uint32_t q=midi_out_q[mo_r++%MQ];unsigned st=q>>8&255,n=q>>16&127,v=q>>24&127,ch=st&15;
   if((st&0xf0)==0x90 || (st&0xf0)==0x80){
    fprintf(f,"%u,%u,%u,%u,%s,%u,%u,%u,%u,%u,%u,%u,%s,%s,%u,%s,%s,%u,%u,%u\n",samples,cge.cycle,cge.bar+1,cpa.t,cge_stage_name(),cge.current.id,cge.session_seed,st,n,v,DM_RHY[cpa.plan.rhythm].id,cge.arp,cf_snd[0].name,cf_snd[1].name,cge.current.fx,CGE_VIBES[cge.current.vibe].name,CGE_DIRECTION_NAME[cge.direction],cge.surprise,cge.locks,cge.pending);
    if(ch<2){if((st&0xf0)==0x90 && v){overlap+=held[ch][n];held[ch][n]=1;}else held[ch][n]=0;}}
  }
  audio_block(out,CTL);if(engine_raw)fwrite(out,sizeof out,1,engine_raw);samples+=CTL;ua.cr=ua.cw;
  if(samples%(8*CTL)==0){cf_snapshot();cpa_ui_poll();cge_poll();}
  for(unsigned i=0;i<CTL*2;i++){int v=out[i]>>8;unsigned a=v<0?-v:v;if(a>peak)peak=a;clips+=a>=32767;squares+=(double)v*v;}
 }
 fclose(f);CHECK(!overlap && !clips && peak>100);CHECK(fabs((double)samples/FS-64*4*60.0/104)<2.0*CTL/FS);
 cpa_stop();blocks(FS*30/CTL);unsigned active=0;for(unsigned p=0;p<2;p++)for(unsigned j=0;j<NVOICE;j++)active+=trk[p].v[j].active!=0;CHECK(!active);
 printf("ENGINE_RENDER,%s,peak=%u,clips=%u,overlap=%u,rms=%.3f,host_cpu_seconds=%.6f,audio_seconds=%.6f\n",interactive?"interactive":"repeat",peak,clips,overlap,sqrt(squares/(samples*2)),(double)(clock()-begin)/CLOCKS_PER_SEC,(double)samples/FS);
}
static void live_and_clock(void)
{
 start();cge.feel=100;ticks(768);CHECK(cge.arp);
 cf_post(CFE_KEY,7,0,1);blocks(4);CHECK(cpa_live_chord());CHECK(!sqp.n && !cfa_sq[0].n);
 ticks(768);CHECK(!sqp.n && !cfa_sq[0].n);cf_post(CFE_KEY,7,0,0);blocks(4);
 CHECK(!cpa_live_chord() && cfa_sq[0].n==cpa.plan.slot[cpa.slot].n);
 cpa_stop();blocks((FS/20+CTL-1)/CTL+2);for(unsigned k=0;k<3;k++)CHECK(cge.gain[k]==32768);
 start();cfp_cfg.clock_in=1;cpa_realtime(0xFC);CHECK(cpa.paused);unsigned slot=cpa.slot,bar=cge.bar;
 cpa_tick(0,24);CHECK(cpa.paused && cge.bar==bar);cpa_realtime(0xFB);CHECK(!cpa.paused && cpa.slot==slot);
 cpa_realtime(0xFA);CHECK(cge.bar==0 && cpa.slot==0);cfp_cfg.clock_in=0;
}
#ifndef CF_ENGINE_HELPERS_ONLY
int main(int argc,char **argv)
{
 emu_fw_options(NULL,1,0,1);emu_hal.ready=2;emu_fw_init(0);
 if(argc==5 && !strcmp(argv[1],"--usb")){ua_reset();ua.cap_alt=atoi(argv[2]);ua.cap_hold=0;ua_rate_set(atoi(argv[3]));engine_raw=fopen(argv[4],"wb");CHECK(engine_raw);if(engine_raw){actual_trace("build/engine/usb-trace.csv",1);fclose(engine_raw);}}
 else {plan();controls();live_and_clock();if(argc>4){actual_trace(argv[3],0);actual_trace(argv[4],1);}}
 printf("RAKT Engine: %u checks, %u failures\n",checks,failures);return failures!=0;
}
#endif
