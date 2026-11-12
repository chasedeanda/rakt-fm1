/* SPDX-License-Identifier: GPL-3.0-only
 * G1: bounded shared-clock consumer; selection/preparation stays in foreground. */
#include "cf_engine_data.h"
enum { CGE_HARMONY=1, CGE_SOUND=2, CGE_GROOVE=4, CGE_FX=8 };
enum { CGE_FEEL=16, CGE_VIBE=32, CGE_DIRECTION=64 };
enum { CGE_REPEAT, CGE_JAM };
enum { CGE_R_HARMONY, CGE_R_GROOVE, CGE_R_CHORD, CGE_R_BASS, CGE_R_FX, CGE_R_FEEL, CGE_NRNG };
enum { CGE_ARP_AUTO, CGE_ARP_ON, CGE_ARP_OFF };
enum { CGE_FRESH=1, CGE_EVOLVE, CGE_BACK, CGE_PICK };
enum { CGE_CHANGE_FRESH=1, CGE_CHANGE_BACK, CGE_CHANGE_VIBE, CGE_CHANGE_DIRECTION,
       CGE_CHANGE_GROOVE, CGE_CHANGE_FEEL, CGE_CHANGE_FX, CGE_CHANGE_PICK };
static struct {
    uint8_t loaded, energy, feel, space, locks;
    cge_take_t current,next,previous;
    uint8_t pending,undo,direction,direction_request,surprise,vibe_request,pin,request_kind;
    uint8_t tempo_pin,key_pin,initial_pins,requeue,flow_manual;
    uint8_t last_change;
    uint8_t bar, arp, bass_gate, drum_gate, sound_taken, fx_taken;
    uint8_t feel_applied, arp_request, arp_applied, energy_applied;
    cf_arp_t manual_arp;
    uint16_t profile[2][4][3], gain[3], target[3], last_target[3], ramp_left[3];
    uint32_t cycle,generation,session_seed,rng[CGE_NRNG],planned_cycle,request_ms,commit_ms;
    cf_fx_t fx;
} cge;
static unsigned cge_change(unsigned flags)
{
    if(cge.request_kind==CGE_BACK && (flags&~CGE_DIRECTION))return CGE_CHANGE_BACK;
    if(cge.request_kind==CGE_PICK && (flags&CGE_HARMONY))return CGE_CHANGE_PICK;
    if(flags&CGE_VIBE)return CGE_CHANGE_VIBE;
    if(!(flags&~CGE_DIRECTION))return CGE_CHANGE_DIRECTION;
    if(cge.request_kind!=CGE_EVOLVE)return CGE_CHANGE_FRESH;
    if(flags&CGE_GROOVE)return CGE_CHANGE_GROOVE;
    if(flags&CGE_FEEL)return CGE_CHANGE_FEEL;
    return CGE_CHANGE_FX;
}
/* One bounded prepared phrase, not an audio buffer or second sequencer. Keeping
 * it separate lets manual Core edits retain their existing chord/bar queue. */
