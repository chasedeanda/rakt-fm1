/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 ChoralRoot FM-1 contributors (a fork of Felucca); CoralFlower contributors */
/* CoralFlower: the audio side's glue (after ChoralRoot's cr_out.c).
 *
 *   part 0 CHORD  <- the chord player (cf_play.c): the chord sound (PRESETS)
 *   part 1 BASS   <- the bass of the chord: the bass sound (SELECT)
 *
 * Everything here runs in the audio ISR: cf_audio_block() is called at the top of every CTL-sample block, before
 * fx.c renders it (the mix_block shim in rakt.c / emu_firmware.h). It drains the events the UI posted
 * (cf_post: a lock-free single-producer ring), runs the chord player and the metronome, and reads MIDI in.
 * Felucca's sequencer (seq.c and its satellites) is not in the unit: what the kept files call of it is below, as
 * ChoralRoot's cr_out.c provided it (events_block, midi_event, panic_req, motion_guard ...). */

#if !FELUCCA_SEQ
/* ---------------------------------------------- what seq.c provided --- */
static volatile uint8_t panic_req;               /* bit per part: its notes released (a sound load, M-UPGRADE) */
static const uint8_t transport_req = 0;          /* (upreset.c, cr_bank.c: Felucca's transport never runs) */
#define chain_defaults(c) ((void)0)              /* (main.c felucca_init: no song chain) */
#ifdef FM1_INPUT_LAT
static uint32_t kb_out_tick;                     /* (audio.c: seq.c's key latency stamp; nothing reads it here) */
#endif
static uint32_t trk_index(const track_t *t) { return (uint32_t)(t - trk); }
static int drum_track(const track_t *t) { return ENGINES[eng_idx(t->eng_req)] == &ENG_DRUM; }
/* motion.c's: no motion lanes, so a part's sounding value is its saved one; the guard keeps the audio IRQ out while
 * a part's parameters are copied (cr_bank.c fm4_apply), as motion.c's did */
static int16_t motion_base_value(const track_t *t, uint32_t id) { return t->p[id]; }
static uint32_t motion_guard(void)
{
#if defined(FM1_IRQ_TARGET)
    uint32_t f = fm1_icfg();
    fm1_irq_off();
    return f;
#else
    return 0;
#endif
}
static void motion_unguard(uint32_t f)
{
#if defined(FM1_IRQ_TARGET)
    fm1_icfg_set(f);
#else
    (void)f;
#endif
}

/* ------------------------------------------------- the parts' MIDI in --- */
/* cr_midi_in (below) forwards a CHORD / BASS channel's notes, pedal, bend, pressure and the other CCs here as
 * midi_event(status, part, d1, d2), as it handed them to midi_control.c before; what of that ChoralRoot uses:
 *   note on / off       trk_note_on / trk_note_off on the part (a repeated note-on restarts it; velocity 0: off)
 *   CC 64 sustain       a released note is held while the pedal is down, released with it
 *   pitch bend          voice.c's midi_bend_target, +-2 semitones (RPN 0 sets 0..24 semitones, 0..99 cents); FM6 bends by its
 *                       own BEND range (track_t.bend_raw, docs/FM6.md)
 *   CC 1 / 11, pressure mod.c's MODW / EXPR / AT sources (mod_midi)
 *   CC 120              all sound off: the part's voices cut (a short fade), the pedal ignored
 *   CC 121              reset controllers: bend, wheel, pressure, expression, the pedal up (the bend range kept)
 *   CC 123              all notes off: the part's MIDI notes released (the pedal honoured), the part's voices too
 *                       when no MIDI note is held then
 * Felucca's chords (CHRD), its arpeggiator and live recording were the sequencer's: gone. A part's MIDI notes and the
 * engine's stream share its voices exactly as before (a MIDI note-off ends that note, whoever started it). */
#define CR_MPED 0x80u                            /* cr_mnote: the key is up, the pedal holds the note */
typedef struct {
    int16_t bend;                                /* signed 14-bit, 0 = centre */
    uint8_t pedal, semis, cents, rpn_msb, rpn_lsb, ready;
} cr_mch_t;
static cr_mch_t cr_mch[2];                       /* parts 0 / 1 (CHORD / BASS) */
static uint8_t cr_mnote[2][128];                 /* 1: a MIDI note-on holds it; | CR_MPED: the pedal does */
static uint8_t cr_mheld[2];                      /* notes cr_mnote holds per part (CC 123's check), at most 128 */

