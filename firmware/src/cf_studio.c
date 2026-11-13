/* SPDX-License-Identifier: GPL-3.0-only
 * Studio uses the accompaniment clock and sequencer note owners. Each clip is
 * two bars of prepared rhythmic events; pitch roles follow the committed plan.
 */
enum { CGS_CHORDS, CGS_BASS, CGS_LAYER, CGS_DRUMS, CGS_NPART };
enum { CGS_ANCHOR=1, CGS_THIRD, CGS_FIFTH, CGS_OCTAVE, CGS_PICKUP, CGS_UPPER, CGS_STEP_IN };
enum { CGS_PATCH_SOLO=250, CGS_PATCH_ROLL, CGS_PATCH_STAGE };
enum { CGS_STEPS=16, CGS_TICKS=SQ_PPQN/4, CGS_BAR=4*SQ_PPQN };
enum { CGS_KICK=1, CGS_SNARE=2, CGS_HAT=4, CGS_OPEN=8, CGS_RIM=16, CGS_LOW_TOM=32, CGS_MID_TOM=64 };
enum { CGS_KICK_REPLY=10, CGS_FOLLOW, CGS_ANSWER, CGS_FLOAT };
typedef struct {
    uint8_t pocket;
    uint8_t cell[5][3]; /* sixteenth, pitch role, length in units of six ticks */
} cgs_riff_t;
#include "cf_studio_profiles.h"

enum { CGS_MAX_EDITS=4 };
enum { CGS_EDIT_ACCENT, CGS_EDIT_CHORD_GATE, CGS_EDIT_HAT, CGS_EDIT_DROP,
       CGS_EDIT_ADD, CGS_EDIT_BASS_ROLE, CGS_EDIT_BASS_LATER };
typedef struct { uint8_t kind, step, value; } cgs_edit_t;
typedef struct {
    uint8_t count;
    cgs_edit_t item[CGS_MAX_EDITS];
} cgs_edits_t;

typedef struct {
    uint8_t event[2][CGS_STEPS]; /* pitch role and gate, or drum lane bits */
    uint8_t voice;
    uint8_t feel[2][CGS_STEPS];  /* velocity trim in high bits, onset delay in low bits */
    uint32_t seed;             /* repeat the musical choices when shaping the clip */
    cgs_edits_t edits;           /* operations on the second bar of the seed phrase */
    uint8_t shape_energy, responds, backbeat[2], arp_pattern; /* zero, or a genre ARP mode/rate/span */
    int8_t transpose;
    uint8_t response_timing[2][CGS_STEPS]; /* Chord openings or planned kick onsets */
} cgs_clip_t;
typedef struct { uint8_t note[CF_P_MAX][7]; } cgs_pitches_t;
static struct {
    cgs_pitches_t pitch, setup_pitch, edit_pitch;
    cgs_clip_t active[CGS_NPART], next[CGS_NPART], take[CGS_NPART];
    uint32_t seed[CGS_NPART], harmony_seed, last_tick, developed, bar, origin_bar;
    uint32_t develop_seed[CGS_NPART], next_develop_seed[CGS_NPART], take_revision;
    uint8_t style[CGS_NPART], activity[CGS_NPART], level[CGS_NPART], touch[CGS_NPART];
    uint8_t gate[CGS_NPART], muted, mute_applied, focus, zoom, space, ready, mix_idle;
    uint8_t defaults_offered, boot_sound_gen[3];
    uint8_t genre, queued_genre, playing_genre, energy, evolution, solo, kit_pinned, feel, queued_feel, playing_feel, meter;
    uint8_t custom, bank_meter;
    uint8_t queued_bpm, shuffled_bpm;
    volatile uint8_t tempo_generation;
    volatile uint8_t pending;
    uint8_t pending_auto, hold, clear_take_hold, held_reply;
    uint8_t bass_pull, bass_event, bass_feel, bass_attack, section;
    uint8_t held_backbeat[2], punk_fill_step, harmony_root, harmony_changed;
    uint16_t harmony_mask;
    uint8_t held_gaps[2][CGS_STEPS], held_kicks[2][CGS_STEPS];
    uint8_t generation[CGS_NPART], harmony_generation, arp, arp_auto, arp_manual, held_solo;
    volatile uint8_t drum_mutes, fill_held;
    uint16_t gain[CGS_NPART];
} cgs;
static void cgs_sound_boundary(unsigned tag);
static void cgs_cancel_rolled_sounds(void);
static int cgs_rolled_patch(unsigned tag)
{ return tag==CGS_PATCH_ROLL || tag==CGS_PATCH_SOLO; }
static int cgs_studio_patch(unsigned tag)
{ return cgs_rolled_patch(tag) || tag==CGS_PATCH_STAGE; }
static int cgs_active(void)
{ return CF_STUDIO && cpa.on && cpa.plan.studio; }
static unsigned cgs_rand(uint32_t *seed, unsigned limit)
{
    uint32_t x=*seed;
    x^=x<<13; x^=x>>17; x^=x<<5;
    *seed=x;
    return x%limit;
}
static unsigned cgs_band_arp(unsigned part)
{
    unsigned bit=1u<<part;
    return (cgs.arp&bit) || (!cgs.arp && (cgs.arp_auto&bit) && cgs.active[part].arp_pattern);
}
static void cgs_arp_settings(cf_arp_t *arp,unsigned pattern)
{
    arp->mode=pattern&15u;
    arp->rate=(pattern>>4)&7u;
    arp->oct=1u+(pattern>>7);
    arp->latch=0;
}
/* The scheduled arpeggiator borrows its musician's settings for this call. */
static int cgs_arp_push(unsigned part,cf_arp_t *saved)
{
    unsigned pattern=cgs.active[part].arp_pattern;
    if(!cgs_active() || !(cgs.arp_auto&(1u<<part)) || !pattern)return 0;
    *saved=cfp_cfg.arp;
    cgs_arp_settings(&cfp_cfg.arp,pattern);
    return 1;
}
static void cgs_release(unsigned part)
{
    if(part==CGS_CHORDS){sqp_arp_off(part);sqp_chord_off();}
    else if(part==CGS_BASS)sqp_bass_off();
    else if(part==CGS_LAYER){sqp_arp_off(part);sqp_layer_off();}
    else sqp_drum_offs();
    cgs.gate[part]=0;
}
static void cgs_midi_takeover(unsigned part)
{
    if(cgs_active())cgs_release(part);
}
static int cgs_manual(unsigned part)
{
    return (part<2 && cr_mheld[part]) ||
        (part==cfp_cp && (cfp.n || cfa_k.n)) ||
        (part==CGS_BASS && cfp.nb);
}
static unsigned cgs_mute_mask(void)
{
    unsigned solo=cgs.solo|cgs.held_solo;
    return cgs.muted|(solo?(15u&~solo):0u);
}
/* Called by playback or with the foreground IRQ guard held. */
static void cgs_cancel_auto(unsigned mask)
{
    unsigned cancel=cgs.pending_auto&mask;
    cgs.pending&=(uint8_t)~cancel;
    cgs.pending_auto&=(uint8_t)~cancel;
}

/* Called at the bar boundary, after a queued take has set its new origin. */
static unsigned cgs_section_bar(void)
{
    const uint8_t *form=CGS_PROFILES[cgs.playing_genre].form;
    unsigned section=form?form[(cgs.bar-cgs.origin_bar)%CGS_FORM_BARS]:0;
    if(cf_moment.pending || cf_moment.dropped || cf_moment.fill==CFPERF_FILL_MANUAL)
        section&=~CGS_FORM_LIGHT_HATS;
    cgs.section=(uint8_t)section;
    return section&CGS_FORM_FILL;
}