static cpa_plan_t cge_prepared_plan;
static int cge_active(void){return cpa.on && cpa.plan.idea==CF_P_ENGINE;}
static void cge_live_takeover(void)
{
    if(cge_active() && cfp_cp==0 && !cfp_bo){cpa_arp_off();sqp_chord_off();sqp_bass_off();}
    if(cgs_active()){
        if(!cfp_bo)cgs_release(cfp_cp);
        if(cfp_bo || (cfp_cp==CGS_CHORDS && !cfp_studio_keys))cgs_release(CGS_BASS);
    }
}
static int cge_arp_wanted(void){return cge_active()?cge.arp:cfp_cfg.arp_on;}
/* Only the scheduled owner consumes this state, at a two-bar chord onset. */
static void cge_articulation(void)
{
    cge.feel_applied=cge.feel;cge.arp_applied=cge.arp_request;cge.current.feel=cge.feel;
    cge.manual_arp=cfp_cfg.arp;
    cge.arp=cge.arp_applied==CGE_ARP_ON ||
        (cge.arp_applied==CGE_ARP_AUTO && cge.feel_applied>=65);
}
static int cge_gated(void)
{return cge_active() && !cge.arp && cge.feel_applied>=25;}
static int cge_chord_gap(void)
{return cge_gated() && cpa.elapsed%192u>=192u*70u/100u;}
static void cge_tick1(void)
{
    if(!cge_gated() || cpa_live_chord() || cla_active() || cpa.plan.mode==CPA_DRUMS)return;
    unsigned t=cpa.elapsed%192u;
    if(t==192u*70u/100u)sqp_chord_off();
    else if(!t && cpa.elapsed){
        const cpa_slot_t *s=&cpa.plan.slot[cpa.slot];
        sqp_chord_off();
        for(unsigned i=0;i<s->n;i++){
            sqp.note[i]=s->note[i];
            if(!cfp_holds(0,s->note[i]))cfp_on(0,s->note[i],cfp_cfg.vel);
        }
        sqp.n=s->n;
    }
}
static int cge_bass_enabled(void){return !cge_active() || cge.bass_gate;}
static int cge_drums_enabled(void){return !cgs_active() && (!cge_active() || cge.drum_gate);}
static unsigned cge_phase(void)
{return cge.bar/4u;}
static void cge_targets(void)
{
    unsigned p=cge_phase();
    for(unsigned k=0;k<3;k++)cge.target[k]=cge.profile[cge.current.vibe][p][k];
}
static void cge_gates(void)
{
    unsigned p=cge_phase();cge.energy_applied=cge.energy;
    /* Energy owns gain, not the clocked note owners. Turning up from zero
     * must recover the current bed/groove without waiting for another bar. */
    cge.bass_gate=CGE_VIBES[cge.current.vibe].gain[p][1]!=0;
    cge.drum_gate=CGE_VIBES[cge.current.vibe].gain[p][2]!=0;
}
static void cge_start(void)
{
    if(!cge_active())return;
    cge.bar=0;cge.cycle=0;cge.planned_cycle=UINT32_MAX;cge_articulation();
    cge.last_change=0;
    cfa_sq[0].rng=cge.session_seed^0xC0FD0001u;cfa_sq[0].rnd_last=255;
    cge_targets();cge_gates();
    for(unsigned k=0;k<3;k++)cge.gain[k]=cge.target[k];
}
static void cge_stop(void)
{cge.arp=0;cge.pending=0;cge.requeue=0;cge.next=cge.current;cge.direction_request=cge.direction;
 for(unsigned k=0;k<3;k++)cge.target[k]=32768;}