static cr_mch_t *cr_mchan(uint32_t p)
{
    cr_mch_t *c = &cr_mch[p];
    if (!c->ready) {
        c->semis = 2;
        c->rpn_msb = c->rpn_lsb = 127;
        c->ready = 1;
    }
    return c;
}
static void cr_mbend(uint32_t p)                 /* the channel's bend -> the part's (voice.c glides to it) */
{
    const cr_mch_t *c = cr_mchan(p);
    int32_t range = ((int32_t)c->semis * 100 + c->cents) * 256 / 100;
    midi_bend_target[p] = (int32_t)c->bend * range / (c->bend < 0 ? 8192 : 8191);
    trk[p].bend_raw = c->bend;                   /* (FM6 bends by its own BEND range: its patch's, docs/FM6.md) */
}
static void cr_mrelease(uint32_t p, uint32_t note)
{
    if (cr_mnote[p][note]) {
        cr_mnote[p][note] = 0;
        if (cr_mheld[p])
            cr_mheld[p]--;
        trk_note_off(&trk[p], note);
        sq_perf_note(p,note,0);
    }
}
static void cr_mpedal_up(uint32_t p)
{
    uint32_t n;
    cr_mchan(p)->pedal = 0;
    for (n = 0; n < 128u; n++)
        if (cr_mnote[p][n] & CR_MPED)
            cr_mrelease(p, n);
}
static void cr_mforget(uint32_t p)               /* a panic: the part's MIDI notes forgotten (their offs end nothing) */
{
    if (p < 2u) {
        memset(cr_mnote[p], 0, sizeof cr_mnote[p]);
        cr_mheld[p] = 0;
    }
    midi_bend_q8[p] = midi_bend_target[p] = 0;
    trk[p].bend_raw = 0;
}
static void cr_mcontrol(uint32_t p, uint32_t cc, uint32_t v)
{
    cr_mch_t *c = cr_mchan(p);
    track_t *t = &trk[p];
    uint32_t n;
    switch (cc) {
    case 64:
        if (v >= 64u)
            c->pedal = 1;
        else
            cr_mpedal_up(p);
        break;
    case 120:                                    /* all sound off: ignores the pedal */
        trk_all_off(t);
        for (n = 0; n < NVOICE; n++)
            if (t->v[n].active)
                voice_kill(&t->v[n]);
        cr_mforget(p);
        break;
    case 121:                                    /* reset all controllers (mod_midi: wheel, pressure, expression) */
        c->bend = 0;
        c->rpn_msb = c->rpn_lsb = 127;
        cr_mbend(p);
        cr_mpedal_up(p);
        break;
    case 123:                                    /* all notes off: normal releases, the pedal honoured */
        for (n = 0; n < 128u; n++)
            if (cr_mnote[p][n] && !(cr_mnote[p][n] & CR_MPED)) {
                if (c->pedal)
                    cr_mnote[p][n] |= CR_MPED;
                else
                    cr_mrelease(p, n);
            }
        if (!cr_mheld[p])
            trk_all_off(t);
        break;
    case 101: c->rpn_msb = (uint8_t)v; break;
    case 100: c->rpn_lsb = (uint8_t)v; break;
    case 99: case 98: c->rpn_msb = c->rpn_lsb = 127; break;   /* NRPN selected: no RPN data entry */
    case 6: case 38:
        if (!c->rpn_msb && !c->rpn_lsb) {        /* RPN 0, pitch bend sensitivity */
            if (cc == 6u)
                c->semis = (uint8_t)(v > 24u ? 24u : v);
            else
                c->cents = (uint8_t)(v > 99u ? 99u : v);
            cr_mbend(p);
        }
        break;
    default: break;
    }
}
static void cgs_midi_takeover(unsigned part);
/* st: the status' high nibble (0x80..0xE0); p: the part 0 / 1 (cr_midi.c crm_map) */
static void __attribute__((noinline)) midi_event(uint32_t st, uint32_t p, uint32_t d1, uint32_t d2)
{
    if (p >= 2u)
        return;
    if (st == 0x90u && d2) {
        if (!cr_mheld[p])cgs_midi_takeover(p);
        if (cr_mnote[p][d1])                     /* a repeated note replaces the previous press (a pedal-held one too) */
            cr_mrelease(p, d1);
        cr_mbend(p);
        trk_note_on(&trk[p], d1, d2);
        sq_perf_note(p,d1,d2);
        cr_mnote[p][d1] = 1;
        if (cr_mheld[p] < 128u)
            cr_mheld[p]++;
    } else if (st == 0x80u || st == 0x90u) {
        if (cr_mnote[p][d1] && cr_mchan(p)->pedal)
            cr_mnote[p][d1] |= CR_MPED;
        else
            cr_mrelease(p, d1);
    } else if (st == 0xE0u) {
        cr_mchan(p)->bend = (int16_t)((int32_t)(d1 | d2 << 7) - 8192);
        cr_mbend(p);
    } else if (st == 0xB0u) {
        mod_midi(&trk[p], st, d1, d2);
        cr_mcontrol(p, d1, d2);
    } else if (st == 0xD0u) {
        mod_midi(&trk[p], st, d1, d2);
    }
}

/* the keys' chord and bass still held (HOLD, or fingers) after their part's notes were released for another sound:
 * struck again on it (the owner, 2026-10-21: "Играет дальше новым пэдом"). At the bar line a pattern's sound comes in
 * (cf_sound.c cf_stage_in); stopped, a load is now (PRESETS, another pattern opened: panic_req, below). An arpeggio
 * strikes its next note itself */
static void cf_restrike(uint32_t part)
{
    track_t *t = &trk[part];
    uint32_t i;
    if (cfa.on)
        return;
    if (part == cfp_cp)
        for (i = 0; i < cfp.n; i++)
            trk_note_on(t, cfp.note[i], cfp_cfg.vel);
    if (part == 1u)
        for (i = 0; i < cfp.nb; i++)
            trk_note_on(t, cfp.bass[i], cfp_cfg.vel);
}

/* fx.c's, between two rendered blocks (after cr_audio_block): the releases asked for, the engine switches. A MIDI in
 * queue that overflowed (a lost note-off) is dropped and every part released: no stuck note */
static void events_block(uint32_t n)
{
    uint32_t i, pr = panic_req;
    (void)n;
    panic_req = 0;
    if (midi_in_overflow) {
        mi_r = mi_w;
        pr |= (1u << NTRK) - 1u;
        RING_PUBLISH();
        midi_in_overflow = 0;
    }
    for (i = 0; i < NTRK; i++) {
        if ((pr >> i) & 1u) {
            if (i < 2u)
                cr_mchan(i)->pedal = 0;
            cr_mforget(i);
            trk_all_off(&trk[i]);
            cf_restrike(i);                      /* (the keys' held notes on the new sound) */
        }
        engine_block(&trk[i]);                   /* engine switch: fade, then switch (voice.c) */
    }
}
#endif /* !FELUCCA_SEQ */

/* ------------------------------------------------------------ the queue --- */
/* UI -> ISR. One producer (the main loop), one consumer (the audio ISR): the index stores are the publication */
enum {
    CFE_CAPTURE,      /* a: 1 arm at the next accompaniment bar, 0 finish */
    CFE_PERFORM,      /* a: CFPERF_FILL / DROP / BACK, next-bar transient moment */
    CFE_ACCOMP,       /* a: stop/start/Studio pause or restart */
    CFE_KEY,          /* a: key 0..26, v: down */
    CFE_CONFIG,       /* the chord settings: cfc_shadow (cf_play.c) */
    CFE_PANIC,
    CFE_METRO,        /* a: on, b: level 0..100 */
    CFE_TEMPO,        /* v: BPM 30..300 */
    CFE_SIG,          /* v: beats a bar 1..12 */
    CFE_TRANSPORT,    /* v: 1 start (from beat 1), 0 stop */
    CFE_SEQ,          /* the sequencer: a 0 stop, 1 play pattern b (SQ_NPAT: the song from place v), 2 drum b heard (kit v),
                       * 3 pattern b next (the song from place v), 4 a take on pattern b, track v, 5 the take ends,
                       * 6 a take from HOME on pattern b, track v (8 bars at most), 7 the tracks' bars read again,
                       * 8 REC went down b ms ago (the chord begun since is a take's: cf_seqp.c sqp_pre) */
    CFE_ROOT,         /* a root from outside (NOTES IN Roots): a its MIDI note, b its age in ms, v down */
    CFE_DRUM,         /* the drum machine (cf_drum.c): a 0 stop, 1 play rhythm b (or next at the bar line), 2 the one
                       * playing's kit and SWING, 3 REC in the SEQUENCER: pattern b, track v, after the rhythm's bar,
                       * 4 a take under the rhythm: pattern b (its DRUMS the rhythm) takes over now. v: kit | SWING << 8 */
};
typedef struct { uint8_t op, a, b, rsv; int16_t v; } cf_ev_in_t;   /* (CFE_KEY: a key, b its age in ms, v down) */
#define CF_EVQ 128u                              /* a power of two */
static cf_ev_in_t cf_evq[CF_EVQ];
static volatile uint32_t cf_evq_w, cf_evq_r;
static volatile uint32_t cf_evq_lost;