static void cgs_start(void)
{
    cgs_cancel_auto((1u<<CGS_NPART)-1u);
    cgs.take_revision++;
    cgs.last_tick=UINT32_MAX;
    memset(cgs.gate,0,sizeof cgs.gate);
    cgs.developed=cgs.bar=cgs.origin_bar=0;
    cgs.section=0;
    cgs.harmony_mask=0;
    cgs.mute_applied=(uint8_t)cgs_mute_mask();
    for(unsigned p=0;p<CGS_NPART;p++)cgs.gain[p]=cgs.level[p]*32768u/100u;
}
static int cgs_voicing_shift(const cpa_slot_t *s,int shift)
{ return s->note[0]+shift>=0 && s->note[s->n-1u]+shift<=127?shift:0; }

static void cgs_common_chord(const cpa_slot_t *s, unsigned velocity)
{
    uint8_t previous[6], n=sqp.n;
    int shift=cgs_voicing_shift(s,cgs.active[CGS_CHORDS].transpose);
    memcpy(previous,sqp.note,n);
    for(unsigned i=0;i<s->n;i++)sqp.note[i]=(uint8_t)(s->note[i]+shift);
    sqp.n=s->n;
    unsigned released=0;
    cfp_fade_released(CGS_CHORDS);
    for(unsigned i=0;i<n;i++){
        if(has(sqp.note,sqp.n,previous[i]))continue;
        sqp_off(CGS_CHORDS,previous[i]);
        if(released++)cfp_fade_note(CGS_CHORDS,previous[i]);
    }
    for(unsigned i=0;i<s->n;i++)
        if(!has(previous,n,sqp.note[i]))cfp_on(CGS_CHORDS,sqp.note[i],velocity);
}
static void cgs_line(unsigned part, unsigned note, unsigned velocity)
{
    uint8_t *held=part==CGS_BASS?&sqp.nb:&sqp.ln;
    uint8_t *pitch=part==CGS_BASS?sqp.bass:sqp.lnote;
    if(*held && pitch[0]!=note)cgs_release(part);
    if(!*held){
        if(part==CGS_BASS)cfp_bass_mono();
        else cfp_fade_released(part);
        pitch[0]=(uint8_t)note;
        *held=1;
        cfp_on(part,note,velocity);
    }
}
static unsigned cgs_velocity(unsigned part,unsigned velocity)
{
    if(part!=CGS_CHORDS)velocity=velocity*(50u+cgs.touch[part])/100u;
    return (unsigned)clamp(velocity,1,127);
}

static unsigned cgs_drum_group(unsigned lane)
{
    if(lane<2)return lane;
    return lane==3 || lane==4?2u:3u;
}

static void cgs_drums(unsigned hit, unsigned step,unsigned steps,unsigned accent)
{
    static const uint8_t lanes[]={0,1,3,4,8,5,6,7};
    static const uint8_t level[]={105,86,53,47,39,69,63,59};
    if(cf_moment.dropped)return;
    unsigned punch=cgs.fill_held, half=steps/2u;
    unsigned style=punch?cf_perf_kit_style(cpa.plan.kit):cf_moment.style;
    int sparse=CGS_PROFILES[cgs.playing_genre].sparse_drums && !punch && !cf_moment.fill;
    for(unsigned i=0;i<sizeof lanes;i++){
        unsigned l=lanes[i];
        if((cgs.drum_mutes>>cgs_drum_group(l))&1u)continue;
        if(punch ? !cf_perf_fill_hit(style,l,step%half,half) : !cf_perf_hit(l,step,steps,(hit>>i)&1u))continue;
        unsigned original=level[i]+accent+(!(step&3u)?7u:0u);
        unsigned v=punch?cf_perf_fill_level(style,l,step%half+half,steps,original):cf_perf_level(l,step,steps,original);
        if(sparse)v=v*2u/3u;
        sqp_drum_hit(l,cpa.plan.kit,cgs_velocity(CGS_DRUMS,v),95);
    }
}
static void cgs_arp(const cpa_slot_t *s,unsigned part)
{
    sq_ev_t chord={.n=s->n,.root=s->root,.mask=(uint16_t)s->mask,.btns=cf_harmony_event_buttons(s->mask)};
    int shift=cgs.active[part].transpose;
    if(part==CGS_LAYER && cgs.active[part].arp_pattern && (cgs.arp_auto&(1u<<part)))shift+=12;
    shift=cgs_voicing_shift(s,shift);
    for(unsigned i=0;i<s->n;i++)chord.note[i]=(uint8_t)(s->note[i]+shift);
    cf_arp_t saved;
    int generated=cgs_arp_push(part,&saved);
    cfa_sq[part==CGS_LAYER].generated=(uint8_t)generated;
    sqp_arp_on(part,&chord);
    if(generated)cfp_cfg.arp=saved;
}

static unsigned cgs_steps(unsigned meter)
{ return meter==4u?CGS_STEPS:12u; }

static unsigned cgs_step_ticks(unsigned meter)
{ return meter==6u?CGS_TICKS*2u:CGS_TICKS; }

enum { CGS_REPLY_LATE_STEPS=3, CGS_REPLY_CLEARANCE=2, CGS_MAX_ONSET_DELAY=5,
       CGS_KICK_ADVANCE=2, CGS_KICK_LATE_TICKS=3, CGS_BASS_MAX_DELAY=7 };

static int cgs_chords_pulsed(const cgs_clip_t *clip)
{ return clip->event[0][0]>1 && clip->event[1][0]>1; }

static const uint8_t *cgs_chord_gaps(unsigned bar)
{
    unsigned idea=cgs_active()?cpa.plan.idea:cpa_setup.plan.idea;
    if(cgs_band_arp(CGS_CHORDS) || idea==CF_P_DRAFT)return 0;
    const cgs_clip_t *chords=&cgs.active[CGS_CHORDS];
    return cgs_chords_pulsed(chords)?chords->response_timing[bar]:0;
}

/* Bass runs before Drums. At a grid boundary both read the incoming drum clip. */
static const cgs_clip_t *cgs_drum_source(unsigned pending)
{
    return pending&(1u<<CGS_DRUMS)?&cgs.next[CGS_DRUMS]:&cgs.active[CGS_DRUMS];
}

static const uint8_t *cgs_bass_kicks(unsigned bar, unsigned pending)
{
    if(cgs.hold&(1u<<CGS_BASS))return cgs.held_kicks[bar];
    return cgs_drum_source(pending)->response_timing[bar];
}

static void cgs_capture_response(unsigned part, unsigned pending)
{
    if(part==CGS_LAYER){
        cgs.held_reply=cgs_chord_gaps(0)!=0;
        if(cgs.held_reply)memcpy(cgs.held_gaps,cgs.active[CGS_CHORDS].response_timing,sizeof cgs.held_gaps);
        memcpy(cgs.held_backbeat,cgs_drum_source(pending)->backbeat,sizeof cgs.held_backbeat);
    } else if(part==CGS_BASS){
        memcpy(cgs.held_kicks,cgs_drum_source(pending)->response_timing,sizeof cgs.held_kicks);
    }
}

static const uint8_t *cgs_layer_gaps(unsigned bar)
{
    if(cgs.hold&(1u<<CGS_LAYER))return cgs.held_reply?cgs.held_gaps[bar]:0;
    return cgs_chord_gaps(bar);
}

static unsigned cgs_layer_backbeat(unsigned bar, unsigned pending)
{
    if(cgs.hold&(1u<<CGS_LAYER))return cgs.held_backbeat[bar];
    return cgs_drum_source(pending)->backbeat[bar];
}

/* A candidate can wait until the first playable opening within three steps.
 * The latest candidate wins when several would land in the same opening. */
