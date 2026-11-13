/* SPDX-License-Identifier: GPL-3.0-only
 * Catalogue accompaniment. UI prepares notes; audio consumes two bounded latest
 * mailboxes (new idea, active edits). No pattern allocation, theory, or storage
 * in the renderer. Chords/bass reuse the mutually exclusive SQ note owner.
 */
#define CPA_OFF 255u
#define CPA_CLICK 254u
enum { CPA_BOTH, CPA_DRUMS, CPA_NMODES };
enum { CPA_NOTES = 1, CPA_GROOVE = 2, CPA_PHRASE = 4 };
typedef struct {
    uint32_t mask;
    uint16_t ticks;
    uint8_t root, n, note[6], bass[2], nb, gate, tie;
    int8_t bass_pc;
} cpa_slot_t;
typedef struct {
    cpa_slot_t slot[CF_P_MAX];
    uint8_t idea, n, tonic, scale, beats, rhythm, mode, nb, kit, swing, repeat_off, click_start;
    uint8_t degree[CF_P_MAX], studio;
} cpa_plan_t;
typedef struct { cpa_plan_t plan; volatile uint8_t flags; } cpa_mail_t;
static cpa_mail_t cpa_setup, cpa_edit;
static volatile uint8_t cpa_commanded; /* foreground transport intent, including queued PLAY */
static struct {
    cpa_plan_t plan;
    uint32_t t, elapsed, acc, revision, q2;
    volatile uint8_t on;
    uint8_t paused, slot, clock_wait, clock_lost, clock_external;
    uint32_t clock_ms, click_generation;
} cpa;
static int cge_arp_wanted(void),cge_bass_enabled(void),cge_drums_enabled(void),cge_arp_push(void);
static void cge_start(void),cge_stop(void),cge_block(uint32_t),cge_arp_pop(int);
static int cge_bar(int);
static void cge_tick1(void);
static int cge_chord_gap(void);
static void sq_perf_capture_sounds(void);
#include "cf_studio.c"
static int cpa_running(void) { return cpa.on; }
static int cpa_valid(const cpa_plan_t *p)
{
    if ((p->idea != CF_P_ENGINE && p->idea != CF_P_DRAFT && !(p->studio && p->idea>=CF_P_COMPOSED && p->idea<CF_P_ENGINE) && p->idea >= cf_progression_count()) || !p->n || p->n > CF_P_MAX ||
        p->tonic > 11 || !cf_harmony_supported(p->scale, CF_Q_BASIC) ||
        !(p->beats == 2 || p->beats == 3 || p->beats == 4 || p->beats == 5 || p->beats == 6 || p->beats == 7) ||
        p->mode >= CPA_NMODES || p->repeat_off > 1 || p->click_start > 100 || p->nb > 2 || p->kit >= SQ_NKIT || p->swing > 100 ||
        (p->rhythm != CPA_OFF && p->rhythm != CPA_CLICK &&
         (p->rhythm >= DM_N || DM_RHY[p->rhythm].beats != p->beats))) return 0;
    for (unsigned i = 0; i < p->n; i++) {
        const cpa_slot_t *s = &p->slot[i];
        if (p->degree[i] > 6 || !s->ticks || s->ticks>64u*SQ_PPQN ||
            !s->n || s->n > 6 || !s->mask || s->root > 127 || s->nb>2 || s->gate<20 || s->gate>100 || s->tie>1 || s->bass_pc<0 || s->bass_pc>11) return 0;
        for (unsigned j = 0; j < s->n; j++)
            if (s->note[j] > 127 || (j && s->note[j] <= s->note[j-1])) return 0;
        for (unsigned j = 0; j < s->nb; j++) if (s->bass[j] > 127) return 0;
    }
    return 1;
}
/* A queued UI config follows the committed song at a bar boundary. */
static void cpa_overlay_source(void)
{
    if (!cpa.on) return;
    if(cpa.plan.idea==CF_P_ENGINE || (cpa.plan.studio && cpa.plan.idea==CF_P_DRAFT)){cfc.source=0;cfc.source_n=0;return;}
    if(cpa.plan.idea==CF_P_DRAFT)return;
    const cf_progression_t *p = cf_progression_get(cpa.plan.idea);
    cfc.source = cf_progression_source(p); cfc.source_n = cfc.source ? p->n : 0;
}
static void cpa_overlay_cfg(cf_chord_cfg_t *cfg)
{
    if (!cpa.on) return;
    cfg->tonic = cpa.plan.tonic; cfg->scale = cpa.plan.scale;
    cfg->quantize = CF_Q_BASIC;
}
/* The accompaniment borrows the existing CHORDS arp owner; recordings retain
 * their independent per-track ARP flags. No extra transport or allocation. */
