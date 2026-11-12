/* SPDX-License-Identifier: GPL-3.0-only; G1 catalog, memory and shared-clock acceptance. */
#define CF_ENGINE_HELPERS_ONLY
#include "cf_engine_test.c"
static void catalog(void)
{
 start();CHECK(cf_progression_count()==130 && cge_resolve_assets());
 static const unsigned gains[2][4][3]={{{82,20,25},{90,60,62},{95,78,78},{84,43,45}},{{78,60,64},{88,78,82},{94,90,95},{82,64,68}}};
 for(unsigned v=0;v<2;v++){cge.energy=CGE_VIBES[v].defaults[0];cge_gain_prepare();for(unsigned p=0;p<4;p++)for(unsigned k=0;k<3;k++)CHECK(cge.profile[v][p][k]==gains[v][p][k]*32768/100);}
 FILE *f=fopen("build/g1/catalog.csv","w");CHECK(f);if(f)fprintf(f,"vibe,scale,total,accepted,meter,shape,scale_mismatch,zero_weight,invalid_plan\n");
 cf_chord_cfg_t cfg=cs.chord;unsigned colored=0,slash=0;
 for(unsigned v=0;v<2;v++)for(unsigned scale=0;scale<CF_NSCALES;scale++){
  cfg.scale=scale;unsigned counts[CGE_NREASONS];uint16_t id=0;int ok=cge_choose(v,cfg,0,&id,counts);unsigned total=0;
  for(unsigned i=0;i<CGE_NREASONS;i++)total+=counts[i];CHECK(total==130 && ok==(counts[CGE_ACCEPT]!=0));
  if(f)fprintf(f,"%s,%u,%u,%u,%u,%u,%u,%u,%u\n",CGE_VIBES[v].name,scale,total,counts[CGE_ACCEPT],counts[0],counts[1],counts[2],counts[3],counts[4]);
  if(ok){int at=cge_find(id);CHECK(at>=0 && cf_progression_get(at)->id==id);CHECK(cf_progression_get(at)->scale==scale);}
  for(unsigned i=0;i<130;i++){
   unsigned reason;if(!cge_eligible(i,v,cfg,&reason))continue;
   const cf_progression_t *p=cf_progression_get(i);const cf_source_chord_t *source=cf_progression_source(p);if(!source)continue;
   cpa_plan_t plan;CHECK(cge_build(&plan,p->id,cfg,cge_rhythm[v][0],CPA_BOTH));
   for(unsigned j=0;j<8;j++){CHECK(plan.slot[j].mask==source[p->n==3 && j%4==3?2:j%4].mask);colored++;
    unsigned at=p->n==3 && j%4==3?2:j%4;if(source[at].bass>=0){CHECK(plan.slot[j].bass_pc==(plan.slot[j].root%12+source[at].bass)%12);slash++;}}
   cpa_plan_t saved=cpa.plan;cge_take_t take=cge.current;cf_chord_cfg_t old_cfg=cs.chord;
   cpa.plan=plan;cge.current.id=p->id;cs.chord=cfg;cf_song_t song;CHECK(cge_snapshot(&song));
   for(unsigned j=0;j<8;j++){cf_song_chord_t o;CHECK(cf_song_resolve(&song,j,&cfg,0,0,&o));
    CHECK(o.n==plan.slot[j].n && !memcmp(o.note,plan.slot[j].note,o.n) && o.bass_pc==plan.slot[j].bass_pc);}
   cpa.plan=saved;cge.current=take;cs.chord=old_cfg;
  }
 }
 if(f)fclose(f);CHECK(colored>0);
 CHECK(slash>0); /* User-approved three-chord mapping includes source ID133. */
 cfg=cs.chord;
 for(unsigned v=0;v<2;v++){
  unsigned seen[130]={0},distinct=0;cge_streams(0x10203040);uint16_t previous=0;
  for(unsigned n=0;n<64;n++){uint16_t id;CHECK(cge_choose(v,cfg,previous,&id,0));CHECK(id!=previous);int at=cge_find(id);CHECK(at>=0);if(!seen[at]++){distinct++;}previous=id;}
  CHECK(distinct>=5);printf("G1_CATALOG,%s,distinct_ids=%u,colored_slots=%u,slash_slots=%u\n",CGE_VIBES[v].name,distinct,colored,slash);
 }
}
static void memory_and_vibe(void)
{
 start();ticks(6*384+57);cge_take_t before=cge.current;unsigned tonic=cs.chord.tonic,scale=cs.chord.scale,bpm=cs.bpm,arp=cge.arp_request;
 cge_regenerate(0);cge_take_t first=cge.next;CHECK(first.id!=before.id);cge_regenerate(0);cge_take_t latest=cge.next;
 ticks(10*384-57-1);CHECK(!memcmp(&before,&cge.current,sizeof before));ticks(1);CHECK(!memcmp(&latest,&cge.current,sizeof latest));
 CHECK(cpa.slot==0 && cge.bar==0);cge_regenerate(1);ticks(16*384);CHECK(!memcmp(&before,&cge.current,sizeof before));
 CHECK(cs.chord.tonic==tonic && cs.chord.scale==scale && cs.bpm==bpm && cge.arp_request==arp);
 ticks(6*384);cge_flow_turn(0,1);CHECK(cge.current.vibe==0 && cge.next.vibe==1 && cge.pending&CGE_VIBE);
 ticks(10*384-1);CHECK(cge.current.vibe==0);ticks(1);CHECK(cge.current.vibe==1 && !cge.pending);
 CHECK(cs.chord.tonic==tonic && cs.chord.scale==scale && cs.bpm==bpm && cge.arp_request==arp);
 ticks(6*384);cge_flow_turn(0,-1);cge_flow_turn(0,1);ticks(10*384);CHECK(cge.current.vibe==1);
 cge_regenerate(0);unsigned oldtone=cs.chord.tonic;cpa_home_key(1,1);ticks(768);CHECK(cpa.plan.tonic==(oldtone+1)%12);
 ticks(16*384);CHECK(cpa.plan.tonic==(oldtone+1)%12);
 ticks((16-cge.bar)*384);ticks(6*384);cge_flow_turn(0,-1);cpa_home_key(1,1);ticks(2*384);
 CHECK(cge.pending&CGE_VIBE);ticks((16-cge.bar)*384);CHECK(cge.current.vibe==0 && cpa.plan.tonic==(oldtone+2)%12);
}
static void pins_and_seed(void)
{
 start();cpa_stop();cge_take_t recorded=cge.current;cf_fx_t record_fx=cs.fx;cf_snd_t record_sound=cf_snd[0];
 cf_sn.sq_on=1;cf_sn.sq_rec=SQ_CHORDS;cge_flow_turn(0,1);cge_regenerate(0);
 CHECK(!memcmp(&recorded,&cge.current,sizeof recorded) && !cge.pending);
 CHECK(!memcmp(&record_fx,&cs.fx,sizeof record_fx) && !memcmp(&record_sound,&cf_snd[0],sizeof record_sound));
 cf_sn.sq_on=0;cf_sn.sq_rec=255;
 start();cge_take_t original=cge.current;uint32_t rng[CGE_NRNG];memcpy(rng,cge.rng,sizeof rng);
 cge.locks=CGE_SOUND|CGE_GROOVE|CGE_FX|CGE_FEEL;cge_regenerate(0);
 CHECK(cge.rng[1]==rng[1] && cge.rng[2]==rng[2] && cge.rng[3]==rng[3] && cge.rng[4]==rng[4] && cge.rng[5]==rng[5]);
 ticks(16*384);CHECK(cge.current.id!=original.id && cge.current.chord==original.chord && cge.current.bass==original.bass && cge.current.fx==original.fx && cge.current.groove==original.groove);
 cge_flow_turn(0,1);ticks(16*384);CHECK(cge.current.vibe==1 && !strcmp(cf_snd[0].name,"GHOST CHOIR") && DM_RHY[cpa.plan.rhythm].id==56);
 cge.locks=0;cge_knob_turn(1,10);CHECK(cge.locks&CGE_FEEL);unsigned feel=cge.feel;
 cge_flow_turn(1,1);cge_flow_turn(2,100);ticks(3*16*384);CHECK(cge.feel==feel);
 cge_seed(0);cpa_stop();cge_init();cs.beats=4;cs.chord.scale=CF_SC_LOCRIAN;cge_load();CHECK(!cge.loaded);
 CHECK(cs.chord.scale==CF_SC_LOCRIAN); /* no incompatible fallback changes the player scale */
 cge_init();cs.chord.scale=CF_SC_MAJOR;cs.chord.tonic=7;cs.bpm=137;cfs_loaded=1;
 cf_snd_t sound=cf_snd[0];cf_fx_t fx=cs.fx;unsigned groove=cpg.rhythm;cge.vibe_request=1;cge_load();
 CHECK(cge.loaded && cs.bpm==137 && cs.chord.tonic==7 && cpg.rhythm==groove);
 CHECK(!memcmp(&sound,&cf_snd[0],sizeof sound) && !memcmp(&fx,&cs.fx,sizeof fx));CHECK((cge.locks&14)==14);cfs_loaded=0;
}
static void bounded_evolution(void)
{
 start();cge.direction=cge.direction_request=CGE_JAM;cge.surprise=100;cge.locks=CGE_GROOVE|CGE_FX;cge_play_it(CGE_ARP_ON);cge_poll();CHECK(!cge.pending);
 cge_play_it(CGE_ARP_OFF);cge.feel=100;cge_poll();CHECK(cge.pending==CGE_FEEL && cge.next.feel==0);
 start();cf_post(CFE_KEY,7,0,1);blocks(8);uint8_t held[CF_MAX_NOTES];unsigned count=cfp.n;memcpy(held,cfp.note,count);
 cge_regenerate(0);ticks(16*384);blocks(64);CHECK(cpa_live_chord() && cfp.n==count && !memcmp(held,cfp.note,count));
 CHECK(!sqp.n && !cfa_sq[0].n);cf_post(CFE_KEY,7,0,0);blocks(8);CHECK(!cpa_live_chord());
 start();cge.direction=cge.direction_request=CGE_JAM;cge.surprise=100;cge.planned_cycle=UINT32_MAX;cge_poll();CHECK(cge.pending && cge.request_kind==CGE_EVOLVE);
 cge_flow_turn(1,-1);cge_flow_turn(2,-100);CHECK(cge.pending&CGE_DIRECTION);ticks(16*384);CHECK(cge.direction==CGE_REPEAT);
 start();cge.direction=cge.direction_request=CGE_JAM;cge.surprise=100;cge.planned_cycle=UINT32_MAX;
 cf_perf_queue(CFPERF_DROP);ticks(384);for(unsigned i=0;i<4;i++){ticks(16*384);CHECK(cf_moment.dropped);}
 cf_perf_queue(CFPERF_BACK);ticks(384);CHECK(!cf_moment.dropped);
 for(unsigned v=0;v<2;v++)for(unsigned surprise=0;surprise<=100;surprise+=25){
  start();if(v){cge_flow_turn(0,1);ticks(16*384);}cge.direction=cge.direction_request=CGE_JAM;cge.surprise=surprise;cge.planned_cycle=UINT32_MAX;
  uint16_t id=cge.current.id;unsigned changes=0;
  for(unsigned cycle=0;cycle<4;cycle++){
   cge_take_t previous=cge.current;cge_poll();unsigned pending=cge.pending;CHECK(!(pending&(CGE_HARMONY|CGE_SOUND|CGE_VIBE)));
   ticks(16*384);unsigned different=(cge.current.groove!=previous.groove)+(cge.current.feel!=previous.feel)+(cge.current.fx!=previous.fx);
   CHECK(different<=1 && cge.current.id==id);changes+=different;
   if(!surprise)CHECK(!different);
  }
  if(surprise>=50)CHECK(changes>=2);if(surprise && surprise<50)CHECK(changes==2);
 }
}
static void render_row(FILE *f,unsigned sample,unsigned st,unsigned n,unsigned v)
{
 char chord[48]={0},bass[24]={0};const cpa_slot_t *p=&cpa.plan.slot[cpa.slot];
 for(unsigned i=0;i<p->n;i++){char b[8];snprintf(b,sizeof b,"%s%u",i?" ":"",p->note[i]);strcat(chord,b);}
 for(unsigned i=0;i<p->nb;i++){char b[8];snprintf(b,sizeof b,"%s%u",i?" ":"",p->bass[i]);strcat(bass,b);}
 fprintf(f,"%u,%u,%u,%u,%s,%u,%u,%s,%s,%u,%u,%u,%u,%u,%s,%u,%u,%u,%u,%u,%u,%u,%u,%s,%u,%u,%u,%u,%u,%u\n",sample,cge.cycle,cge.bar+1,cpa.t,cge_stage_name(),cge.current.id,cge.session_seed,chord,bass,st,n,v,cpa.plan.rhythm<DM_N?DM_RHY[cpa.plan.rhythm].id:65535,cge.arp_applied,cge_articulation_name(),cs.arp.mode,cge.feel_applied,cf_snd[0].engine,cf_snd[0].fk,cf_snd[1].engine,cf_snd[1].fk,cfx_identify(&cs.fx),cfs_fnv(&cs.fx,sizeof cs.fx),CGE_VIBES[cge.current.vibe].name,cge.direction,cge.surprise,cge.locks,cge.pending,cge.request_ms,cge.commit_ms);
}
static void render_events(FILE *f,unsigned sample,unsigned held[2][128],unsigned *overlap,unsigned *clocks)
{
 if(cge.sound_taken || cge.fx_taken)cge_sound_poll();
 while(mo_r!=mo_w){uint32_t q=midi_out_q[mo_r++%MQ];unsigned st=q>>8&255,n=q>>16&127,v=q>>24&127,ch=st&15;
  if(st==0xf8)(*clocks)++;
  if((st&0xf0)!=0x90 && (st&0xf0)!=0x80)continue;
  render_row(f,sample,st,n,v);
  if(ch<2){if((st&0xf0)==0x90 && v){*overlap+=held[ch][n];held[ch][n]=1;}else held[ch][n]=0;}
 }
}
static void render(int argc,char **argv)
{
 unsigned vibe=atoi(argv[2]),direction=atoi(argv[3]),surprise=atoi(argv[4]),seed=strtoul(argv[5],0,0),capture=atoi(argv[7]),rate=atoi(argv[8]);
 unsigned interactive=argc>9?atoi(argv[9]):0;
 cpa_stop();sqp_stop();dmp_stop();cfp_panic();cf_boot_go=0;cfb.on=0;cf_intro_on=0;blocks(4);cge_init();cge_seed(seed);cfs_loaded=0;
 cu.scr=SCR_HOME;home_boot_main=1;cf_home_bank=CF_HB_ENGINE;cs.chord.tonic=vibe?0:4;cs.chord.scale=CF_SC_MAJOR;cs.chord.octave=0;cs.beats=4;cs.bpm=CGE_VIBES[vibe].bpm;cs.bass_on=1;cs.clock_in=2;cu.arp_on=0;
 cge.vibe_request=vibe;cge_flow_turn(1,direction==CGE_JAM?1:-1);cge.surprise=surprise;cge.flow_manual|=2;cge_load();blocks(64);
 if(interactive==2){cge.arp_request=CGE_ARP_ON;cs.arp.mode=CF_ARP_RAND;cs.arp.gate=70;cs.arp.rate=4;cf_send_config();blocks(1);}
 ua_reset();ua.cap_alt=capture;ua.cap_hold=0;ua_rate_set(rate);usb.up=usb.config=1;cfp_cfg.midi_out=1;mo_r=mo_w;
 char file[256];snprintf(file,sizeof file,"%s.raw",argv[6]);FILE *raw=fopen(file,"wb");snprintf(file,sizeof file,"%s.csv",argv[6]);FILE *f=fopen(file,"w");CHECK(raw && f);if(!raw || !f)return;
 fprintf(f,"sample,cycle,bar,tick,stage,progression_id,seed,chord_notes,bass_notes,status,note,velocity,groove_id,arp_override,arp_effective,arp_mode,feel,chord_engine,chord_factory,bass_engine,bass_factory,fx_id,fx_hash,vibe,direction,surprise,pins,pending,request_ms,commit_ms\n");
 cpa_ui_play();while(cf_evq_r!=cf_evq_w)cf_apply(&cf_evq[cf_evq_r++%CF_EVQ]);CHECK(cge_active());
 unsigned samples=0,held[2][128]={{0}},overlap=0,clocks=0,last=999,last_bar=999,peak=0,clips=0;int32_t out[CTL*2];uint16_t initial_id=cge.current.id;double squares=0;
 while(cge.cycle<4){
  unsigned position=cge.cycle*16+cge.bar;
  if(interactive==1 && position!=last){last=position;
   if(position==6)cge_flow_turn(0,vibe?-1:1);
   if(position==17)cge.arp_request=CGE_ARP_ON;
   if(position==20)cge.arp_request=CGE_ARP_OFF;
   if(position==23)cge.arp_request=CGE_ARP_AUTO;
   if(position==25)cge_regenerate(0);
   if(position==33)cf_perf_queue(CFPERF_DROP);
   if(position==36)cge_regenerate(1);
   if(position==44)cf_perf_queue(CFPERF_BACK);
  }
  render_events(f,samples,held,&overlap,&clocks);
  if(position!=last_bar){last_bar=position;render_row(f,samples,0,0,0);}
  if(!interactive)CHECK(cge.current.id==initial_id);
  audio_block(out,CTL);fwrite(out,sizeof out,1,raw);samples+=CTL;ua.cr=ua.cw;
  for(unsigned i=0;i<CTL*2;i++){int x=out[i]>>8;unsigned a=x<0?-x:x;if(a>peak)peak=a;clips+=a>=32767;squares+=(double)x*x;}
  if(samples%(8*CTL)==0){cf_snapshot();cpa_ui_poll();cge_poll();if(cge_active() && interactive==3)cf_ui_draw();}
 }
 render_events(f,samples,held,&overlap,&clocks);cpa_stop();render_events(f,samples,held,&overlap,&clocks);
 unsigned stale=0;for(unsigned c=0;c<2;c++)for(unsigned n=0;n<128;n++)stale+=held[c][n];
 CHECK(!overlap && !stale && !clips && peak>100);CHECK(fabs((double)samples/FS-256*60.0/CGE_VIBES[vibe].bpm)<2.0*CTL/FS);
 CHECK(clocks>=6144 && clocks<=6145);
 fclose(f);fclose(raw);printf("G1_RENDER,%s,%s,samples=%u,clocks=%u,overlap=%u,stale=%u,peak=%u,rms=%.3f\n",CGE_VIBES[vibe].name,CGE_DIRECTION_NAME[direction],samples,clocks,overlap,stale,peak,sqrt(squares/(samples*2)));
}
static void screenshot(const char *name)
{
 cf_snapshot();cpa_ui_poll();cge_poll();cu.pop=0;cf_intro_on=0;cf_motion=0;fb_full=1;cf_ui_draw();
 char path[128];snprintf(path,sizeof path,"build/g1/%s.ppm",name);FILE *f=fopen(path,"wb");CHECK(f);if(!f)return;
 fprintf(f,"P6\n240 240\n255\n");for(unsigned i=0;i<FB_W*FB_H;i++){uint32_t rgb=cf_pal[fb[i]];uint8_t p[]={rgb>>16,rgb>>8,rgb};fwrite(p,1,3,f);}fclose(f);
}
static void ui_images(void)
{
 start();ticks(6*384);screenshot("haze-home");
 CHECK(cf_stage_last.engine_bar==7 && strstr(cf_stage_last.applied,"DreamPop") && strstr(cf_stage_last.applied,"Wide Drift"));
 ui_knob_turn(1,65);screenshot("feel-queued");
 CHECK(!strcmp(cf_stage_last.pending,"At bar 9: Feel 100%") && strstr(cf_stage_last.notes,"Feel 35%"));
 ticks(768);screenshot("feel-applied");
 CHECK(!cf_stage_last.pending[0] && strstr(cf_stage_last.notes,"Feel 100%") && cf_stage_last.focus==1);
 ui_button(B_HOME);CHECK(cf_home_bank==CF_HB_FLOW);
 ui_knob_turn(0,1);screenshot("vibe-queued");
 CHECK(!strcmp(cf_stage_last.pending,"At bar 1: Drive") && strstr(cf_stage_last.applied,"DreamPop"));
 ticks(8*384);screenshot("drive-flow");
 CHECK(!strcmp(cf_stage_last.next_setup,"Vibe applied") && cf_stage_last.engine_bar==1);
 ui_algo(-1);CHECK(cf_home_bank==CF_HB_ENGINE);
 ui_knob_turn(3,-1);screenshot("back-queued");
 CHECK(!strcmp(cf_stage_last.pending,"At bar 1: Back One"));
 ticks(16*384);screenshot("back-applied");
 CHECK(!strcmp(cf_stage_last.next_setup,"Back One applied") && cge.current.vibe==0);
 ui_button(B_ARP);ui_knob_turn(0,1);screenshot("arp-queued");ticks(768);screenshot("arp-on");
 ui_button(B_HOME);CHECK(cf_home_bank==CF_HB_ENGINE && cge_active());
 ui_button(B_SCL);CHECK(cu.scr==SCR_LAB && cge_active());
 ui_button(B_HOME);CHECK(cf_home_bank==CF_HB_ENGINE && cge_active());
 ui_button(B_HOME);cge.locks=CGE_FEEL|CGE_GROOVE|CGE_FX;
 ui_knob_turn(1,1);ticks(16*384);screenshot("jam-pinned");
 CHECK(!strcmp(cf_stage_last.pending,"Jam blocked by pins"));
 ui_button(B_SCL);CHECK(cge_pins_open && cge.locks==(CGE_FEEL|CGE_GROOVE|CGE_FX));
 ui_knob_turn(3,1);ui_knob_turn(3,1);screenshot("pins");
 CHECK(cf_stage_last.engine_page==2 && cf_stage_last.focus==2 && cf_stage_last.pin_mask&CGE_GROOVE);
 ui_knob_turn(0,1);CHECK(cge.vibe_request==0);
 ui_button(B_SCL);screenshot("groove-released");
 CHECK(cf_stage_last.engine_page==2 && !(cf_stage_last.pin_mask&CGE_GROOVE) && cge_jam_choices()==CGE_GROOVE);
 ui_button(B_HOME);CHECK(!cge_pins_open && cf_home_bank==CF_HB_FLOW && cge_active());
 ui_knob_turn(3,-1);CHECK(cge_pins_open && cge.pin==2);
 ui_button(B_HOME);CHECK(!cge_pins_open);
}
static void play_from_flow(void)
{
 for(unsigned vibe=0;vibe<2;vibe++){
  cpa_stop();sqp_stop();dmp_stop();cfp_panic();blocks(4);cge_init();cge_seed(1234);cfs_loaded=0;cs_defaults(&cs);cu.arp_on=0;
  cu.scr=SCR_HOME;home_boot_main=1;cf_home_bank=CF_HB_FLOW;cf_scene_set(CF_SCENE_PROGRESSIONS);
  if(vibe)cge_flow_turn(0,1);ui_button(B_PLAY);blocks(48);
  CHECK(cge_active() && cge.current.vibe==vibe && cge.bar==0 && dmp.on);
  CHECK(cs.bpm==CGE_VIBES[vibe].bpm && cs.chord.tonic==(vibe?0:4));
  CHECK(DM_RHY[cpa.plan.rhythm].id==CGE_VIBES[vibe].groove[0] && sqp.n);
  voice_t *voice=trk_voice_of(&trk[0],sqp.note[0]);CHECK(voice && voice->gate);
  ui_button(B_PLAY);blocks(2);CHECK(!cpa.on);
 }
}
static void manual_progression_ui(void)
{
 start();ticks(6*384);
 cge.locks=CGE_HARMONY;
 cge_take_t before=cge.current;
 char title[40];str_cpy(title,cge_view.name,sizeof title);
 ui_select(1);CHECK(cge_preview<cf_progression_count());
 unsigned chosen=cf_progression_get(cge_preview)->id;
 screenshot("progression-preview");
 CHECK(!strcmp(cf_stage_last.idea,title) && strstr(cf_stage_last.pending,"Preview: "));
 CHECK(!strcmp(cf_stage_last.next_setup,"SEL queues for bar 1") && !cge.pending);
 cf_home_bank=CF_HB_CORE;
 screenshot("progression-core-preview");
 CHECK(!strcmp(cf_stage_last.idea,title) && cf_stage_last.engine_bar==7 && strstr(cf_stage_last.pending,"Preview: "));
 ui_button(B_SCL);
 CHECK(cge_active() && cge.pending==CGE_HARMONY && !cpa_setup.flags && cge.next.id==chosen);
 unsigned tonic=cpa.plan.tonic,bpm=cs.bpm,scale=cpa.plan.scale;
 ticks(10*384-1);CHECK(cge.current.id==before.id);
 ticks(1);screenshot("progression-core-applied");
 CHECK(cge_active() && cge.current.id==chosen && !strcmp(cf_stage_last.idea,cge_progression_name(chosen)));
 CHECK(cge.current.groove==before.groove && cge.current.chord==before.chord && cge.current.fx==before.fx);
 CHECK(cpa.plan.tonic==tonic && cs.bpm==bpm && cpa.plan.scale==scale && (cge.locks&CGE_HARMONY));
 cf_home_bank=CF_HB_ENGINE;screenshot("progression-applied");
 CHECK(!strcmp(cf_stage_last.idea,cge_progression_name(chosen)) && !strcmp(cf_stage_last.next_setup,"Progression applied"));
}
int main(int argc,char **argv)
{
 emu_fw_options(NULL,1,0,1);emu_hal.ready=2;emu_fw_init(0);
 if(argc>=9 && !strcmp(argv[1],"--render"))render(argc,argv);
 else {
  CHECK(cf_home_bank==CF_HB_ENGINE);
  screenshot("jam-ready");CHECK(cf_stage_last.engine_page && !strcmp(cf_stage_last.next_setup,"Press PLAY to start"));
  catalog();memory_and_vibe();pins_and_seed();bounded_evolution();play_from_flow();ui_images();manual_progression_ui();
 }
 printf("RAKT G1: %u checks, %u failures\n",checks,failures);return failures!=0;
}