static unsigned cgs_gap_reply(const cgs_clip_t *clip, const uint8_t *gaps,
                              unsigned bar, unsigned step, unsigned *feel)
{
    unsigned window=gaps[step];
    if(!window)return 0;
    unsigned first=step>CGS_REPLY_LATE_STEPS?step-CGS_REPLY_LATE_STEPS:0;
    unsigned from=step;
    while(!clip->event[bar][from]){
        if(from==first || gaps[from-1u])return 0;
        from--;
    }
    unsigned event=clip->event[bar][from], accent=clip->feel[bar][from];
    unsigned delay=accent&7u, opening=window>>5;
    if(delay<opening)delay=opening;
    unsigned limit=(window&31u)-(delay!=0);
    if(!limit)return 0;
    unsigned gate=event>>3;
    *feel=(accent&~7u)|delay;
    return (event&7u)|((gate<limit?gate:limit)<<3);
}

static unsigned cgs_first_fragment(const cgs_clip_t *clip,unsigned bar,unsigned *feel)
{
    for(unsigned step=0;step<CGS_STEPS;step++){
        if(!clip->event[bar][step])continue;
        *feel=clip->feel[bar][step]&~7u;
        return clip->event[bar][step];
    }
    return 0;
}

/* One fragment per backbeat, in the first available opening after it. */
static unsigned cgs_snare_reply(const cgs_clip_t *clip,const uint8_t *gaps,
                               unsigned backbeat,unsigned bar,unsigned step,unsigned *feel)
{
    if(!backbeat || step<backbeat || step>=backbeat+3u)return 0;
    if(gaps){
        if(!gaps[step])return 0;
        for(unsigned j=backbeat;j<step;j++)if(gaps[j])return 0;
    } else if(step!=backbeat)return 0;
    unsigned event=cgs_first_fragment(clip,bar,feel);
    if(!event)return 0;
    unsigned gate=event>>3;
    if(gate>5u)gate=5u;
    if(gaps){
        unsigned delay=gaps[step]>>5, limit=(gaps[step]&31u)-(delay!=0);
        if(!limit)return 0;
        if(gate>limit)gate=limit;
        *feel|=delay;
    }
    return (event&7u)|(gate<<3);
}

static unsigned cgs_harmony_reply(const cgs_clip_t *clip,unsigned bar,unsigned ticks,unsigned offset,
                                 unsigned delay,const cpa_slot_t *slot,unsigned *feel)
{
    if(!cgs.harmony_changed || offset || cpa.elapsed<delay || cpa.elapsed-delay>=ticks)return 0;
    unsigned event=cgs_first_fragment(clip,bar,feel);
    unsigned end=slot->tie?slot->ticks:(uint32_t)slot->ticks*slot->gate/100u;
    if(!event || cpa.elapsed>=end)return 0;
    unsigned gate=(end-cpa.elapsed)/6u;
    if(gate>(event>>3))gate=event>>3;
    return gate?((event&7u)|(gate<<3)):0;
}

static int cgs_bass_can_edit(unsigned event)
{
    unsigned role=event&7u;
    return role && role!=CGS_ANCHOR && role!=CGS_PICKUP && role!=CGS_STEP_IN;
}

/* An empty kick can pull the next weak note forward. Remember its source
 * until it passes, even if a Drums edit changes the kick map in between. */
static unsigned cgs_kick_bass(unsigned reply, const uint8_t *notes, const uint8_t *accents,
                              const uint8_t *kicks, unsigned step, unsigned steps,
                              unsigned *feel, uint8_t *pulled)
{
    unsigned event=notes[step], kick=kicks[step];
    if(reply==CGS_BASS_REPLY_LATE){
        if(kick && cgs_bass_can_edit(event)){
            unsigned delay=kick-1u+CGS_KICK_LATE_TICKS;
            *feel=(*feel&~7u)|(delay<CGS_BASS_MAX_DELAY?delay:CGS_BASS_MAX_DELAY);
        }
        return event;
    }
    if(step+1u==*pulled)return 0;
    if(!kick)return event;
    unsigned from=step;
    if(!event){
        unsigned end=step+CGS_KICK_ADVANCE+1u;
        if(end>steps)end=steps;
        for(from=step+1u;from<end;from++){
            if(kicks[from])return 0;
            if(notes[from])break;
        }
        if(from==end || from+1u==*pulled || !cgs_bass_can_edit(notes[from]))return 0;
        event=notes[from];
        *pulled=(uint8_t)(from+1u);
    }
    unsigned accent=(accents[from]>>3)+4u;
    *feel=((accent<31u?accent:31u)<<3)|(kick-1u);
    return event;
}

/* Each take keeps one kick map across independent musician rolls. */
static const uint16_t CGS_PUNK_KICK[][2]={
    {0x0445,0x2441}, /* syncopated chug */
    {0x2085,0x14a1}, /* late kick and pushed answer */
    {0x0109,0x4481}, /* half-time breakdown */
    {0x1541,0x1145}, /* straight drive */
};

/* Leave room after the last shared riff attack. Extra Double kick hits
 * and bass pickups do not extend the riff. */
static unsigned cgs_punk_turnaround(const cgs_clip_t *drums,unsigned bar,unsigned steps)
{
    const uint8_t *bass=cgs.active[CGS_BASS].event[bar];
    unsigned pattern=CGS_PUNK_KICK[drums->voice][bar], step=steps;
    while(step){
        step--;
        unsigned role=bass[step]&7u;
        unsigned source=steps==CGS_STEPS?step:step*4u/3u;
        if((pattern&(1u<<source)) && (drums->event[bar][step]&CGS_KICK) &&
           role && role!=CGS_PICKUP && role!=CGS_STEP_IN)
            break;
    }
    return step+1u<steps-3u?steps-3u:step+1u;
}