static int cf_post(uint32_t op, uint32_t a, uint32_t b, int32_t v)
{
    cf_ev_in_t *e;
    if (cf_evq_w - cf_evq_r >= CF_EVQ) {
        cf_evq_lost++;
        return 0;
    }
    e = &cf_evq[cf_evq_w % CF_EVQ];
    e->op = (uint8_t)op;
    e->a = (uint8_t)a;
    e->b = (uint8_t)b;
    e->v = (int16_t)v;
    RING_PUBLISH();
    cf_evq_w++;
    return 1;
}

/* -------------------------------------------------------- the metronome --- */
/* the beat clock runs while the transport plays (PLAY: from beat 1) or the metronome screen is open; the click is
 * a decaying sine (ChoralRoot's cr_click_mix): 1.5 kHz on beat 1, 1 kHz on the others */
static struct {
    uint8_t on;                  /* the click sounds (SEQ open, recording, or the transport running) */
    uint8_t running;             /* the transport */
    uint8_t level;               /* 0..100 */
    uint8_t beats;               /* a bar */
    uint16_t bpm;
    uint32_t acc;                /* samples x bpm since the last beat, against FS x 60 */
    uint8_t beat;                /* the beat about to come (0 = the downbeat) */
    uint8_t click;               /* 1 a beat, 2 the downbeat: set for the mix */
    volatile uint32_t beats_done;
    uint32_t beat_ms;            /* the engine clock at the last beat */
} cfm = {0, 0, 0, 4, 120, 0, 0, 0, 0, 0};

#include "cr_midi.c"                             /* (MIDI in's pure part: the clock's follower, the channels' map) */
/* MIDI clock in (GLO > MIDI > CLOCK IN, cf_midi_in): while its pulses come, they run the beat (a pulse is 1/24 of
 * one) instead of the tempo, so the metronome, the arpeggiator and the sequencer follow the other instrument */
static struct {
    crm_clock_t f;               /* (cr_midi.c's tempo follower) */
    uint32_t pulses;             /* come since the last block */
    uint32_t last_ms;            /* the last one (cf_now_ms) */
    uint8_t active;              /* pulses within the last 300 ms */
    uint16_t bpm;                /* the tempo they make (0: none yet) */
} cfx_in;

/* In 6/8 the six visual counts are eighths; MIDI and TEMPO always count quarters. */
static uint32_t cf_meter_rate(void) { return cfm.beats == 6u ? 2u : 1u; }

static void cfm_tick(uint32_t n, uint32_t pulses)
{
    uint32_t k;
    if (!cfm.on && !cfm.running) {
        cf_grid.on = 0;
        return;
    }
    if (!cfm.acc && !cfm.beat && !cfm.beats_done) {
        cfm.click = 2;                           /* the first beat right away */
        cfm.beat = 1 % cfm.beats;
        cfm.beats_done++;
        cfm.beat_ms = cf_now_ms;
    }
    if (cfx_in.active || (cpa_running() && cfp_cfg.clock_in)) { /* the clock's pulses: FS x 60 / 24 each */
        cfm.acc += pulses * (FS * 60u / 24u) * cf_meter_rate();
    } else {
        cfm.acc += n * cfm.bpm * cf_meter_rate();
    }
    for (k = 0; k < 4u && cfm.acc >= FS * 60u; k++) {
        cfm.acc -= FS * 60u;
        cfm.click = cfm.beat == 0 || (cfm.beats == 6u && cfm.beat == 3u) ? 2u : 1u;
        cfm.beat = (uint8_t)((cfm.beat + 1u) % cfm.beats);
        cfm.beats_done++;
        cfm.beat_ms = cf_now_ms;
#ifdef CF_TRACE
        fprintf(stderr, "%6u beat %u\n", (unsigned)cf_now_ms, (unsigned)cfm.beats_done);
#endif
    }
    cf_grid.on = 1;                              /* (the arpeggiator's lock: where the beat is, in 1/24 beats) */
    {   /* Keep the arp/MIDI grid in quarter-note units (24 PPQN), including its old origin. */
        uint32_t q = 24u / cf_meter_rate();
        cf_grid.g24 = 24u + (cfm.beats_done - 1u) * q + cfm.acc / (FS * 60u / q);
    }
}
/* MIDI clock out (CLOCK Out): while the beat runs (PLAY, the sequencer), 24 pulses a beat on the metronome's
 * own grid (cf_grid.g24: a beat's first pulse is its downbeat), a start when it begins, a stop when it ends. USB MIDI
 * CIN 0x0F: a single-byte system real-time message (as ChoralRoot's cr_out.c sent them) */
static struct { uint8_t was; uint32_t g24, gen; } cfo;
static void cf_clock_out(void)
{
    int run = cfp_cfg.clock_out && cfp_cfg.midi_out && cfm.running && !cfx_in.active;
    if (run != cfo.was) {
        cfo.was = (uint8_t)run;
        midi_out_event(0x0Fu | (run ? 0xFAu : 0xFCu) << 8);
        cfo.g24 = cf_grid.g24;
        cfo.gen = cf_grid.gen;
        if (run)
            midi_out_event(0x0Fu | 0xF8u << 8);  /* (the downbeat's pulse with the start) */
        return;
    }
    if (!run)
        return;
    if (cfo.gen != cf_grid.gen) {                /* the beat started again from 1 (a new take, the sequencer's PLAY):
                                                  * start again with it */
        cfo.gen = cf_grid.gen;
        cfo.g24 = cf_grid.g24;
        midi_out_event(0x0Fu | 0xFAu << 8);
        midi_out_event(0x0Fu | 0xF8u << 8);
        return;
    }
    while (cfo.g24 < cf_grid.g24) {
        cfo.g24++;
        midi_out_event(0x0Fu | 0xF8u << 8);
    }
}

static void cfm_restart(void)
{
    cfm.acc = 0;
    cfm.beat = 0;
    cfm.beats_done = 0;
    cf_grid.gen++;
}

#include "cf_loop.c"                             /* (the old looper's saved format: read once, cf_sequi.c) */
#include "cf_seqd.c"                             /* (the sequencer's patterns and song) */
#include "cf_seqp.c"                             /* (.. playing them) */
#include "cf_drum.c"                             /* (the drum machine's rhythms and their player, 0.6) */
#include "cf_accomp.c"
#include "cf_lab_audio.c"