static void cpa_arp_off(void)
{
    cfa_t *saved=cfa_at;cfa_at=&cfa_sq[0];cfa_stop();cfa.on=0;cfa_at=saved;
}
static int cpa_live_chord(void)
{ return cfp_cp==0 && (cfp.n || cfa_k.n); }
static void cpa_release_notes(void)
{
    cpa_arp_off();
    cla_cancel(0);
    sqp_chord_off(); sqp_bass_off(); sqp.dn = 0;
    if(cgs_active()){
        sqp_arp_off(CGS_CHORDS);
        cgs_release(CGS_LAYER);
        memset(cgs.gate,0,sizeof cgs.gate);
    }
    sqp_drum_offs();
}
static void cpa_release(void)
{
    cf_perf_reset();
    cpa_release_notes();
}
static void cpa_stop(void)
{
    sq_perf_finish();
    cpa_commanded = 0;cge_stop();
    if(CF_STUDIO){
        cgs.take_revision++;
        cgs.pending=cgs.pending_auto=cgs.clear_take_hold=0;
        cgs.queued_bpm=0;
        cgs_cancel_rolled_sounds();
    }
    cla_cancel(0);
    if (!cpa.on) return;
    if(cgs_active() && cpa_setup.flags){
        cgs.ready=0;
        cgs.meter=cpa.plan.beats;
    }
    cpa_release(); dmp_stop();
    cpa.on = cpa.paused = cpa.clock_wait = cpa.clock_lost = 0;
    cpa_setup.flags = cpa_edit.flags = 0;
    cfm.running = 0; cf_grid.on = 0; cpa.revision++;
}
static uint8_t cpa_tie_from;
static void cpa_emit(void)
{
    if(cla_active())return;
    if(cgs_active()) {
        const cpa_slot_t *s=&cpa.plan.slot[cpa.slot];
        sqp.dn=cpa.plan.mode==CPA_DRUMS?0:s->n;
        sqp.droot=s->root; sqp.dmask=s->mask;
        memcpy(sqp.dnote,s->note,s->n); sqp.dser++;
        cgs_tick();
        return;
    }
    const cpa_slot_t *s = &cpa.plan.slot[cpa.slot];
    if(cpa_tie_from && sqp.n==s->n && sqp.nb==(cfp_cfg.bass_on?s->nb:0) &&
       !memcmp(sqp.note,s->note,s->n) && !memcmp(sqp.bass,s->bass,sqp.nb)) {
        sqp.dmask=s->mask;sqp.droot=s->root;cpa_tie_from=0;return;
    }
    if(cpa_tie_from && cge_arp_wanted() && cfa_sq[0].n==s->n &&
       !memcmp(cfa_sq[0].note,s->note,s->n)) {cpa_tie_from=0;return;}
    cpa_tie_from=0;
    cpa_arp_off();
    if(cpa.plan.idea==CF_P_ENGINE && cpa_live_chord()){
        sqp_chord_off();sqp_bass_off();sqp.dn=s->n;return;
    }
    uint32_t gt = cfp.nb || cfa.on ? 0 : sqp_glide_ticks();
    if (cpa.plan.mode == CPA_DRUMS) {
        sqp_chord_off(); sqp_bass_off(); sqp.dn = 0; return;
    }
    sq_ev_t e = {0}; e.n = s->n;
    for (unsigned i = 0; i < s->n; i++) e.note[i] = s->note[i];
    if(cge_arp_wanted()) {
        sqp_chord_off();
        if(!cpa_live_chord()) {
            cf_chord_out_t o={0};o.sounding=1;o.n=s->n;o.root_pc=s->root%12;o.root_note=s->root;o.mask=s->mask;
            memcpy(o.note,s->note,s->n);cfa_t *saved=cfa_at;cfa_at=&cfa_sq[0];cfa.on=1;int pushed=cge_arp_push();cfa_chord(&o,s->n);cge_arp_pop(pushed);cfa_at=saved;
        }
    } else if (gt && (sqp.n || (cfp_cfg.glide_always && sqp.gn && cf_now_ms - sqp.gms < 4000u)))
        sqp_chord_glide(&e, gt, sqp.n != 0);
    else {
        sqp_chord_off();
        for (unsigned i = 0; i < s->n; i++) {
            sqp.note[i] = s->note[i];
            if (!cfp_holds(0, s->note[i])) cfp_on(0, s->note[i], cfp_cfg.vel);
        }
        sqp.n = s->n;
    }
    sqp.dn = sqp.gn = s->n;
    for (unsigned i = 0; i < s->n; i++) sqp.dnote[i] = sqp.gnote[i] = s->note[i];
    sqp.droot = s->root; sqp.dmask = s->mask; sqp.dsingle = 0; sqp.dser++;
    sqp_bass_on(s->bass, cfp_cfg.bass_on && cge_bass_enabled() ? s->nb : 0, s->ticks, gt);
}
static void cpa_arp_config(void)
{
    if(!cpa.on || cgs_active())return;
    if(cpa.paused || cpa.clock_wait){cpa_arp_off();return;}
    if(cpa.plan.idea==CF_P_ENGINE)return; /* scheduled settings wait for the next onset */
    if(cfa_sq[0].on!=cge_arp_wanted() && sqp.dn){cpa_tie_from=0;cpa_emit();}
    else if(cfa_sq[0].on){cfa_t *saved=cfa_at;cfa_at=&cfa_sq[0];int pushed=cge_arp_push();cfa_config(1);cge_arp_pop(pushed);cfa_at=saved;}
}
static void cpa_arp_service(void)
{
    if(!cpa.on || cgs_active())return;
    if(cpa.paused || cpa.clock_wait || cpa_live_chord() || cla_active()) {
        if(cfa_sq[0].on)cpa_arp_off();
        return;
    }
    /* Disabling ARP during a live takeover must still return to the current
     * scheduled chord on release, rather than waiting for its next slot. */
    if(sqp.dn && !cge_chord_gap() && (cge_arp_wanted()?!cfa_sq[0].n:!sqp.n)){cpa_tie_from=0;cpa_emit();}
}
static void cpa_groove(int reset)
{
    unsigned r = cpa.plan.rhythm;
    if (r >= DM_N) { dmp_stop(); cfm.running = 1; return; }
    if (!dmp.on || dmp.r != r || reset) dmp.bar = 0;
    else dmp.bar = (uint8_t)((dmp.bar + 1u) % DM_RHY[r].bars);
    dmp.r = (uint8_t)r; dmp.kit = cpa.plan.kit; dmp.swing = cpa.plan.swing;
    dmp.on = 1; dmp.next = 255; dmp.rin = 0;
    dmp.t = 0; dmp.nh = 0;
}
static void cpa_commit(void)
{
    cfm.beats = sq_beats = cpa.plan.beats;
    if(cpa.plan.idea==CF_P_DRAFT && !cpa.plan.studio && cfc.cfg.tonic==cpa.plan.tonic && cfc.cfg.scale==cpa.plan.scale && cfc.cfg.quantize==CF_Q_BASIC){cpa.revision++;return;}
    cf_chord_cfg_t cfg = cfc.cfg;
    cpa_overlay_source();
    cpa_overlay_cfg(&cfg); cf_chord_config(&cfc, &cfg); cfp_sync(1);
    cpa.revision++;
}
static void cpa_grid_start(void)
{
    cpa.q2 = 0; cfm_restart(); cfm.running = 1;
    cfm.click = 2; cfm.beat = 1; cfm.beats_done = 1; cfm.beat_ms = cf_now_ms;
    cf_grid.on = 1; cf_grid.g24 = 24;
}
static void cpa_capture_begin(void)
{
    unsigned bpm=cfp_cfg.clock_in && cfx_in.bpm>=30 && cfx_in.bpm<=300?cfx_in.bpm:cfm.bpm;
    sq_perf_draft.meta=(sq_take_meta_t){bpm,cpa.plan.beats,0};
    memset(sq_perf_draft.level,100,sizeof sq_perf_draft.level);
    sq_perf_draft.space=cgs_active()?cgs.space:100;
    if(cgs_active())memcpy(sq_perf_draft.level,cgs.level,sizeof sq_perf_draft.level);
    sq_perf_capture_sounds();
    sq_capture.state=SQ_PERF_ACTIVE;
    sq_perf_seed_held();
}
static void cpa_start(int capture)
{
    if (!cpa_valid(&cpa_setup.plan)) {
        if(capture){sq_capture.state=SQ_PERF_ARMED;sq_perf_finish();}
        return;
    }
    cpa_stop(); sqp_stop(); dmp_stop(); cf_perf_reset();
    /* stop cleared flags, but the immutable snapshot remains available. */
    cpa.plan = cpa_setup.plan; cpa_setup.flags = cpa_edit.flags = 0;
    cpa_tie_from=0;
    cpa.slot = 0; cpa.t = cpa.elapsed = cpa.acc = 0;
    cpa.on = cpa_commanded = 1; cpa.paused = 0; cge_start();
    if(cgs_active()){
        cgs.pitch=cgs.setup_pitch;
        cgs.playing_genre=cgs.queued_genre;
        cgs.playing_feel=cgs.queued_feel;
        cgs_start();
        cgs_sound_boundary(CGS_PATCH_STAGE);
    }
    cpa_commit();
    cpa.clock_wait=cpa.clock_external=cfp_cfg.clock_in; cpa.clock_lost=0; cpa.clock_ms=cf_now_ms;
    if(capture){
        sq_capture.tick=sq_capture.full=0;
        sq_capture.state=SQ_PERF_ARMED;
    }
    if(cpa.clock_wait) { cfm.running=0; cf_grid.on=0; return; }
    if(capture)cpa_capture_begin();
    cpa_grid_start();cge_start();
    cpa_groove(1); cpa_emit(); if (dmp.on && cge_drums_enabled()) dmp_now();
}
static void cpa_tick1(void)
{
    if (!cpa.on || cpa.paused) return;
    sqp_drum_offs(); cpa.q2 += cfm.beats == 6u ? 1u : 2u;
    cpa.t++; cpa.elapsed++;
    int bar=cpa.t>=cpa.plan.beats*SQ_PPQN;
    int onset=cpa.elapsed>=cpa.plan.slot[cpa.slot].ticks;
    if(sq_capture.state==SQ_PERF_ACTIVE){
        if(sq_capture.tick+1u>=sq_perf_draft.meta.beats*SQ_PPQN*SQ_BARS ||
           (bar && cpa_setup.flags && (cpa_setup.plan.beats!=cpa.plan.beats ||
            (cgs_active() && cgs.queued_bpm && !cfp_cfg.clock_in && cgs.queued_bpm!=cfm.bpm)))){
            sq_perf_finish();
        } else sq_capture.tick++;
    }
    if (bar || onset) {
        cpa_tie_from=onset?cpa.plan.slot[cpa.slot].tie:0;
        int reset = 0, revoice=0;
        if(bar){cpa.t=0;if(cgs_active())cgs.bar++;}
        if (bar && cpa_setup.flags) {
            if (cpa_valid(&cpa_setup.plan)) {
                cpa.plan = cpa_setup.plan; cpa.slot = 0;
                if(cgs_active())cgs_commit_take();
                cpa.elapsed = cpa.t = 0; onset = reset = 1;cpa_tie_from=0;
                cpa_edit.flags = 0; cpa_commit();
            }
            cpa_setup.flags = 0;
        } else {
            if((cpa_edit.flags&CPA_PHRASE) && onset) {
                unsigned next=(cpa.slot+1u)%cpa_edit.plan.n;
                uint8_t r=cpa.plan.rhythm,mode=cpa.plan.mode,kit=cpa.plan.kit,swing=cpa.plan.swing,repeat=cpa.plan.repeat_off;
                cpa.plan=cpa_edit.plan;cpa.slot=(uint8_t)next;cpa.elapsed=0;
                cpa_edit.flags&=CPA_GROOVE;
                if(!bar && cpa_edit.flags){cpa.plan.rhythm=r;cpa.plan.mode=mode;cpa.plan.kit=kit;cpa.plan.swing=swing;cpa.plan.repeat_off=repeat;cpa.plan.click_start=0;}
                cpa_commit();onset=2;
            }
            if ((cpa_edit.flags & CPA_NOTES) && onset) {
                const cpa_plan_t *p = &cpa_edit.plan;
                for (unsigned i = 0; i < cpa.plan.n; i++) cpa.plan.slot[i] = p->slot[i];
                cpa.plan.tonic = p->tonic; cpa.plan.scale = p->scale; cpa.plan.nb = p->nb;
                if(cgs_active())cgs.pitch=cgs.edit_pitch;
                cpa_edit.flags &= ~CPA_NOTES; cpa_commit();
            }
            if (bar && (cpa_edit.flags & CPA_GROOVE)) {
                cpa.plan.rhythm = cpa_edit.plan.rhythm;
                cpa.plan.mode = cpa_edit.plan.mode;
                cpa.plan.kit = cpa_edit.plan.kit; cpa.plan.swing = cpa_edit.plan.swing;
                cpa.plan.repeat_off = cpa_edit.plan.repeat_off;
                if(cpa_edit.plan.click_start){cfm.level=cpa_edit.plan.click_start;cpa.click_generation++;}
                cpa.plan.click_start=cpa_edit.plan.click_start=0;
                cpa_edit.flags &= ~CPA_GROOVE; cpa.revision++;
                if (cpa.plan.mode == CPA_DRUMS) {
                    cpa_arp_off(); sqp_chord_off(); sqp_bass_off(); sqp.dn = 0;
                    if(cgs_active())cgs_release(CGS_LAYER);
                }
            }
            if (onset==1) {
                if(cpa.plan.idea!=CF_P_ENGINE && cpa.plan.repeat_off && cpa.slot+1u>=cpa.plan.n) { cpa_stop(); return; }
                cpa.slot = (uint8_t)((cpa.slot + 1u) % cpa.plan.n); cpa.elapsed = 0;
            }
        }
        if(bar && sq_capture.state==SQ_PERF_ARMED)cpa_capture_begin();
        if(bar || reset) {
            revoice=cge_bar(onset);
            cpa_groove(reset);
            unsigned automatic_fill=cgs_active()?cgs_section_bar():0;
            if(dmp.on)cf_perf_bar(dmp_fill_style(dmp.r),automatic_fill!=0);
        }
        if (onset || revoice) {
            /* The scheduled ARP takes its first gate from the onset grid.
             * Publish that tick before emission, as on the first PLAY. */
            if(cpa.plan.idea==CF_P_ENGINE)cf_grid.g24=24u+cpa.q2/8u;
            cpa_emit();
        }
    }
    const cpa_slot_t *gate=&cpa.plan.slot[cpa.slot];
    if(cgs_active())cgs_tick();
    cge_tick1();
    if(gate->gate<100 && !gate->tie && cpa.elapsed>=(uint32_t)gate->ticks*gate->gate/100u) {
        cpa_arp_off();sqp_chord_off();sqp_bass_off();sqp.dn=0;
    }
    if (dmp.on) { dmp.t = cpa.t; if(cge_drums_enabled())dmp_now(); }
    if (!(cpa.t % SQ_PPQN)) {
        cfm.click = !cpa.t || (cfm.beats == 6u && cpa.t == 3u * SQ_PPQN) ? 2 : 1;
        cfm.beat = (cpa.t / SQ_PPQN + 1u) % cfm.beats;
        cfm.beats_done++; cfm.beat_ms = cf_now_ms;
    }
    cla_tick1();
    cfm.acc = (cpa.t % SQ_PPQN) * (FS * 60u / SQ_PPQN);
    cf_grid.on = 1; cf_grid.g24 = 24u + cpa.q2 / 8u;
}
static void cpa_tick(uint32_t n, uint32_t pulses)
{
    if(!cpa.on)return;
    if(cpa.clock_external!=cfp_cfg.clock_in) {
        sq_perf_finish();
        cpa.clock_external=cfp_cfg.clock_in;cpa.clock_ms=cf_now_ms;cpa.clock_lost=0;
        if(cpa.clock_external) {
            cpa_release();cpa.paused=1;cpa.clock_wait=1;cfm.running=0;cf_grid.on=0;
        } else {
            if(cpa.clock_wait && !cpa.paused){cpa_grid_start();cpa_groove(1);cpa_emit();if(dmp.on && cge_drums_enabled())dmp_now();}
            cpa.clock_wait=0;
        }
    }
    if(cpa.paused)return;
    if (cfp_cfg.clock_in) {
        if(cpa.clock_wait) {
            if(!pulses)return;
            if(sq_capture.state==SQ_PERF_ARMED)cpa_capture_begin();
            cpa.clock_wait=0; cpa_grid_start(); cpa_groove(1); cpa_emit(); if(dmp.on && cge_drums_enabled())dmp_now();
        }
        if(pulses)cpa.clock_ms=cf_now_ms;
        else if(cf_now_ms-cpa.clock_ms>=300u) {
            sq_perf_finish();
            cpa_release();cpa.paused=1;cpa.clock_lost=1;cfm.running=0;cf_grid.on=0;return;
        }
        unsigned q2 = pulses * 8u;
        while (q2 && cpa.on && !cpa.paused) {
            q2 -= cfm.beats == 6u ? 1u : 2u;
            cpa_tick1();
        }
    } else {
        /* Quarter-note units do not change at a queued metre boundary. This
         * also handles a boundary in the middle of an audio block/pulse batch. */
        cpa.acc += n * cfm.bpm * SQ_PPQN * 2u;
        while (cpa.on && !cpa.paused) {
            unsigned limit = FS * 60u * (cfm.beats == 6u ? 1u : 2u);
            if (cpa.acc < limit) break;
            cpa.acc -= limit; cpa_tick1();
        }
    }
}
static void cpa_realtime(unsigned status)
{
    if (!cpa.on) return;
    if(status==0xFCu || (status==0xFAu && !cpa.clock_wait))sq_perf_finish();
    if(status==0xFAu || status==0xFBu || status==0xFCu)cpa.clock_external=cfp_cfg.clock_in;
    if (status == 0xFCu) {
        if(cgs_active()){cgs.take_revision++;cgs_cancel_auto((1u<<CGS_NPART)-1u);}
        cpa.clock_wait=cpa.clock_lost=0; cpa_release(); cpa.paused = 1; cfm.running = 0; cf_grid.on = 0;
    }
    if (status == 0xFAu) {
        cpa.clock_wait=cpa.clock_lost=0;cpa.clock_ms=cf_now_ms;
        cpa_release(); cpa.slot = 0; cpa.t = cpa.elapsed = cpa.acc = 0;
        cpa.paused = 0; cpa_grid_start();cge_start();
        if(cgs_active())cgs_start();
        if(sq_capture.state==SQ_PERF_ARMED)cpa_capture_begin();
        cpa_groove(1); cpa_emit(); if (dmp.on && cge_drums_enabled()) dmp_now();
    }
    if (status == 0xFBu && (cpa.paused || cpa.clock_wait)) {
        int fresh=cpa.clock_wait && !cpa.paused;
        cpa.clock_wait=cpa.clock_lost=0;cpa.clock_ms=cf_now_ms;
        cpa.paused = 0;
        if(fresh){
            if(sq_capture.state==SQ_PERF_ARMED)cpa_capture_begin();
            cpa_grid_start();cpa_groove(1);if(dmp.on && cge_drums_enabled())dmp_now();
        }
        cfm.running = 1; cf_grid.on = 1;
        const cpa_slot_t *s=&cpa.plan.slot[cpa.slot];
        if(s->tie || cpa.elapsed<(uint32_t)s->ticks*s->gate/100u)cpa_emit();
    }
}

enum { CGS_PLAY_PAUSE=2, CGS_PLAY_RESTART };
static void cgs_live_transport(unsigned action)
{
    if(!cgs_active() || cfp_cfg.clock_in)return;
    if(action==CGS_PLAY_RESTART){
        cf_perf_release_drums();
        cpa_realtime(0xFAu);
    } else if(cpa.paused)cpa_realtime(0xFBu);
    else {
        sq_perf_finish();
        cgs.take_revision++;
        cgs_cancel_auto((1u<<CGS_NPART)-1u);
        cpa_release_notes();
        cf_perf_release_drums();
        cpa.paused=1;
        cfm.running=0;
        cf_grid.on=0;
    }
}

#include "cf_engine_audio.c"