static void cgs_tick(void)
{
    if(cgs.last_tick==cpa.q2 || cpa.clock_wait || cpa.paused)return;
    cgs.last_tick=cpa.q2;
    for(unsigned p=0;p<CGS_DRUMS;p++)
        if(cgs.gate[p] && !--cgs.gate[p])cgs_release(p);
    unsigned ticks=cgs_step_ticks(cpa.plan.beats);
    unsigned steps=cgs_steps(cpa.plan.beats);
    unsigned offset=cpa.t%ticks;
    const cpa_slot_t *s=&cpa.plan.slot[cpa.slot];
    if(!cpa.elapsed){
        cgs.harmony_changed=cgs.harmony_root!=s->root || cgs.harmony_mask!=s->mask;
        cgs.harmony_root=s->root;
        cgs.harmony_mask=s->mask;
    }
    if(offset>CGS_MAX_ONSET_DELAY &&
       (offset>CGS_BASS_MAX_DELAY || cgs.active[CGS_BASS].responds!=CGS_BASS_REPLY_LATE))return;
    if(!offset && !cpa_setup.flags)cgs_sound_boundary(CGS_PATCH_ROLL);
    unsigned pending=offset?0:cgs.pending;
    unsigned step=cpa.t/ticks, phrase=cgs.bar-cgs.origin_bar, bar=phrase&1u;
    const cgs_profile_t *profile=&CGS_PROFILES[cgs.playing_genre];
    int custom=cpa.plan.idea==CF_P_DRAFT;
    int slot_closed=custom && !s->tie && s->gate<100 && cpa.elapsed>=(uint32_t)s->ticks*s->gate/100u;
    for(unsigned p=0;p<CGS_NPART;p++){
        if(offset>CGS_MAX_ONSET_DELAY && p!=CGS_BASS)continue;
        unsigned bit=1u<<p, changed=pending&bit, rerolled=0;
        if(changed){
            if(cgs.active[p].arp_pattern!=cgs.next[p].arp_pattern || cgs.active[p].transpose!=cgs.next[p].transpose)
                cgs_release(p);
            rerolled=cgs.active[p].seed!=cgs.next[p].seed;
            cgs.active[p]=cgs.next[p];
            if(cgs.hold&bit)cgs_capture_response(p,pending);
            if(cgs.pending_auto&bit)cgs.develop_seed[p]=cgs.next_develop_seed[p];
            cgs.generation[p]++;
            if(p!=CGS_CHORDS && rerolled)cgs_release(p);
            if(p==CGS_CHORDS && cgs.active[p].event[bar][0]<=1)cgs.gate[p]=0;
        }
        const cgs_clip_t *clip=&cgs.active[p];
        if(p==CGS_BASS && clip->responds && !offset){
            if(!step || rerolled)cgs.bass_pull=0;
            const uint8_t *kicks=cgs_bass_kicks(bar,pending);
            unsigned feel=clip->feel[bar][step];
            cgs.bass_event=(uint8_t)cgs_kick_bass(clip->responds,clip->event[bar],clip->feel[bar],
                                                kicks,step,steps,&feel,&cgs.bass_pull);
            cgs.bass_feel=(uint8_t)feel;
            cgs.bass_attack=clip->responds==CGS_BASS_REPLY_LOCK && kicks[step];
        }
        if(p==CGS_DRUMS && profile->composer==CGS_COMPOSE_PUNK && (phrase&7u)==7u &&
           !offset && (!step || (pending&((1u<<CGS_BASS)|(1u<<CGS_DRUMS)))))
            cgs.punk_fill_step=(uint8_t)cgs_punk_turnaround(clip,bar,steps);
        if(cgs.mute_applied&bit)continue;
        if(p<CGS_DRUMS && (cpa.plan.mode==CPA_DRUMS || cgs_manual(p)))continue;
        if((slot_closed && p<CGS_DRUMS) || (custom && p==CGS_BASS && !s->nb)){
            cgs_release(p);
            continue;
        }
        if(p==CGS_LAYER && (cgs.section&CGS_FORM_LAYER_REST)){
            if(!step && !offset)cgs_release(p);
            continue;
        }
        int arp=(p==CGS_CHORDS || p==CGS_LAYER) && cgs_band_arp(p);
        if((p==CGS_CHORDS || p==CGS_LAYER) && !offset){
            if(arp){
                if(!(sqp.aon&bit)){cgs_release(p);cgs_arp(s,p);}
                else if(!cpa.elapsed)cgs_arp(s,p);
            } else if(sqp.aon&bit)sqp_arp_off(p);
        }
        if(arp)continue;
        if(p==CGS_DRUMS && cpa.plan.rhythm>=DM_N)continue;
        unsigned event=clip->event[bar][step], feel=clip->feel[bar][step];
        int pedal=p==CGS_BASS && profile->bass_style==CGS_BASS_PEDAL && !clip->voice;
        if(pedal && (offset || (step && cpa.elapsed>=ticks)))continue;
        if(pedal){event=clip->event[bar][0];feel=clip->feel[bar][0];}
        if(p==CGS_BASS && clip->responds){
            event=cgs.bass_event;
            feel=cgs.bass_feel;
        } else if(p==CGS_LAYER && clip->responds){
            const uint8_t *gaps=cgs_layer_gaps(bar);
            unsigned reply=clip->responds;
            if(reply==CGS_LAYER_REPLY_HARMONY || (reply==CGS_LAYER_REPLY_DREAM && !gaps)){
                unsigned delay=reply==CGS_LAYER_REPLY_HARMONY?4u*CGS_TICKS:2u*CGS_TICKS;
                event=cgs_harmony_reply(clip,bar,ticks,offset,delay,s,&feel);
            } else if(gaps && !(cgs.hold&bit) &&
                      (!gaps[step] || (cgs.gate[CGS_CHORDS] && !cgs_manual(CGS_CHORDS)))){
                if(sqp.ln)cgs_release(CGS_LAYER);
                event=0;
            } else if(reply==CGS_LAYER_REPLY_SNARE)
                event=cgs_snare_reply(clip,gaps,cgs_layer_backbeat(bar,pending),bar,step,&feel);
            else if(gaps)event=cgs_gap_reply(clip,gaps,bar,step,&feel);
        }
        if(profile->composer==CGS_COMPOSE_DREAM){
            /* The middle of the form opens room. The last bar brings the answer forward. */
            if((phrase&7u)==4u && p==CGS_LAYER)event=0;
            if((phrase&7u)==4u && p==CGS_DRUMS)event&=~(CGS_HAT|CGS_OPEN);
            if((phrase&7u)==7u && p==CGS_DRUMS && step==steps-2u)event|=CGS_RIM;
        } else if(profile->composer==CGS_COMPOSE_PUNK){
            unsigned section=phrase&7u;
            if(section==4u){
                if(p==CGS_LAYER || (p==CGS_CHORDS && step && (step&3u)))event=0;
                if(p==CGS_DRUMS)event&=~(CGS_OPEN|CGS_HAT);
            }
            if(p==CGS_CHORDS && (section==6u || section==7u) && event>1 &&
               (step==0 || step==steps/2))event=16;
            if(p==CGS_DRUMS && section==7u && !cf_moment.fill){
                if(step==cgs.punk_fill_step)event|=CGS_MID_TOM;
                if(step==cgs.punk_fill_step+1u)event|=CGS_LOW_TOM;
            }
        }
        if(p==CGS_CHORDS){
            if(offset)continue;
            unsigned pulse=clip->event[bar][0]>1;
            if(event==1 || (pulse && event && cpa.elapsed))sqp_chord_off();
            if(rerolled || (!pulse && !sqp.n) || !cpa.elapsed || event){
                cgs_common_chord(s,feel?feel:79);
                cgs.gate[p]=(uint8_t)(pulse?(event>1?event:clip->event[bar][0])*6u:0u);
            }
        } else if(offset==(feel&7u)){
            if(p==CGS_DRUMS){
                if(cgs.section&CGS_FORM_LIGHT_HATS)event&=~(CGS_HAT|CGS_OPEN);
                if(cgs.active[CGS_LAYER].responds==CGS_LAYER_REPLY_HARMONY && cgs.harmony_changed &&
                   cpa.elapsed<SQ_PPQN*2u && !cf_moment.fill)
                    event&=~(CGS_RIM|CGS_HAT|CGS_OPEN);
                cgs_drums(event,step,steps,feel>>3);
            }
            else if(event){
                unsigned tone=(event&7u)-1u;
                unsigned note=p==CGS_BASS?cgs.pitch.note[cpa.slot][tone]:s->note[tone<s->n?tone:s->n-1];
                if(p==CGS_BASS && (tone==CGS_PICKUP-1u || tone==CGS_STEP_IN-1u) &&
                    s->ticks-cpa.elapsed>4*CGS_TICKS)note=cgs.pitch.note[cpa.slot][CGS_FIFTH-1u];
                if(p==CGS_LAYER && (profile->composer==CGS_COMPOSE_PUNK ||
                                    clip->responds==CGS_LAYER_REPLY_HARMONY)){
                    unsigned pc=tone==CGS_FIFTH-1u?cgs.pitch.note[cpa.slot][tone]%12u:s->root%12u;
                    note=profile->layer_floor+pc+(tone==CGS_OCTAVE-1u?12u:0u);
                } else if(p==CGS_LAYER && note<profile->layer_floor)note+=12;
                if(p==CGS_LAYER && profile->composer==CGS_COMPOSE_DREAM && clip->voice==3 && note<70)note+=12;
                if(p==CGS_BASS && clip->responds && cgs.bass_attack && sqp.nb)cgs_release(CGS_BASS);
                int shifted=(int)note+clip->transpose;
                cgs_line(p,shifted>=0 && shifted<=127?(unsigned)shifted:note,cgs_velocity(p,48u+2u*(feel>>3)));
                cgs.gate[p]=pedal?0:(uint8_t)((event>>3)*6u);
            }
        }
    }
    if(!offset)cgs.pending=cgs.pending_auto=0;
}
static void cgs_apply_mute(void)
{
    unsigned muted=cgs_mute_mask(), release=muted&~cgs.mute_applied;
    cgs.mute_applied=(uint8_t)muted;
    for(unsigned p=0;p<CGS_NPART;p++)
        if(release&(1u<<p))cgs_release(p);
}