/* the metronome's sounds: a decaying resonator (the accent on the downbeat a different pitch), some with noise.
 * a1 = 2 r cos w and a2 = r^2 (Q14), s = sin w (x 10000) sets the start; len: samples to -60 dB; noise: % */
typedef struct { const char *name; int16_t a1_acc, s_acc, a1, s, a2; uint16_t len; uint8_t noise; } cf_click_t;
static const cf_click_t CF_CLICKS[] = {
    {"Beep", 30673, 3480, 32008, 2074, 16335, 4601, 0},   /* (ChoralRoot's) */
    {"Wood", 32105, 1772, 32364, 1251, 16237, 1531, 0},
    {"Click", 28266, 4907, 29710, 4015, 16058, 687, 35},
    {"Hat", 17511, 8400, 20482, 7729, 15896, 457, 100},
    {"Rim", 31369, 2674, 31605, 2398, 16172, 1059, 30},
};
#define CF_NCLICKS ((uint32_t)NELEM(CF_CLICKS))
static volatile uint8_t cf_click_sound;          /* CF_CLICKS index (the UI's) */
static int32_t cf_ck_y1, cf_ck_y2, cf_ck_a1, cf_ck_a2, cf_ck_amp, cf_ck_ne, cf_ck_nd;
static uint32_t cf_ck_n, cf_ck_rng = 0x1234567u;
static uint8_t cf_ck_noise;
static void cf_click_mix(int32_t *out, uint32_t n)
{
    uint32_t i;
    if (cfm.click) {
        const cf_click_t *k = &CF_CLICKS[cf_click_sound < CF_NCLICKS ? cf_click_sound : 0];
        int32_t lv = sqp.click && !cfm.level ? 100 : (int32_t)cfm.level;   /* (a count-in and a take are
                                                  * always heard: METRONOME's LEVEL never set (0), 100, its beat 1 a
                                                  * little over a usual chord, the median of the 51 sounds' peaks
                                                  * +1 dB: "я бы наоборот чуть громче сделал", the owner, 2026-10-20;
                                                  * a LEVEL the player set is used as it is) */
        int32_t amp = (cfm.on || cfm.running) ? 100 * lv : 0;
        if (cpa.on && cpa.paused) amp = 0; /* Click gain is independent of the selected drum groove. */
        int acc = cfm.click == 2u;
        if (!fx_usb_fixed)                       /* MASTER on it, as on the music: it was mixed in after MASTER, and the
                                                  * volume knob at its bottom left the click as loud as ever (the
                                                  * owner, 2026-10-20: "орёт при записи. Даже когда громкость синта на
                                                  * минимум"). With USB Level Fixed MASTER comes after it (audio.c) */
            amp = amp * (int32_t)song.master_q12 >> 12;
        cf_ck_a1 = acc ? k->a1_acc : k->a1;
        cf_ck_a2 = k->a2;
        cf_ck_y1 = amp * (acc ? k->s_acc : k->s) / 10000;
        cf_ck_y2 = 0;
        cf_ck_n = amp ? k->len : 0u;
        cf_ck_amp = acc ? amp : amp * 3 / 4;
        cf_ck_noise = k->noise;
        cf_ck_ne = cf_ck_amp * 2;                /* the noise's envelope, Q12 steps down over len */
        cf_ck_nd = k->len ? cf_ck_ne / (int32_t)k->len + 1 : cf_ck_ne;
        cfm.click = 0;
    }
    for (i = 0; i < n && cf_ck_n; i++, cf_ck_n--) {
        int32_t y = ((cf_ck_a1 * cf_ck_y1) >> 14) - ((cf_ck_a2 * cf_ck_y2) >> 14), o;
        cf_ck_y2 = cf_ck_y1;
        cf_ck_y1 = y;
        o = y * (100 - cf_ck_noise) / 100;
        if (cf_ck_noise) {
            int32_t w;
            cf_ck_rng ^= cf_ck_rng << 13;
            cf_ck_rng ^= cf_ck_rng >> 17;
            cf_ck_rng ^= cf_ck_rng << 5;
            w = (int32_t)(cf_ck_rng & 0xFFFFu) - 32768;
            o += (int32_t)(((int64_t)w * cf_ck_ne) >> 15) * cf_ck_noise / 100;
            cf_ck_ne = cf_ck_ne > cf_ck_nd ? cf_ck_ne - cf_ck_nd : 0;
        }
        out[2u * i] += o;
        out[2u * i + 1u] += o;
    }
}

/* ------------------------------------------------------------- MIDI in --- */
/* ISR -> UI: the notes an outside keyboard plays when NOTES IN is not Direct (the UI makes them the FM-1's keys, or
 * roots: cf_ui.c cf_ui_pump). One producer (the audio ISR), one consumer (the main loop); a full queue drops a note-on
 * (its note-off then finds nothing down) */
#define CF_XQ 64u                                /* a power of two */
static uint16_t cf_xq[CF_XQ];                    /* note | down << 7 */
static uint32_t cf_xq_ms[CF_XQ];                 /* when it came (fm1_ms) */
static volatile uint32_t cf_xq_w, cf_xq_r;
static void cf_xq_put(uint32_t note, uint32_t down, uint32_t ms)
{
    uint32_t w = cf_xq_w;
    if (w - cf_xq_r >= CF_XQ && down)
        return;
    if (w - cf_xq_r >= CF_XQ)
        cf_xq_r++;                               /* (a note-off never lost: the oldest goes) */
    cf_xq[w % CF_XQ] = (uint16_t)(note | (down ? 0x80u : 0u));
    cf_xq_ms[w % CF_XQ] = ms;
    RING_PUBLISH();
    cf_xq_w = w + 1u;
}

/* usb.c's midi_in_q (USB and TRS) is drained here: notes on the chord / bass channels play the parts directly
 * (cr_midi.c crm_map, ChoralRoot's), with the pedal, bend and controllers (midi_event above) */