/* Uses the existing staged sound loader; defined after cf_sound.c. */
static int cge_sound_boundary(void);
static int cge_bar(int onset)
{
    if(!cge_active())return 0;
    cge.bar=(cge.bar+1u)%16u;if(!cge.bar)cge.cycle++;
    if(!cge.bar){cfa_sq[0].rng=cge.session_seed^0xC0FD0001u;cfa_sq[0].rnd_last=255;}
    int sound=0;
    if(!cge.bar && cge.pending){
        unsigned flags=cge.pending;
        if(flags&~CGE_DIRECTION){cge.previous=cge.current;cge.undo=1;}
        if(flags&CGE_HARMONY){
            cge.current.id=cge.next.id;
            for(unsigned i=0;i<8;i++){cpa.plan.slot[i]=cge_prepared_plan.slot[i];cpa.plan.degree[i]=cge_prepared_plan.degree[i];}
            cpa.plan.tonic=cge_prepared_plan.tonic;cpa.plan.scale=cge_prepared_plan.scale;cpa.plan.nb=cge_prepared_plan.nb;
        }
        if(flags&CGE_SOUND){sound=cge_sound_boundary();cge.current.chord=cge.next.chord;cge.current.bass=cge.next.bass;}
        if(flags&CGE_GROOVE){cge.current.groove=cge.next.groove;cpa.plan.rhythm=cge_prepared_plan.rhythm;
            cpa.plan.kit=cge_prepared_plan.kit;cpa.plan.swing=cge_prepared_plan.swing;}
        if(flags&CGE_FX){cfx_target=cge.fx;cfx_pending=cfx_ready=1;cge.current.fx=cge.next.fx;cge.fx_taken=1;}
        if(flags&CGE_FEEL)cge.feel=cge.next.feel;
        if(flags&CGE_VIBE)cge.current.vibe=cge.next.vibe;
        if(flags&CGE_DIRECTION)cge.direction=cge.direction_request;
        /* These plans retain the player's committed key/scale/meter. Publish
         * the new phrase identity without rebuilding the live-key chord model. */
        if(flags&(CGE_HARMONY|CGE_GROOVE))cpa.revision++;
        cge.last_change=cge_change(flags);
        cge.pending=0;cge.generation++;cge.commit_ms=cf_now_ms; /* telemetry only */
    }
    unsigned bass=cge.bass_gate;cge_targets();cge_gates();
    if(!onset && bass!=cge.bass_gate){
        const cpa_slot_t *s=&cpa.plan.slot[cpa.slot];
        if(!cge.bass_gate)sqp_bass_off();
        else if(!cpa_live_chord() && cpa.plan.mode!=CPA_DRUMS)
            sqp_bass_on(s->bass,cfp_cfg.bass_on?s->nb:0,s->ticks-cpa.elapsed,0);
    }
    if(!(cge.bar&1u)) {
        cge_articulation();
    }
    return sound;
}
static void cge_block(uint32_t n)
{
    if(sqp.on && sq_perf[sqp.pat].bars){
        cfx_space=sq_perf[sqp.pat].space*32768u/100u;
        return;
    }
    if(cgs_mix_live()){
        cfx_space=cgs.space*32768u/100u;
        cgs_audio_block(n);
        return;
    }
    cfx_space=cge_active()?cge.space*32768u/100u:32768u;
    if(cge_active())cge_targets();else for(unsigned k=0;k<3;k++)cge.target[k]=32768;
    /* Every changed target gets its own linear 50ms control-rate ramp. */
    for(unsigned k=0;k<3;k++){
        if(cge.last_target[k]!=cge.target[k]){cge.last_target[k]=cge.target[k];cge.ramp_left[k]=FS/20u;}
        unsigned left=cge.ramp_left[k],take=n<left?n:left;
        if(left){int delta=(int)cge.target[k]-cge.gain[k];cge.gain[k]=(uint16_t)((int)cge.gain[k]+delta*(int)take/(int)left);cge.ramp_left[k]-=take;}
        else cge.gain[k]=cge.target[k];
    }
}
static uint32_t cge_mix_gain(unsigned part)
{
    if(part<SQ_NTRK && sqp.on && sq_perf[sqp.pat].bars)
        return sq_perf[sqp.pat].level[part]*32768u/100u;
    if(part<CGS_NPART && cgs_mix_live())return cgs.gain[part];
    if(part==0 || part==1)return cge.gain[part];
    if(part==CF_PART_DRUM)return cge.gain[2];
    return 32768;
}
/* Arp override is scoped to the scheduled chord owner; live ARP stays independent. */
static cf_arp_t cge_arp_save;
static uint8_t cge_arp_save_on;
static int cge_arp_push(void)
{
    if(!cge_active())return 0;
    cge_arp_save=cfp_cfg.arp;cge_arp_save_on=cfp_cfg.arp_on;
    cfp_cfg.arp_on=cge.arp;
    if(cge.arp_applied==CGE_ARP_ON)cfp_cfg.arp=cge.manual_arp;
    else {
        cfp_cfg.arp.rate=4;cfp_cfg.arp.oct=1;cfp_cfg.arp.gate=70;cfp_cfg.arp.latch=0;
        if(CF_ARP_STRUMMING(cfp_cfg.arp.mode))cfp_cfg.arp.mode=CF_ARP_UP;
    }
    return 1;
}
static void cge_arp_pop(int pushed)
{if(pushed){cfp_cfg.arp=cge_arp_save;cfp_cfg.arp_on=cge_arp_save_on;}}