static int cgs_mix_live(void)
{
    return cgs_active() || (CF_STUDIO && cgs.mix_idle && !cpa.on && !sqp.on && !dmp.on);
}

static void cgs_audio_block(uint32_t n)
{
    for(unsigned p=0;p<CGS_NPART;p++){
        int32_t target=cgs.level[p]*32768u/100u;
        int32_t delta=target-cgs.gain[p];
        int32_t move=delta*(int32_t)n/(int32_t)(FS/40u);
        cgs.gain[p]=(uint16_t)(move?cgs.gain[p]+move:target);
    }
}
static void cgs_commit_take(void)
{
    if(cgs.queued_bpm && !cfp_cfg.clock_in){
        cfm.bpm=cfp_cfg.bpm=cgs.shuffled_bpm=cgs.queued_bpm;
        cgs.tempo_generation++;
    }
    cgs.queued_bpm=0;
    cgs.take_revision++;
    cgs.pending_auto=0;
    if(cgs.clear_take_hold)cgs.hold=0;
    cgs.clear_take_hold=0;
    cgs.pitch=cgs.setup_pitch;
    cgs.origin_bar=cgs.bar;
    cgs.harmony_mask=0;
    cgs.developed=0;
    for(unsigned p=0;p<CGS_NPART;p++){
        cgs.next[p]=cgs.take[p];
    }
    cgs.pending=(1u<<CGS_NPART)-1u;
    cgs.playing_genre=cgs.queued_genre;
    cgs.playing_feel=cgs.queued_feel;
    cgs.harmony_generation++;
}

/* Original motifs. The same rhythmic question recurs over successive
 * chords. The response is voiced against the current and incoming harmony. */
static const cgs_riff_t CGS_RIFFS[]={
    {CGS_FOLLOW,{{0,CGS_ANCHOR,11},{5,CGS_UPPER,3},{7,CGS_THIRD,5},{10,CGS_FIFTH,10},{15,CGS_PICKUP,3}}},
    {CGS_ANSWER,{{0,CGS_ANCHOR,7},{4,CGS_OCTAVE,5},{8,CGS_UPPER,6},{11,CGS_FIFTH,9},{15,CGS_STEP_IN,3}}},
    {CGS_FLOAT, {{0,CGS_ANCHOR,17},{6,CGS_THIRD,7},{9,CGS_FIFTH,12},{13,CGS_UPPER,5},{15,CGS_STEP_IN,3}}},
    {CGS_FOLLOW,{{0,CGS_ANCHOR,7},{3,CGS_FIFTH,4},{7,CGS_OCTAVE,6},{10,CGS_ANCHOR,11},{15,CGS_PICKUP,3}}},
    {CGS_FLOAT, {{0,CGS_ANCHOR,23},{7,CGS_UPPER,5},{9,CGS_THIRD,10},{13,CGS_FIFTH,5},{15,CGS_STEP_IN,3}}},
    {CGS_ANSWER,{{0,CGS_ANCHOR,9},{5,CGS_THIRD,6},{8,CGS_FIFTH,5},{11,CGS_OCTAVE,6},{14,CGS_STEP_IN,5}}},
    {CGS_FLOAT, {{0,CGS_ANCHOR,12},{4,CGS_UPPER,5},{7,CGS_FIFTH,13},{12,CGS_THIRD,5},{15,CGS_PICKUP,3}}},
    {CGS_FOLLOW,{{0,CGS_ANCHOR,8},{3,CGS_OCTAVE,5},{6,CGS_FIFTH,6},{10,CGS_THIRD,9},{15,CGS_STEP_IN,3}}},
    {CGS_ANSWER,{{0,CGS_ANCHOR,16},{5,CGS_FIFTH,4},{8,CGS_OCTAVE,7},{11,CGS_UPPER,7},{15,CGS_PICKUP,3}}},
    {CGS_FLOAT, {{0,CGS_ANCHOR,13},{4,CGS_THIRD,6},{7,CGS_UPPER,14},{12,CGS_FIFTH,7},{15,CGS_STEP_IN,3}}},
    {CGS_FOLLOW,{{0,CGS_ANCHOR,7},{3,CGS_UPPER,5},{7,CGS_THIRD,8},{10,CGS_FIFTH,12},{15,CGS_PICKUP,3}}},
    {CGS_ANSWER,{{0,CGS_ANCHOR,12},{5,CGS_OCTAVE,4},{8,CGS_THIRD,8},{11,CGS_FIFTH,9},{15,CGS_STEP_IN,3}}},
};
static const uint8_t CGS_RIFF_WEIGHT[2][sizeof CGS_RIFFS/sizeof CGS_RIFFS[0]]={
    {6,5,4,6,5,4,2,2,2,1,4,4},
    {2,4,5,1,3,5,6,4,5,6,2,3}
};
static const cgs_riff_t *cgs_riff(unsigned style, uint32_t *seed)
{
    const uint8_t *weight=CGS_RIFF_WEIGHT[style];
    unsigned total=0;
    for(unsigned i=0;i<sizeof CGS_RIFF_WEIGHT[0];i++)total+=weight[i];
    unsigned draw=cgs_rand(seed,total), index=0;
    while(draw>=weight[index])draw-=weight[index++];
    return &CGS_RIFFS[index];
}
static void cgs_bass_clip(cgs_clip_t *clip, const cgs_riff_t *riff, unsigned activity, uint32_t *seed)
{
    unsigned answer=cgs_rand(seed,3);
    for(unsigned bar=0;bar<2;bar++){
        for(unsigned i=0;i<5;i++){
            const uint8_t *cell=riff->cell[i];
            unsigned step=cell[0], role=cell[1], gate=cell[2];
            if(i==1 && activity<36)continue;
            if(i==4 && (!bar || activity<39))continue;
            if(bar && i==2 && activity>60 && answer==1)role=CGS_UPPER;
            if(bar && i==3 && answer==2 && riff->pocket==CGS_ANSWER)step++;
            unsigned velocity=i==0?24:i==4?10:riff->pocket==CGS_FOLLOW && step==CGS_KICK_REPLY?22:15;
            unsigned delay=i>0 && i<4 && (step&1u) && riff->pocket!=CGS_FOLLOW?2u+answer:0;
            clip->event[bar][step]=(uint8_t)(role+(gate<<3));
            clip->feel[bar][step]=(uint8_t)((velocity<<3)+delay);
        }
    }
}
static void cgs_chord_clip(cgs_clip_t *clip,unsigned activity,unsigned shape)
{
    static const uint8_t second_attack[]={7,8,10,11};
    for(unsigned bar=0;bar<2;bar++){
        uint8_t *e=clip->event[bar], *v=clip->feel[bar];
        v[0]=(uint8_t)(80u+shape*2u);
        unsigned second=second_attack[shape];
        e[0]=(uint8_t)(12u+shape);
        if(activity>=26){
            e[second]=(uint8_t)(bar?10u:12u);
            v[second]=(uint8_t)(bar?76u:72u);
        }
        if(bar && activity>=65){e[13]=8;v[13]=68;}
    }
}