static uint8_t cfx_direct;                       /* NOTES IN was Direct at the last look (cf_midi_in) */
static void cf_midi_in(void)
{
    uint32_t w = mi_w;
    uint8_t en[2] = {1, 1}, ch[2];
    ch[0] = cfp_cfg.ch_chord;
    ch[1] = cfp_cfg.ch_bass;
    if (cfx_direct && cfp_cfg.notes_in != CF_NI_DIRECT) {   /* Direct left: the notes it holds go up, as the FM-1
                                                  * keys' and the Roots' do on K3 (cf_ui.c cf_xnotes_up_all): their
                                                  * offs now go the other way and would end nothing (0.5.1) */
        uint32_t p, n;
        for (p = 0; p < 2u; p++)
            for (n = 0; n < 128u; n++)
                cr_mrelease(p, n);
    }
    cfx_direct = cfp_cfg.notes_in == CF_NI_DIRECT;
    if (midi_in_overflow)
        return;
    while (mi_r != w) {
        uint32_t at = mi_r % MQ, pkt = midi_in_q[at], status = (pkt >> 8) & 0xFFu;
        uint32_t d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
        mi_r++;
        if (status >= 0xF8u) {                   /* MIDI clock in (CLOCK IN On): the pulses, start, continue, stop */
            if (!cfp_cfg.clock_in)
                continue;
            if (status == 0xF8u) {
                int b = crm_clock_pulse(&cfx_in.f, midi_in_ms[at]);
                if (b) {
                    cfx_in.bpm = (uint16_t)b;
                    cfm.bpm = (uint16_t)b;
                }
                cfx_in.pulses++;
                cfx_in.last_ms = cf_now_ms;
            } else if (cpa.on) {
                if (status == 0xFAu || status == 0xFBu || status == 0xFCu) cfx_in.pulses = 0;
                cpa_realtime(status);
            } else if (status == 0xFAu || status == 0xFBu) {   /* start / continue: from beat 1 */
                cfm_restart();
                cfm.running = 1;
            } else if (status == 0xFCu) {        /* stop */
                cfm.running = 0;
            }
            continue;
        }
        if (cfp_cfg.notes_in != CF_NI_DIRECT && (status & 0xE0u) == 0x80u) {   /* a note on or off: IN CH's, or any
                                                  * channel with IN CH All (0.5.1, GitHub #2: a Digitakt's other tracks
                                                  * played chords) */
            if (!cfp_cfg.in_ch || (status & 0x0Fu) == cfp_cfg.in_ch - 1u)
                cf_xq_put(d1, (status & 0xF0u) == 0x90u && d2, midi_in_ms[at]);
            continue;
        }
        if (status >= 0x80u && status < 0xF0u) {
            crm_act_t a[2];
            int i, n = crm_map(status, d1, d2, en, ch, a);
            for (i = 0; i < n; i++)
                if (a[i].kind == CRM_FORWARD)
                    midi_event(status & 0xF0u, a[i].part, d1, d2);
        }
    }
}

/* ------------------------------------------------------- the audio side --- */
static volatile uint8_t cf_ready;

static void cf_apply(const cf_ev_in_t *e)
{
    if(e->op==CFE_PANIC || e->op==CFE_ACCOMP || e->op==CFE_SEQ || e->op==CFE_DRUM || e->op==CFE_TRANSPORT)cla_cancel(0);
    if(e->op==CFE_CONFIG)cla_cancel(1);
    switch (e->op) {
    case CFE_CAPTURE:
        if(e->a){
            sq_capture.tick=sq_capture.full=0;
            sq_capture.state=SQ_PERF_ARMED;
            if(!cpa.on)sq_perf_finish();
        } else {
            sq_perf_finish();
            if(e->b)cpa_stop();
        }
        break;
    case CFE_KEY:
        if (e->v) {
            sqp.rage = e->b;                     /* (its age, ms: a take puts it back where it was played) */
            sqp_pre.key_ms = cf_now_ms - e->b;
        }
        cfp_key(e->a, e->v);
        break;
    case CFE_ROOT:
        if (e->v) {
            sqp.rage = e->b;
            sqp_pre.key_ms = cf_now_ms - e->b;
        }
        cfp_key(CF_KEY_EXT + e->a, e->v);
        break;
    case CFE_CONFIG: cfp_config(); cpa_arp_config(); break;
    case CFE_ACCOMP:
        if (e->a >= CGS_PLAY_PAUSE) cgs_live_transport(e->a);
        else if (e->a) cpa_start(e->b);
        else {
            if(cgs_active() && cpa_setup.flags && cpa_setup.plan.studio && !cgs.queued_palette){
                cpa_setup.plan=cpa.plan;
                cgs.setup_pitch=cgs.pitch;
            }
            cpa_stop();
        }
        break;
    case CFE_PANIC:
        cpa_stop();
        sqp_stop();
        dmp_stop();
        cfp_panic();
        break;
    case CFE_SEQ:
        if (e->a == 1)
            sqp_start(e->b, (uint32_t)e->v);
        else if (e->a == 2)
            sqp_audition(e->b, (uint32_t)e->v);
        else if (e->a == 3)
            sqp_queue(e->b, (uint32_t)e->v);
        else if (e->a == 4)
            sqp_rec_start(e->b, (uint32_t)e->v);
        else if (e->a == 5)
            sqp_rec_stop();
        else if (e->a == 6) {                    /* (a take from HOME: into a new pattern, 8 bars at most) */
            sqp_rec_start(e->b, (uint32_t)e->v);
            sqp.rlim = SQ_BARS;
        } else if (e->a == 7) {                  /* (its tracks' bars changed: each track's bar read again) */
            if (sqp.on)
                sqp_bars();
        } else if (e->a == 8)                    /* (REC went down, e->b ms ago: the chord begun since is a take's) */
            sqp_rec_down(e->b);
        else
            sqp_stop();
        break;
    case CFE_PERFORM:
        if ((cpa.on && !cpa.paused && !cpa.clock_wait && !cpa.clock_lost && dmp.on) ||
            (!cpa.on && dmp.on && !dmp.rin) ||
            (sqp.on && !sqp.cin && sqp.rec == 0xFFu &&
             !((sq_st.pat[sqp.pat % SQ_NPAT].mute >> SQ_DRUMS) & 1u) &&
             sq_own(&sq_st.pat[sqp.pat % SQ_NPAT], SQ_DRUMS))) cf_perf_queue(e->a);
        break;
    case CFE_DRUM:
        if (cpa.on) {
            if (e->a == 0) cpa_stop();
            else if (e->a == 1 && e->b < DM_N && DM_RHY[e->b].beats == cpa.plan.beats) {
                if (!cpa_edit.flags) cpa_edit.plan = cpa.plan;
                cpa_edit.plan.rhythm = e->b;
                cpa_edit.plan.kit = (uint8_t)e->v % SQ_NKIT;
                cpa_edit.plan.swing = (uint8_t)(e->v >> 8) % 101u;
                cpa_edit.flags |= CPA_GROOVE;
            } else if (e->a == 2) {
                if (!cpa_edit.flags) cpa_edit.plan = cpa.plan;
                cpa_edit.plan.kit = (uint8_t)e->v % SQ_NKIT;
                cpa_edit.plan.swing = (uint8_t)(e->v >> 8) % 101u;
                cpa_edit.flags |= CPA_GROOVE;
            }
            break;
        }
        if (e->a == 1)
            dmp_queue(e->b, (uint32_t)e->v & 0xFFu, ((uint32_t)e->v >> 8) & 0xFFu);
        else if (e->a == 2 && dmp.on) {
            dmp.kit = (uint8_t)(e->v & 0xFF);
            dmp.swing = (uint8_t)((e->v >> 8) & 0xFF);
        } else if (e->a == 3)
            dmp_rec_in(e->b, (uint32_t)e->v);
        else if (e->a == 4)
            dmp_take_over(e->b);
        else if (e->a == 5)
            dmp.rin = 0;
        else
            dmp_stop();
        break;
    case CFE_METRO:
        /* A queued foreground update must not undo a newer bar-boundary Click enable. */
        if((e->a&0x80u) && cpa.on && (uint16_t)e->v!=(uint16_t)cpa.click_generation)break;
        if ((e->a&1u) && !cfm.on && !cfm.running)
            cfm_restart();
        cfm.on = e->a&1u;
        cfm.level = e->b;
        break;
    case CFE_TEMPO:
        if(cfp_cfg.clock_in || (CF_STUDIO && e->a!=cgs.tempo_generation))break;
        cfm.bpm=(uint16_t)(e->v<30?30:e->v>300?300:e->v);
        if(CF_STUDIO)cfp_cfg.bpm=cfm.bpm;
        break;
    case CFE_SIG: if (cpa.on) break; cfm.beats = (uint8_t)(e->v < 1 ? 1 : e->v > 12 ? 12 : e->v); break;
    case CFE_TRANSPORT:
        if (cpa.on) { if (!e->v) cpa_stop(); break; }
        if (e->v)
            cfm_restart();
        cfm.running = (uint8_t)(e->v != 0);
        break;
    default: break;
    }
}

