/* SPDX-License-Identifier: GPL-3.0-only
 * Foreground preparation and UI requests. Mailbox copies are bounded and IRQ
 * protected; audio never reads a half-built chord or calls the chord model here.
 */
static const char *const CPA_MODE_NAME[CPA_NMODES] = {"Chords + Drums", "Drums only"};
static const char *cpa_rhythm_name(unsigned r)
{ return r == CPA_OFF ? "Off" : r == CPA_CLICK ? "Click" : DM_RHY[r % DM_N].name; }
static unsigned cpa_compatible(unsigned r, unsigned beats)
{
    if (r == CPA_OFF || r == CPA_CLICK || (r < DM_N && DM_RHY[r].beats == beats)) return r;
    r = dm_nth(beats, 0); return r < DM_N ? r : CPA_CLICK;
}
static void cpa_explore(void)
{
    if (cpg.explored) return;
    cpg.saved_tonic = cs.chord.tonic; cpg.saved_scale = cs.chord.scale;
    cpg.saved_quantize = cs.chord.quantize; cpg.saved_beats = cs.beats;
    cpg.explored = 1;
}
static int cfd_prepare(cpa_plan_t *,cf_chord_cfg_t,unsigned,unsigned,unsigned,unsigned);
static int cgs_rebuild_harmony(cpa_plan_t *, cf_chord_cfg_t);
static int cpa_prepare_full(cpa_plan_t *plan, unsigned idea, cf_chord_cfg_t cfg, unsigned rhythm, unsigned mode, unsigned repeat_off, unsigned click_start)
{
    if(CF_STUDIO && ((idea>=CF_P_COMPOSED && idea<CF_P_ENGINE) ||
        (idea==CF_P_DRAFT && cgs_active() && cpa.plan.idea==CF_P_DRAFT)))return cgs_rebuild_harmony(plan,cfg);
    if(idea==CF_P_ENGINE)return cge_prepare(plan,cfg,rhythm,mode);
    if(idea==CF_P_DRAFT)return cfd_prepare(plan,cfg,rhythm,mode,repeat_off,click_start);
    const cf_progression_t *p = cf_progression_get(idea);
    uint32_t stamp[CF_H_SLOTS]; uint16_t slots;
    if (!p || !cf_harmony_supported(cfg.scale, CF_Q_BASIC)) return 0;
    memset(plan, 0, sizeof *plan);
    plan->idea = idea; plan->n = p->n; plan->tonic = cfg.tonic; plan->scale = cfg.scale;
    plan->beats = p->beats; plan->mode = mode; plan->repeat_off=repeat_off; plan->click_start=click_start;
    plan->rhythm = cpa_compatible(rhythm, p->beats); cfg.quantize = CF_Q_BASIC;
    if(plan->rhythm < DM_N) { plan->kit = dm_kit(plan->rhythm); plan->swing = dm_swing(plan->rhythm); }
    fm1_irq_off();
    slots = ch.mode == CF_H_AUTO && cfc.harmony.mode == ch.mode ? cfc_effective(&cfc) : 0;
    for (unsigned j = 0; j < CF_H_SLOTS; j++) stamp[j] = cfc.stamp[j];
    fm1_irq_on();
    for (unsigned i = 0; i < p->n; i++) {
        cpa_slot_t *s = &plan->slot[i]; cf_chord_out_t o = {0};
        uint16_t used, conflict; int inv = 0, bass = 0; uint32_t newest = 0;
        int key = cf_progression_key(&cfg, p->degree[i]);
        if (key < 0) return 0;
        const cf_source_chord_t *source=cf_progression_source(p);
        s->root = cf_key_root_note(&cfg,key)+(source?source[i].alter:0);
        plan->degree[i] = p->degree[i];
        s->mask = source ? cf_harmony_apply(&ch,source[i].mask,slots,stamp,&used,&conflict)
                        : cf_harmony_mask(&ch,cfg.scale,cfg.tonic,s->root%12,slots,stamp,&used,&conflict);
        for (unsigned j = 0; j < CF_H_SLOTS; j++) if ((used >> j & 1u) && stamp[j] >= newest) {
            newest = stamp[j]; inv = ch.inv[j]; bass = ch.bass[j];
        }
        if(source && source[i].bass>=0 && !inv && !bass) {
            int8_t tones[12]; int nt=cf_mask_tones(s->mask,tones);
            for(int ti=0;ti<nt;ti++) if(tones[ti]%12==source[i].bass) { inv=ti; break; }
        }
        int base = s->root;
        if (inv == CF_INV_AUTO) {
            const cpa_slot_t *prev = i ? &plan->slot[i-1] : 0;
            inv = cf_auto_inversion(base, s->mask, prev ? prev->note : 0, prev ? prev->n : 0);
            int8_t tones[12]; int n = cf_mask_tones(s->mask, tones);
            if (inv >= n) { base -= 12; inv -= n; }
        }
        int8_t tones[12]; int nt = cf_mask_tones(s->mask, tones);
        while (base < 0) base += 12;
        while (nt && base + tones[nt-1] > 127) base -= 12;
        o.n = cf_voice(base, s->mask, inv < 0 ? 0 : inv, o.note);
        for (unsigned j = 0; j < o.n; j++)
            if (!s->n || o.note[j] != s->note[s->n-1]) {
                if (s->n == 6) return 0;
                s->note[s->n++] = o.note[j];
            }
        o.sounding = 1; o.root_note = s->root; o.root_pc = s->root % 12; o.mask = s->mask;
        o.bass_pc = source && source[i].bass>=0 && !bass ? (o.root_pc+source[i].bass)%12 : cf_bass_pc(o.root_pc,s->mask,bass);
        plan->nb = cfp_bass_notes_of(&o, s->bass);
        s->nb=plan->nb;s->gate=100;s->bass_pc=o.bass_pc;
        s->ticks = p->beats * SQ_PPQN; /* explicit durations can be supplied here later */
    }
    return cpa_valid(plan);
}
static int cpa_prepare(cpa_plan_t *p,unsigned idea,cf_chord_cfg_t cfg,unsigned rhythm,unsigned mode)
{ return cpa_prepare_full(p,idea,cfg,rhythm,mode,0,0); }
static void cgs_voice_plan(cpa_plan_t *p,cgs_pitches_t *pitch);
static void cpa_publish(const cpa_plan_t *p, unsigned flags, int setup)
{
    if (!cpa_valid(p)) return;
    cpa_plan_t voiced;
    cgs_pitches_t pitch={0};
    int studio_notes=CF_STUDIO && (flags&CPA_NOTES) && cgs_active();
    if(studio_notes){
        voiced=*p;
        for(unsigned i=0;i<voiced.n && i<cpa.plan.n;i++)voiced.slot[i].ticks=cpa.plan.slot[i].ticks;
        cgs_voice_plan(&voiced,&pitch);
        p=&voiced;
    }
    if(p->idea==CF_P_ENGINE){
        if(flags&CPA_GROOVE)cge_direct(CGE_GROOVE);
        if((flags&CPA_NOTES) && (p->tonic!=cpa.plan.tonic || p->scale!=cpa.plan.scale))cge_key_edit();
    }
    fm1_irq_off();
    if (setup) { cpa_setup.plan = *p; cpa_setup.flags = 1; }
    if(!setup && cgs_active() && (flags&CPA_NOTES) && cpa_setup.flags && cpa_setup.plan.studio &&
        (p->tonic!=cpa.plan.tonic || p->scale!=cpa.plan.scale)){
        cpa_setup.flags=0;
        cgs_cancel_rolled_sounds();
    }
    if (!setup && cpa.on && (p->idea == cpa.plan.idea || (((flags|cpa_edit.flags)&CPA_PHRASE) && p->idea==CF_P_DRAFT))) {
        cpa_edit.plan = *p; cpa_edit.flags |= flags;
        if(studio_notes)cgs.edit_pitch=pitch;
    }
    fm1_irq_on();
}
static void cpa_active_request(cpa_plan_t *p)
{
    fm1_irq_off();
    *p = cpa_edit.flags ? cpa_edit.plan : cpa.plan;
    fm1_irq_on();
}
static void cpa_ui_dirty(void)
{
    if (!cpa_running() || (cgs_active() && cpa.plan.idea==CF_P_DRAFT)) return;
    cpa_plan_t p; cpa_active_request(&p);
    cf_chord_cfg_t cfg = cs.chord; cfg.tonic = p.tonic; cfg.scale = p.scale;
    if (cpa_prepare_full(&p, p.idea, cfg, p.rhythm, p.mode, p.repeat_off, p.click_start)) cpa_publish(&p, CPA_NOTES, 0);
}
static void cpa_ui_poll(void)
{
    cf_click_sync();
    if (cf_sn.ac_revision != cpg.revision) {
        cpg.revision = cf_sn.ac_revision;
        if (cf_sn.ac_on) {
            cpa_explore();
            if((cf_sn.ac_idea<CF_P_COMPOSED || cf_sn.ac_idea>=CF_P_ENGINE) &&
                !(cgs_active() && cf_sn.ac_idea==CF_P_DRAFT))cpg.idea=cf_sn.ac_idea;
            cpg.rhythm = cf_sn.ac_rhythm; cpg.mode = cf_sn.ac_mode;
            cs.chord.tonic = cf_sn.ac_tonic; cs.chord.scale = cf_sn.ac_scale;
            cs.chord.quantize = CF_Q_BASIC; cs.beats = cf_sn.ac_beats;
            sq_beats = cs.beats; cu_spell_update();
            /* Update the shared config without rebuilding or cancelling queued edits. */
            fm1_irq_off(); cfc_shadow = cs.chord; fm1_irq_on();
        }
    }
    uint32_t clock;
    fm1_irq_off(); clock = cfc.clock; fm1_irq_on();
    if (clock != cpg.modifiers) { cpg.modifiers = clock; cpa_ui_dirty(); }
}
static uint8_t cpa_scale_step(unsigned scale,int d)
{
    static const uint8_t modes[]={CF_SC_MAJOR,CF_SC_DORIAN,CF_SC_PHRYGIAN,CF_SC_LYDIAN,
        CF_SC_MIXOLYDIAN,CF_SC_MINOR,CF_SC_LOCRIAN};
    unsigned i=0;
    while(i+1<NELEM(modes) && modes[i]!=scale)i++;
    return modes[wrapi((int)i+d%(int)NELEM(modes),NELEM(modes))];
}
static void cpa_home_key(int k, int d)
{
    if(!cgs_active())cge_key_edit();
    cpa_plan_t p; cpa_active_request(&p);
    cf_chord_cfg_t cfg = cs.chord; cfg.tonic = p.tonic; cfg.scale = p.scale;
    if (k == 1) cfg.tonic = wrapi(cfg.tonic + d, 12);
    else cfg.scale=cpa_scale_step(cfg.scale,d);
    if (cpa_prepare_full(&p, p.idea, cfg, p.rhythm, p.mode, p.repeat_off, p.click_start)) cpa_publish(&p, CPA_NOTES, 0);
}
static void cpa_ui_rhythm(int d)
{
    cge_direct(CGE_GROOVE);
    cpa_plan_t p; unsigned beats = cs.beats, r = cpg.rhythm;
    if (cpa_running()) { cpa_active_request(&p); beats = p.beats; r = p.rhythm; }
    unsigned n = dm_count(beats), pos = r == CPA_OFF ? 0 : r == CPA_CLICK ? 1 : dm_place(r) + 2;
    pos = wrapi((int)pos + d, n + 2);
    r = pos == 0 ? CPA_OFF : pos == 1 ? CPA_CLICK : dm_nth(beats, pos - 2);
    if (cpa_running()) {
        if(r==CPA_CLICK && p.rhythm!=CPA_CLICK && !cs.metro_level)p.click_start=cf_click_level;
        p.rhythm = r;
        if(r<DM_N) { p.kit=dm_kit(r); p.swing=dm_swing(r); }
        cpa_publish(&p, CPA_GROOVE, 0);
    }
    else {
        if(r==CPA_CLICK && cpg.rhythm!=CPA_CLICK && !cs.metro_level){cs.metro_level=cf_click_level;cf_send_metro();}
        cpg.rhythm = r;
    }
}
static void cpa_ui_mode(int d)
{
    if (cpa_running()) {
        cpa_plan_t p; cpa_active_request(&p); p.mode = wrapi(p.mode + d, CPA_NMODES);
        cpa_publish(&p, CPA_GROOVE, 0);
    } else cpg.mode = wrapi(cpg.mode + d, CPA_NMODES);
}
static void cpa_tempo(int d)
{
    if(d)cge.tempo_pin=1;
    if (cs.clock_in == 1u) { cf_message("Tempo follows MIDI clock"); return; }
    cs.bpm = clampi(cs.bpm + d, 30, 300); cf_send_metro();
}
static void cpa_tempo_info(cf_knob_t *o)
{
    kfmt(o->name, 12, "TEMPO");
    kint(o->val, cs.clock_in == 1u ? cf_sn.ext_bpm : cs.bpm, cs.clock_in == 1u ? " EXT" : " BPM");
    o->dis = cs.clock_in == 1u;
}
static int cpa_ui_start(int capture)
{
    cpa_plan_t p;
    cpa_explore();
    const cf_progression_t *idea = cf_progression_current();
    cf_chord_cfg_t cfg = cs.chord;
    if (!cf_harmony_supported(cfg.scale, CF_Q_BASIC)) cfg.scale = idea->scale;
    if (cpa_prepare_full(&p, cpg.idea, cfg, cpg.rhythm, cpg.mode, cpg.repeat_off, 0)) {
        cpa_publish(&p, 0, 1);
        if (cf_post(CFE_ACCOMP, 1, capture, 0)) {cpa_commanded = 1;return 1;}
        else cf_message("Busy: press PLAY again");
    } else cf_message("Cannot prepare progression");
    return 0;
}
static void cpa_ui_play(void)
{
    if(cpa_commanded){
        if(cf_post(CFE_ACCOMP,0,0,0))cpa_commanded=0;
        else cf_message("Busy: press PLAY again");
    } else if(sqk.on || cf_sn.sq_rec!=255)cf_message("Finish recording to play");
    else cpa_ui_start(0);
}
static int cgs_start_recording(void);
static void cpa_ui_record(void)
{
    if(cr_safe || cfs_restore_lock){cf_message("Recording unavailable: restart");return;}
    if(sqp.on || (cpa_commanded!=cpa_running()) || (cpa.on && cpa.paused)){
        cf_message("Stop replay or resume Studio first");
        return;
    }
    int slot=sq_home_target();
    if(slot<0){cf_message("All 3 recordings are used:\nclear one in RECORDINGS");return;}
    if(cf_evq_w-cf_evq_r>CF_EVQ-8u){cf_message("Busy: press REC again");return;}
    memset(&sq_perf_draft,0,sizeof sq_perf_draft);
    int started=cpa.on?cf_post(CFE_CAPTURE,1,0,0):
        cgs_home()?cgs_start_recording():cpa_ui_start(1);
    if(!started)return;
    sqk.on=sqk.performance=1;
    sqk.discard=sqk.seen=sqk.rolled=0;
    sqk.pat=(uint8_t)slot;
    sqk.nbar=sqk.t=sqk.bt=0;
    if(cpa.on && sq_capture.state!=SQ_PERF_ACTIVE)cf_message("Recording on the next bar");
}