static void cgs_punk_clip(cgs_clip_t *clip,unsigned part,unsigned style,unsigned activity,unsigned shape)
{
    unsigned pocket=clip->voice;
    for(unsigned bar=0;bar<2;bar++){
        uint8_t *event=clip->event[bar], *feel=clip->feel[bar];
        uint16_t kicks=CGS_PUNK_KICK[pocket][bar];
        if(part==CGS_CHORDS){
            feel[0]=107;
            if(style){
                unsigned lift=8u+((shape+bar)&1u)*4u;
                event[0]=(uint8_t)(activity>=48?24:32);
                if(activity>=35){event[lift]=20;feel[lift]=98;}
                if(bar && activity>=72){event[15]=4;feel[15]=92;}
                continue;
            }
            event[0]=(uint8_t)(pocket==2?13u:5u+(shape&1u)*2u);
            for(unsigned i=1;i<CGS_STEPS;i++){
                if(!((kicks>>i)&1u) || (activity<32 && i<6))continue;
                int accent=(i&3u)==0 || (i>=8 && ((i+shape)&3u)>=2u);
                event[i]=(uint8_t)(accent?8:3);
                feel[i]=(uint8_t)(accent?103:83);
            }
            unsigned push=bar?12u+(shape&1u):5u+(shape&1u);
            if(activity>=47 && !event[push]){event[push]=3;feel[push]=82;}
            if(bar && activity>=78){event[15]=3;feel[15]=96;}
        } else if(part==CGS_BASS){
            static const uint8_t replies[]={CGS_FIFTH,CGS_THIRD,CGS_OCTAVE,CGS_UPPER};
            event[0]=CGS_ANCHOR+((pocket==2?13u:7u)<<3);feel[0]=120;
            for(unsigned i=1;i<CGS_STEPS;i++){
                if(!((kicks>>i)&1u))continue;
                unsigned role=style || (i>=7 && activity>=40)?replies[(shape+bar+i/2)%NELEM(replies)]:CGS_ANCHOR;
                unsigned gate=pocket==2?9u:(i&3u)?3u:6u;
                event[i]=(uint8_t)(role+(gate<<3));feel[i]=(uint8_t)((i&3u)?104:120);
            }
            unsigned answer=bar?11u+(shape&1u):5u+(shape&1u);
            if(activity>=39 && !event[answer]){
                event[answer]=(uint8_t)(replies[(shape+bar)%NELEM(replies)]+(4u<<3));
                feel[answer]=101;
            }
            if(activity>=52){event[15]=(uint8_t)((bar?CGS_PICKUP:CGS_STEP_IN)+(3u<<3));feel[15]=96;}
        } else if(part==CGS_DRUMS){
            for(unsigned i=0;i<CGS_STEPS;i++){
                if((kicks>>i)&1u)event[i]|=CGS_KICK;
                if(activity>=29 && i%(activity>=55?2u:4u)==0)event[i]|=CGS_HAT;
                if(event[i])feel[i]=(uint8_t)(((i&3u)?(shape+i)%3u:4u)<<3);
            }
            event[0]|=CGS_OPEN;
            if(pocket==2 || (!style && pocket==0)){event[8]|=CGS_SNARE;feel[8]=5u<<3;}
            else {event[4]|=CGS_SNARE;event[12]|=CGS_SNARE;feel[4]=feel[12]=5u<<3;}
            if(style && activity>=55)event[bar?11:7]|=CGS_KICK;
            if(bar && activity>=43){
                event[13+(shape&1u)]|=(shape&2u)?CGS_SNARE:CGS_MID_TOM;
                event[15]|=CGS_LOW_TOM;
            } else if(activity>=48)event[bar?14:10]|=CGS_OPEN;
        } else {
            unsigned first=bar?10u:4u, last=first+4u;
            unsigned onset=first;
            while(onset<=last && (kicks&(1u<<onset)))onset+=2u;
            if(onset>last)continue;
            unsigned role=(bar^(shape&1u))?CGS_FIFTH:CGS_ANCHOR;
            if(style && bar)role=CGS_OCTAVE;
            event[onset]=(uint8_t)(role+((style?5u:6u)<<3));
            feel[onset]=style?72:80;
            if(activity>=54 && onset+2u<CGS_STEPS && !(kicks&(1u<<(onset+2u)))){
                event[onset+2u]=(uint8_t)((role==CGS_ANCHOR?CGS_FIFTH:CGS_ANCHOR)+(3u<<3));
                feel[onset+2u]=60;
            }
        }
    }
}

static void cgs_pocket_clip(cgs_clip_t *clip, unsigned part, const cgs_profile_t *profile, unsigned style,
                           unsigned activity, unsigned shape)
{
    const cgs_pocket_t *p=&profile->pocket;
    for(unsigned bar=0;bar<2;bar++){
        uint8_t *events=clip->event[bar], *feel=clip->feel[bar];
        if(part==CGS_CHORDS && p->chords){
            for(unsigned i=0;i<CGS_STEPS;i++){
                if(!((p->chords>>i)&1u) || (i && activity<32 && i!=8))continue;
                events[i]=(uint8_t)(p->gate+style*3);
                feel[i]=(uint8_t)(i?63u+((i+shape+bar)%4)*5u:87u);
            }
            if(bar && activity>68){events[15]=4;feel[15]=70;}
        } else if(part==CGS_BASS){
            if(profile->bass_style==CGS_BASS_PEDAL){
                clip->voice=(uint8_t)style;
                for(unsigned i=1;i<CGS_STEPS;i++)
                    if(!style || activity<23)events[i]=0;
                continue;
            }
            if(profile->bass_style==CGS_BASS_OCTAVE_PULSE && activity>=53){
                for(unsigned i=2;i<CGS_STEPS;i+=4){
                    if(events[i])continue;
                    events[i]=CGS_OCTAVE+(6u<<3);
                    feel[i]=13u<<3;
                }
            } else if(profile->bass_style==CGS_BASS_LATE){
                for(unsigned i=2;i<CGS_STEPS;i+=4)
                    if(events[i])feel[i]=(uint8_t)((feel[i]&~7u)|5u);
            }
        } else if(part==CGS_LAYER){
            if(profile->layer_style==CGS_LAYER_SWELL){
                unsigned i=bar?9u+shape:3u+shape;
                events[i]=(uint8_t)((style?3u:CGS_FIFTH)+(31u<<3));
                feel[i]=style?74:64;
                if(style && bar && activity>65){events[15]=2u+(21u<<3);feel[15]=61;}
            } else if(profile->layer_style==CGS_LAYER_FRAGMENT){
                unsigned i=(bar?9u:5u)+shape;
                events[i]=(uint8_t)((style?4u:2u)+(12u<<3));feel[i]=101;
                if(activity>=45){events[14]=3u+(5u<<3);feel[14]=68;}
            } else {
                unsigned count=1u+(activity>=30)+(activity>=53)+(activity>=76);
                for(unsigned j=0;j<count;j++){
                    unsigned i=profile->layer_start+j*(style?3u:2u)+(shape&1u);
                    events[i]=(uint8_t)((1u+(j+shape+(bar && j==count-1u))%4u)+((style?9u:5u)<<3));
                    feel[i]=(uint8_t)((j?9u:14u)<<3);
                }
            }
        } else if(part==CGS_DRUMS){
            if(profile->sparse_drums){
                if(activity>=32 && !bar){events[0]=CGS_KICK;feel[0]=2;}
                if(activity>=47 && bar){events[10+shape]=CGS_RIM;feel[10+shape]=4;}
                if(style && activity>=65)events[14]=CGS_OPEN;
                continue;
            }
            unsigned kicks=style && bar && p->kick_answer?p->kick_answer:p->kick;
            for(unsigned i=0;i<CGS_STEPS;i++){
                if((kicks>>i)&1u)events[i]|=CGS_KICK;
                if(activity>=26 && i%(activity>=68?2u:p->hat_gap)==0)events[i]|=CGS_HAT;
                if(events[i] && i)feel[i]=(uint8_t)((((shape+i+bar)%4u)<<3)+(i&3u?p->drift:0));
            }
            events[p->snare]|=CGS_SNARE;
            feel[p->snare]|=p->drift;
            if(p->snare==4){events[12]|=CGS_SNARE;feel[12]|=p->drift;}
            if(bar && activity>=48)events[14u+(shape&1u)]|=style?CGS_OPEN:CGS_RIM;
            if(bar && activity>=79)events[13]|=CGS_SNARE;
        }
    }
}