static void cf_out_init(void)                    /* power-on (the audio ISR may already run: IRQ off) */
{
    fm1_irq_off();
    cf_ready = 0;
    if (master_pot_q12)                          /* MASTER: the pot's level, before the first block */
        fx_master_now(master_pot_q12 - 1u);
    voice_fade_steal = 1;                        /* voice.c: a stolen voice fades out, its new note waits a block */
    fx_smooth = 1;                               /* fx.c: the limiter's gain eased, no audio-rate crackle */
    fx_output_set(1u, 1);                        /* fx.c's Clean output (ChoralRoot 0.19): the limiter a safety at
                                                  * -2 dBFS that holds its gain, a knee above, MASTER last; the
                                                  * Classic one (-5 dB, 0.15 s release) pumped under a big reverb */
#if CR_FX
    xf_defaults();                               /* cr_fx.c: the delay's and the reverb's parameters */
#endif
    cfp_init();
    cf_now_ms = cf_ms_rem = 0;
    cf_evq_r = cf_evq_w = 0;
    cf_ready = 1;
    fm1_irq_on();
}

/* the audio ISR, once per CTL-sample block, before the block renders: input, the player, the metronome */
static void cf_audio_block(uint32_t n)
{
    uint32_t w = cf_evq_w;
    if (!cf_ready)
        return;
    while (cf_evq_r != w) {
        cf_apply(&cf_evq[cf_evq_r % CF_EVQ]);
        cf_evq_r++;
    }
    cf_midi_in();
    if(cgs_active())cgs_apply_mute();
    cla_process(n);
    cf_ms_rem += n * 1000u;
    while (cf_ms_rem >= FS) {
        cf_ms_rem -= FS;
        cf_now_ms++;
    }
    {
        uint32_t pulses = cfx_in.pulses;         /* (MIDI clock in: the pulses come since the last block) */
        cfx_in.pulses = 0;
        cfx_in.active = cfp_cfg.clock_in && cfx_in.bpm && (uint32_t)(cf_now_ms - cfx_in.last_ms) < 300u;
        cpa_arp_service();
        cfa_tick(n);                             /* (the arpeggiator: its steps, its strum) */
        sqp_arp_tick(n);                         /* (the sequencer's tracks' with ARP on) */
        sqp_pre_watch();                         /* (since REC went down, before a take begins) */
        if (cpa.on) cpa_tick(n, pulses);
        else {
        sqp_tick(n, pulses);                     /* (the sequencer) */
        dmp_tick(n, pulses);                     /* (.. or the drum machine's rhythm) */
        }
        if (!cpa.on) cfm_tick(n, pulses);
        cf_clock_out();
    }
    cge_block(n);
}

/* ISR-owned scalar activity mailbox; no retained sample buffer or new clock. */
static volatile uint8_t cf_visual_want=1;
static uint16_t cf_visual_peak;
static uint16_t cf_visual_window_peak;
static uint32_t cf_visual_window_ms;
static uint8_t cf_visual_parts,cf_visual_muted;
static uint32_t cf_visual_ms;
static void cf_visual_publish(uint32_t peak)
{
    cf_visual_peak=(uint16_t)(peak>32767?32767:peak);cf_visual_ms=cf_now_ms;
    cf_visual_muted=song.master_q12==0;
    /* A foreground frame can fall between short drum hits. Retain the maximum
     * until the existing IRQ-bounded snapshot consumes this display window. */
    if(!cf_visual_want || cf_visual_muted || cf_now_ms-cf_visual_window_ms>100)cf_visual_window_peak=0;
    if(cf_visual_want && !cf_visual_muted && cf_visual_peak>cf_visual_window_peak) {
        cf_visual_window_peak=cf_visual_peak;cf_visual_window_ms=cf_now_ms;
    }
    cf_visual_parts=0;
    if(sqp.on && sq_perf[sqp.pat].bars)
        for(unsigned p=0;p<SQ_DRUMS;p++)
            if(sq_perf_held[p][0] || sq_perf_held[p][1] || sq_perf_held[p][2] || sq_perf_held[p][3])
                cf_visual_parts|=1u<<p;
    if(trk[0].p[P_LEVEL] && (cfp.n || sqp.dn || cla.on))cf_visual_parts|=1;
    if(trk[1].p[P_LEVEL] && (cfp.nb || sqp.nb))cf_visual_parts|=2;
    if(trk[2].p[P_LEVEL] && sqp.ln)cf_visual_parts|=4;
    if(trk[3].p[P_LEVEL] && sqp.lit_t[SQ_DRUMS] && cf_now_ms-sqp.lit_t[SQ_DRUMS]<150)cf_visual_parts|=8;
}