/* Four Dream Pop feels give each musician a different job in the same take.
 * Kick maps are anchors for the bass, and Layer answers in the gaps. */
static const uint16_t CGS_DREAM_KICK[][2]={
    {0x0401,0x4001},{0x0081,0x1041},{0x1101,0x4141},{0x0221,0x4201}
};
static void cgs_dream_clip(cgs_clip_t *clip,unsigned p,unsigned voice,unsigned style,unsigned activity,uint32_t *seed)
{
    unsigned shape=cgs_rand(seed,4);
    if(p==CGS_CHORDS && !style){
        clip->feel[0][0]=clip->feel[1][0]=(uint8_t)(73u+voice*4u+activity/12u);
        return;
    }
    if(p==CGS_CHORDS && voice<2){
        cgs_chord_clip(clip,activity,shape);
        return;
    }
    if(p==CGS_BASS && voice==3){
        cgs_bass_clip(clip,cgs_riff(style,seed),activity,seed);
        return;
    }
    for(unsigned bar=0;bar<2;bar++){
        uint8_t *e=clip->event[bar], *v=clip->feel[bar];
        uint16_t kick=CGS_DREAM_KICK[voice][bar];
        if(p==CGS_CHORDS){
            e[0]=(uint8_t)((voice==2?8:15)+style*3);
            e[voice==2?(bar?7:6):10]=(uint8_t)(voice==2?7:12);
            v[0]=83; v[voice==2?(bar?7:6):10]=72;
            if(activity>55)e[14]=5;
        } else if(p==CGS_BASS){
            unsigned count=0;
            for(unsigned step=0;step<16;step++){
                if(!(kick&(1u<<step)))continue;
                unsigned role=count==0?CGS_ANCHOR:voice==2?CGS_OCTAVE:
                    (shape&1u)?CGS_THIRD:CGS_FIFTH;
                unsigned len=voice==0?25u:voice==1?14u:7u;
                e[step]=(uint8_t)(role+(len<<3));v[step]=(uint8_t)((count++?15u:24u)<<3);
            }
            if(activity>=40){
                unsigned step=voice==0?6u:voice==1?(bar?10u:12u):4u+2u*(shape&1u);
                if(!e[step]){
                    e[step]=(uint8_t)((style?CGS_UPPER:CGS_FIFTH)+((voice==0?12u:7u)<<3));
                    v[step]=(uint8_t)(104u+(voice==1?2u:0u));
                }
            }
            if(bar && activity>=53){e[15]=CGS_STEP_IN+(3u<<3);v[15]=88;}
        } else if(p==CGS_DRUMS){
            for(unsigned step=0;step<16;step++){
                if(kick&(1u<<step))e[step]=CGS_KICK;
                if(activity>=27 && (step+shape%2u)%(voice<2?4u:2u)==0){
                    e[step]|=CGS_HAT; v[step]=(uint8_t)((((step+bar)%3u)<<3)+(step&1u));
                }
            }
            unsigned backbeat=voice==0?8u:4u;
            e[backbeat]|=voice==1?CGS_RIM:CGS_SNARE;v[backbeat]=2;
            if(voice)e[12]|=CGS_SNARE;
            if(bar && activity>39)e[voice==0?15:14]|=style?CGS_OPEN:CGS_RIM;
        } else {
            unsigned first=voice==0?5u:voice==1?3u:voice==2?2u:7u;
            unsigned second=voice==0?12u:voice==1?10u:voice==2?8u:13u;
            unsigned gate=voice==0?22u:voice==1?12u:6u;
            unsigned tone=1u+shape%3u;
            e[first]=(uint8_t)(tone+(gate<<3));v[first]=105;
            if(activity>=25){
                e[second]=(uint8_t)((bar?2u:1u+(tone%4u))+(gate<<3));v[second]=81;
            }
            if(voice==2 && activity>40){e[5]=3u+(5u<<3);v[5]=86;}
            if(style && bar && activity>=55){e[15]=1u+(4u<<3);v[15]=65;}
        }
    }
}

/* Build response timing from the final, meter-mapped source clip. */
static void cgs_prepare_response_timing(cgs_clip_t *clip, unsigned part)
{
    const cgs_profile_t *profile=&CGS_PROFILES[cgs.genre];
    unsigned steps=cgs_steps(cgs.meter);
    unsigned first=profile->layer_reply[0], second=profile->layer_reply[1];
    if(part==CGS_DRUMS && (profile->bass_reply[0] || profile->bass_reply[1] ||
                         first==CGS_LAYER_REPLY_SNARE || second==CGS_LAYER_REPLY_SNARE)){
        memset(clip->response_timing,0,sizeof clip->response_timing);
        memset(clip->backbeat,0,sizeof clip->backbeat);
        for(unsigned bar=0;bar<2;bar++)for(unsigned step=0;step<steps;step++){
            unsigned delay=clip->feel[bar][step]&7u, event=clip->event[bar][step];
            if(delay>CGS_MAX_ONSET_DELAY)continue;
            if(event&CGS_KICK)clip->response_timing[bar][step]=(uint8_t)(delay+1u);
            if((event&CGS_SNARE) && !clip->backbeat[bar])clip->backbeat[bar]=(uint8_t)(step+1u);
        }
        return;
    }
    if(part!=CGS_CHORDS || !(first || second))return;
    memset(clip->response_timing,0,sizeof clip->response_timing);
    if(!cgs_chords_pulsed(clip))return;
    unsigned ticks=cgs_step_ticks(cgs.meter), release=0;
    for(unsigned i=0;i<=2u*steps;i++){
        unsigned event=i<2u*steps?clip->event[i/steps][i%steps]:0;
        if(!event && i<2u*steps)continue;
        unsigned attack=i*ticks;
        /* Keep grid points where a reply can begin by the last supported delay tick. */
        unsigned first=(release+ticks-1u-CGS_MAX_ONSET_DELAY)/ticks;
        for(unsigned j=first;j<i;j++){
            unsigned start=j*ticks;
            unsigned opening=release>start?release-start:0;
            unsigned units=(attack-start)/6u;
            if(units>31)units=31;
            clip->response_timing[j/steps][j%steps]=(uint8_t)((opening<<5)|units);
        }
        if(event)release=attack+(event>1?event:clip->event[i/steps][0])*6u+CGS_REPLY_CLEARANCE;
    }
}

static void cgs_meter_clip(cgs_clip_t *,unsigned,unsigned);
static void cgs_compose(cgs_clip_t *clip, unsigned p, uint32_t *seed)
{
    const cgs_profile_t *profile=&CGS_PROFILES[cgs.genre];
    unsigned style=cgs.style[p];
    unsigned activity=(unsigned)clamp(cgs.activity[p]+(int)cgs.energy-50,0,100);
    memset(clip,0,sizeof *clip);
    clip->seed=*seed;
    clip->shape_energy=cgs.energy;
    if(p==CGS_BASS)clip->responds=profile->bass_reply[style];
    else if(p==CGS_LAYER)clip->responds=profile->layer_reply[style];
    if(profile->composer==CGS_COMPOSE_DREAM){
        clip->voice=cgs.feel;
        cgs_dream_clip(clip,p,clip->voice,style,activity,seed);
        cgs_meter_clip(clip,p,cgs.meter);
        cgs_prepare_response_timing(clip,p);
        return;
    }
    unsigned shape;
    if(p==CGS_BASS && profile->composer!=CGS_COMPOSE_PUNK){
        cgs_bass_clip(clip,&profile->bass[style],activity,seed);
    }
    shape=cgs_rand(seed,4);
    if(profile->composer==CGS_COMPOSE_PUNK){
        clip->voice=cgs.feel;
        cgs_punk_clip(clip,p,style,activity,shape);
    }
    else if(p==CGS_CHORDS && (profile->hold_chords || style))
        clip->feel[0][0]=clip->feel[1][0]=(uint8_t)(74u+shape*2u+activity/12u+style*7u);
    else cgs_pocket_clip(clip,p,profile,style,activity,shape);
    cgs_meter_clip(clip,p,cgs.meter);
    cgs_prepare_response_timing(clip,p);
}

/* Fit existing rhythmic material to 3/4 or compound 6/8. The second
 * accent in 6/8 falls on the fourth eighth; 3/4 gets two light afterbeats. */
static void cgs_meter_clip(cgs_clip_t *clip,unsigned part,unsigned meter)
{
    if(meter==4u)return;
    const cgs_profile_t *profile=&CGS_PROFILES[cgs.genre];
    int preserve_notes=(part==CGS_BASS && clip->responds==CGS_BASS_REPLY_LATE) ||
        (part==CGS_LAYER && (clip->responds==CGS_LAYER_REPLY_SNARE ||
                            clip->responds==CGS_LAYER_REPLY_HARMONY || clip->responds==CGS_LAYER_REPLY_DREAM));
    int late_kicks=part==CGS_DRUMS && (profile->bass_reply[0]==CGS_BASS_REPLY_LATE ||
                                      profile->bass_reply[1]==CGS_BASS_REPLY_LATE);
    for(unsigned bar=0;bar<2;bar++){
        uint8_t *e=clip->event[bar],*v=clip->feel[bar];
        for(unsigned step=1;step<12;step++){
            unsigned src=step*4u/3u;
            unsigned event=e[src], feel=v[src];
            if(step%3u==2u){
                /* Keep the Late pocket notes and their kick anchors that fall
                 * between sampled steps. Use the nearer step if it is free. */
                if(preserve_notes && !event && e[src+1u]){
                    event=e[src+1u];feel=v[src+1u];
                } else if(late_kicks && !(event&CGS_KICK) && (e[src+1u]&CGS_KICK)){
                    if(!event)feel=v[src+1u];
                    event|=CGS_KICK;
                }
            }
            e[step]=(uint8_t)event;v[step]=(uint8_t)feel;
        }
        memset(e+12,0,4);
        memset(v+12,0,4);
        if(part!=CGS_DRUMS)continue;
        for(unsigned step=0;step<12;step++)e[step]&=(uint8_t)~CGS_SNARE;
        if(meter==6u)e[6]|=CGS_SNARE;
        else {e[4]|=CGS_HAT;e[8]|=CGS_SNARE;}
    }
}

static void cgs_apply_edit(cgs_clip_t *clip, unsigned part, unsigned steps, cgs_edit_t edit)
{
    unsigned step=edit.step;
    if(step>=steps)return;
    uint8_t *event=clip->event[1], *feel=clip->feel[1];
    switch(edit.kind){
    case CGS_EDIT_ACCENT:
        if(event[step] || (part==CGS_CHORDS && !step))
            feel[step]=(uint8_t)(part==CGS_CHORDS?edit.value:(feel[step]&7u)|(edit.value&~7u));
        break;
    case CGS_EDIT_CHORD_GATE:
        if(event[step]>1)event[step]=edit.value;
        break;
    case CGS_EDIT_HAT:
        event[step]^=CGS_HAT;
        break;
    case CGS_EDIT_DROP:
    case CGS_EDIT_ADD:
        for(unsigned earlier=0;earlier<step;earlier++){
            if(!event[earlier])continue;
            if(edit.kind==CGS_EDIT_DROP)event[step]=0;
            else if(!event[step]){event[step]=edit.value;feel[step]=64;}
            break;
        }
        break;
    case CGS_EDIT_BASS_ROLE:
        if(cgs_bass_can_edit(event[step]))
            event[step]=(uint8_t)((event[step]&~7u)|edit.value);
        break;
    case CGS_EDIT_BASS_LATER:
        if(step+1u<steps && cgs_bass_can_edit(event[step]) && !event[step+1u]){
            event[step+1u]=event[step];feel[step+1u]=feel[step];
            event[step]=0;
        }
        break;
    }
}

static int cgs_bass_edit(const cgs_clip_t *clip, unsigned steps, unsigned kicks,
                         uint32_t *seed, cgs_edit_t *edit)
{
    const uint8_t *answer=clip->event[1];
    unsigned span=steps-3u;
    unsigned start=cgs_rand(seed,span), move=cgs_rand(seed,3);
    for(unsigned i=0;i<span;i++){
        unsigned step=3u+(start+i)%span;
        if((kicks&(1u<<step)) || !cgs_bass_can_edit(answer[step]))continue;
        edit->step=(uint8_t)step;
        if(!move){
            edit->kind=CGS_EDIT_BASS_ROLE;
            edit->value=(uint8_t)((answer[step]&7u)==CGS_THIRD?CGS_FIFTH:CGS_THIRD);
        } else if(move==1 && step+1u<steps && !(kicks&(1u<<(step+1u))) && !answer[step+1u]){
            edit->kind=CGS_EDIT_BASS_LATER;
        } else {
            edit->kind=CGS_EDIT_ACCENT;
            edit->value=clip->feel[1][step]>96?64:120;
        }
        return 1;
    }
    return 0;
}

static int cgs_make_edit(const cgs_clip_t *clip, unsigned part, unsigned steps,
                         unsigned kicks, uint32_t *seed, cgs_edit_t *edit)
{
    if(part==CGS_BASS)return cgs_bass_edit(clip,steps,kicks,seed,edit);
    const uint8_t *event=clip->event[1], *feel=clip->feel[1];
    unsigned first=0;
    while(first<steps && !event[first])first++;
    unsigned span=steps-7u;
    unsigned start=cgs_rand(seed,span);
    for(unsigned i=0;i<span;i++){
        unsigned step=6u+(start+i)%span;
        edit->step=(uint8_t)step;
        if(part==CGS_LAYER){
            if(step<=first)continue;
            edit->kind=event[step]?CGS_EDIT_DROP:CGS_EDIT_ADD;
            edit->value=CGS_THIRD+(6u<<3);
        } else if(part==CGS_DRUMS){
            if(CGS_PROFILES[cgs.genre].sparse_drums){
                if(!event[step])continue;
                edit->kind=CGS_EDIT_ACCENT;
                edit->value=feel[step]>=24?8:32;
            } else edit->kind=CGS_EDIT_HAT;
        } else {
            if(event[step]<=1)continue;
            edit->kind=CGS_EDIT_CHORD_GATE;
            edit->value=(uint8_t)(event[step]>2?event[step]-1u:3u);
        }
        return 1;
    }
    if(part!=CGS_CHORDS)return 0;
    edit->kind=CGS_EDIT_ACCENT;
    edit->step=0;
    edit->value=feel[0]>=80?74:84;
    return 1;
}