/* ---------------------------------------------------- what the UI reads --- */
typedef struct {
    cf_lab_chord_t audition;
    uint8_t audition_on,audition_blocked;
    uint32_t visual_ms;
    uint16_t visual_peak;
    uint8_t visual_parts,visual_muted;
    uint32_t ac_revision,ac_mask[CF_P_MAX];
    uint8_t ac_n,ac_root[CF_P_MAX],ac_degree[CF_P_MAX];
    int8_t ac_bass_pc[CF_P_MAX];
    uint8_t ac_waiting, ac_lost, studio_arp_step;
    uint8_t ac_kit, ac_swing, ac_repeat_off, ac_next_beats, ac_next_mode, ac_next_kit, ac_next_swing, ac_next_repeat_off;
    uint8_t ac_on, ac_paused, ac_slot, ac_idea, ac_tonic, ac_scale, ac_beats, ac_rhythm, ac_mode, ac_next, ac_edits, ac_next_tonic, ac_next_scale, ac_next_rhythm;
    cf_chord_out_t out;          /* the chord model's answer */
    uint8_t n, note[CF_MAX_NOTES], nb, bass[2];
    uint8_t onset;
    uint32_t root_press; /* gesture count for the foreground LED guide */
    uint8_t beat, metro_on, running, metro_level;
    uint32_t beats_done, beat_ms, beat_acc, now_ms;
    uint32_t studio_keys;        /* the highlighted pitched part, folded onto the panel */
    uint32_t keys_held;          /* bit k: key k is down in the chord model's view */
    uint8_t arp_on, arp_latched, arp_steps, arp_note, arp_n, arp_note_list[CF_MAX_NOTES];
    uint32_t arp_idx;            /* the arpeggiator: steps played, the last note, the chord it plays */
    cf_arp_view_t arp_view;
    uint8_t ext_clock;           /* MIDI clock in runs the beat */
    uint16_t ext_bpm;            /* .. at this tempo */
    uint8_t sq_on, sq_song, sq_pat, sq_slot, sq_bar;   /* the sequencer: playing, the song, where (bar: in the
                                                        * pattern, 0 .. its length - 1) */
    uint32_t sq_t, sq_bt;        /* .. ticks into the bar, of the bar */
    uint32_t sq_n;               /* .. bars since the pattern began (each track's bar: cf_seqd.c sq_trk_bar) */
    uint8_t sq_lit[SQ_NTRK];     /* .. each track's step that sounded last */
    uint32_t sq_lit_t[SQ_NTRK];  /* .. when */
    uint32_t sq_lane_t[SQ_NLANE];   /* .. each drum's last hit */
    uint8_t sq_rec, sq_next;     /* .. a take's track (0xFF none); the pattern coming in next (0xFF none) */
    uint32_t sq_cin;             /* .. count-in ticks left */
    uint8_t sq_ckey, sq_lkey;    /* .. the chord, the layer sounding: its root key (0xFF), its buttons */
    uint16_t sq_cbtns, sq_lbtns;
    uint8_t sq_dn, sq_dnote[6], sq_droot, sq_dsingle, sq_dser, sq_bn, sq_bass[2];   /* .. the CHORDS track's chord
                                  * for HOME (cf_seqp.c sqp.dn ..), the bass sounding */
    uint32_t sq_dmask;
    uint8_t dm_step;
    /* Transient drum-moment metadata. Zero until an actual performance owner
     * publishes it; the renderer never guesses a fill from loudness. */
    uint8_t perf_pending, perf_fill, perf_dropped;
    uint32_t perf_bloom, perf_bloom_ms;
    uint8_t dm_on, dm_r, dm_next, dm_bar, dm_rin;   /* the drum machine: a rhythm plays, which, the next, its bar;
                                  * REC in the SEQUENCER waits for its bar line (pattern + 1) */
    uint32_t dm_t, dm_left;      /* .. ticks into its bar; ticks till REC in's pattern takes over */
} cf_sn_t;
static cf_sn_t cf_sn;

/* The panel spans MIDI F3 through G5. Fold outside pitches to its nearest octave. */
static unsigned cf_panel_pitch_key(unsigned note)
{
    while(note<53u)note+=12u;
    while(note>79u)note-=12u;
    return note-53u;
}

static uint32_t cf_sounding_panel_keys(unsigned part)
{
    uint32_t keys=0;
    for(unsigned i=0;i<NVOICE;i++){
        const voice_t *v=&trk[part].v[i];
        if(v->active && v->gate && v->stage!=4u)keys|=1u<<cf_panel_pitch_key(v->note);
    }
    return keys;
}

static void cf_snapshot(void)
{
    uint32_t i;
    fm1_irq_off();
    cf_sn.audition=cla.chord;cf_sn.audition_on=cla.on;cf_sn.audition_blocked=cla.blocked;
    cf_sn.visual_ms=cf_visual_ms;cf_sn.visual_peak=cf_visual_window_peak;cf_visual_window_peak=0;
    cf_sn.visual_parts=cf_visual_parts;cf_sn.visual_muted=cf_visual_muted;
    cf_sn.ac_revision = cpa.revision;cf_sn.ac_n=cpa.plan.n;
    for(unsigned i=0;i<CF_P_MAX;i++) {
        cf_sn.ac_mask[i]=cpa.plan.slot[i].mask;cf_sn.ac_root[i]=cpa.plan.slot[i].root;
        cf_sn.ac_degree[i]=cpa.plan.degree[i];cf_sn.ac_bass_pc[i]=cpa.plan.slot[i].bass_pc;
    }
    cf_sn.ac_waiting=cpa.clock_wait;cf_sn.ac_lost=cpa.clock_lost;
    cf_sn.ac_kit = cpa.plan.kit; cf_sn.ac_swing = cpa.plan.swing; cf_sn.ac_repeat_off=cpa.plan.repeat_off;
    cf_sn.ac_on = cpa.on; cf_sn.ac_paused = cpa.paused; cf_sn.ac_slot = cpa.slot;
    cf_sn.ac_idea = cpa.plan.idea; cf_sn.ac_tonic = cpa.plan.tonic; cf_sn.ac_scale = cpa.plan.scale;
    cf_sn.ac_beats = cpa.plan.beats; cf_sn.ac_rhythm = cpa.plan.rhythm; cf_sn.ac_mode = cpa.plan.mode;
    cf_sn.ac_next = cpa.on && cpa_setup.flags ? cpa_setup.plan.idea : CF_P_NONE;
    cf_sn.ac_edits = cpa.on ? cpa_edit.flags : 0;
    const cpa_plan_t *next=cpa_setup.flags?&cpa_setup.plan:&cpa_edit.plan;
    cf_sn.ac_next_tonic=next->tonic; cf_sn.ac_next_scale=next->scale; cf_sn.ac_next_rhythm=next->rhythm;
    cf_sn.ac_next_beats=next->beats; cf_sn.ac_next_mode=next->mode;
    cf_sn.ac_next_kit=next->kit; cf_sn.ac_next_swing=next->swing; cf_sn.ac_next_repeat_off=next->repeat_off;
    cf_sn.studio_keys=CF_STUDIO && cgs.focus<CGS_DRUMS?cf_sounding_panel_keys(cgs.focus):0;
    if(CF_STUDIO){
        const cfa_t *arp=cgs_band_arp(cgs.focus)?&cfa_sq[cgs.focus==CGS_LAYER]:&cfa_k;
        cf_sn.studio_arp_step=arp->on && arp->n && arp->seen?(uint8_t)(1u+((arp->count-1u)&15u)):0;
    }
    cf_sn.out = cfc.out;
    cf_sn.n = cfp.n;
    for (i = 0; i < cfp.n; i++)
        cf_sn.note[i] = cfp.note[i];
    cf_sn.nb = cfp.nb;
    cf_sn.bass[0] = cfp.bass[0];
    cf_sn.bass[1] = cfp.bass[1];
    cf_sn.onset = cfp.onset;
    cf_sn.root_press = cfp.root_press;
    cf_sn.arp_on = cfa.on;
    cf_sn.arp_latched = cfa.latched;
    cf_sn.arp_steps = cfa.steps;
    cf_sn.arp_note = cfa.last_note;
    cf_sn.arp_idx = cfa.idx;
    cf_sn.arp_n = cfa.n;
    const cfa_t *av=&cfa_k;
    if (!(av->on && av->n)) { if(cfa_sq[0].on && cfa_sq[0].n)av=&cfa_sq[0];else if(cfa_sq[1].on && cfa_sq[1].n)av=&cfa_sq[1]; }
    cf_arp_view_t *v=&cf_sn.arp_view;
    v->n=av->n;v->on=av->on;v->idx=av->idx;v->count=av->count;v->seen=av->seen;
    v->onset_ms=av->onset_ms;v->interval_ms=av->interval_ms;v->root=av->root;v->mask=av->mask;
    v->mode=av->mode;v->latched=av->latched;v->strum_n=av->sn;v->strum_i=av->si;
    v->source=av==&cfa_k?0:av==&cfa_sq[1]?3:cpa.on?1:2;
    int pushed=av==&cfa_sq[0]?cge_arp_push():0;
    cf_arp_t saved;
    int generated=CF_STUDIO && av!=&cfa_k && cgs_arp_push(av->part,&saved);
    v->oct=cfp_cfg.arp.oct;v->rate=cfp_cfg.arp.rate;v->gate=cfp_cfg.arp.gate;v->swing=cfp_cfg.arp.swing;
    if(generated)cfp_cfg.arp=saved;
    cge_arp_pop(pushed);
    memcpy(v->trace,av->trace,sizeof v->trace);memcpy(v->note,av->note,sizeof v->note);memcpy(v->order,av->order,sizeof v->order);memcpy(v->strum,av->snote,sizeof v->strum);
    cf_sn.ext_clock = cfx_in.active;
    cf_sn.ext_bpm = cfx_in.bpm;
    for (i = 0; i < cfa.n; i++)
        cf_sn.arp_note_list[i] = cfa.note[i];
    cf_sn.beat = cfm.beat;
    cf_sn.metro_level=cfm.level;
    cf_sn.metro_on = cfm.on;
    cf_sn.running = cfm.running;
    cf_sn.beats_done = cfm.beats_done;
    cf_sn.beat_ms = cfm.beat_ms;
    cf_sn.beat_acc = cfm.acc;
    cf_sn.now_ms = cf_now_ms;
    cf_sn.sq_on = sqp.on;
    cf_sn.sq_rec = sqp.rec;
    cf_sn.sq_next = sqp.next;
    cf_sn.sq_cin = sqp.cin;
    cf_sn.sq_song = sqp.song;
    cf_sn.sq_pat = sqp.pat;
    cf_sn.sq_slot = sqp.slot;
    cf_sn.sq_bar = sqp.bar;
    cf_sn.sq_n = sqp.nbar;
    cf_sn.sq_t = sqp.t;
    cf_sn.sq_bt = sqp_bar_ticks();
    for (i = 0; i < SQ_NTRK; i++) {
        cf_sn.sq_lit[i] = sqp.lit[i];
        cf_sn.sq_lit_t[i] = sqp.lit_t[i];
    }
    for (i = 0; i < SQ_NLANE; i++)
        cf_sn.sq_lane_t[i] = sqp.lane_t[i];
    cf_sn.sq_ckey = sqp.on && sqp.n ? sqp.ckey : 0xFFu;
    cf_sn.sq_cbtns = sqp.on && sqp.n ? sqp.cbtns : 0u;
    cf_sn.sq_lkey = sqp.on && sqp.ln ? sqp.lkey : 0xFFu;
    cf_sn.sq_lbtns = sqp.on && sqp.ln ? sqp.lbtns : 0u;
    cf_sn.sq_dn = (sqp.on || cpa.on) ? sqp.dn : 0u;
    for (i = 0; i < 6u; i++)
        cf_sn.sq_dnote[i] = sqp.dnote[i];
    cf_sn.sq_droot = sqp.droot;
    cf_sn.sq_dmask = sqp.dmask;
    cf_sn.sq_dsingle = sqp.dsingle;
    cf_sn.sq_dser = sqp.dser;
    cf_sn.sq_bn = (sqp.on || cpa.on) ? sqp.nb : 0u;
    cf_sn.sq_bass[0] = sqp.bass[0];
    cf_sn.sq_bass[1] = sqp.bass[1];
    cf_sn.dm_step=dmp.nh?(uint8_t)(dmp.nh-1u):0;
    cf_sn.dm_on = dmp.on;
    cf_sn.dm_r = dmp.r;
    cf_sn.dm_next = dmp.next;
    cf_sn.dm_bar = dmp.bar;
    cf_sn.dm_t = dmp.t;
    cf_sn.perf_pending = cf_moment.pending;
    cf_sn.perf_fill = cf_moment.fill;
    cf_sn.perf_dropped = cf_moment.dropped;
    cf_sn.perf_bloom = cf_moment.bloom;
    cf_sn.perf_bloom_ms = cf_moment.bloom_ms;
    cf_sn.dm_rin = dmp.on ? dmp.rin : 0u;
    cf_sn.dm_left = dmp.rin ? (dmp.rbars - 1u) * sqp_bar_ticks() + (sqp_bar_ticks() - dmp.t) : 0u;
    fm1_irq_on();
}
