#if !CF_SCENES
static void cf_rakt_begin(void);
#endif
/* SPDX-License-Identifier: GPL-3.0-only
 * CoralFlower: the UI - the keys, buttons and knobs, the screens, the key lights, the plant's food.
 * The main loop's (ui_input / ui_leds / ui_draw through cf_shim.c). Included after cf_draw.c.
 *
 * Every screen has the same grammar (docs/SPEC.md): MASTER is the volume; SELECT, PRESETS and ALGORITHM keep
 * their roles (bass sound, chord sound, scene) unless a screen needs pages; KNOB 1-4 are the screen's.
 * A screen's button opens it; pressed again (or HOME) it goes back to HOME. Pages turn only with an encoder.
 */

static void cf_message(const char *s);
static int cgu_glow_available(void);
#ifndef CF_KEY_T0
#ifdef CU_KEY_T0
#define CF_KEY_T0(k) CU_KEY_T0(k)                /* (the emulator's: tools/emu/emu_hal_fw.h) */
#else
#define CF_KEY_T0(k) fm1_in.note_t0[k]           /* the first scan that saw key k closed (hal/fm1_input.h) */
#endif
#endif
#ifndef CF_BTN_T0
#ifdef CU_BTN_T0
#define CF_BTN_T0(i) CU_BTN_T0(i)
#else
#define CF_BTN_T0(i) fm1_in.btn_t0[i]           /* .. button i (0.6.1: REC's moment, cf_rec_down) */
#endif
#endif
static void cf_pop(uint32_t kind, uint32_t ms);
static int cf_editing_part(void);
static void cf_edited(uint32_t part);
static void cf_glide_migrate(void);
static void ui_snd_heard(uint32_t part);
static void cf_save_open(void);
static void cf_save_name_init(void);
static void snd_num(uint32_t idx, char *b);
static const char CF_NAME_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.+#/";   /* (SAVE: a name's letters) */

/* -------------------------------------------------------- the engine --- */
static uint8_t sq_live_part(void);              /* (cf_sequi.c) */
static uint8_t sq_live_bass(void);              /* (cf_sequi.c) */
static uint8_t sq_keys_arp(void);               /* (cf_sequi.c: the keys' ARP, a track's own while it is open) */
static void cpa_ui_dirty(void);
static int cgu_play_part(void);
static int cgu_keys_arp(void);
static void cf_key_config(void)
{
    int part=cgu_play_part();
    cfp_cfg.studio_keys=part>=0;
    cfp_cfg.cpart=part<0?sq_live_part():part==CF_PART_BASS?CF_PART_CHORD:(uint8_t)part;
    cfp_cfg.bass_only=part<0?sq_live_bass():part==CF_PART_BASS;
    cfp_cfg.arp_on=part<0?sq_keys_arp():cgu_keys_arp();
}
static void cf_keys_changed(void)
{
    fm1_irq_off();
    cf_key_config();
    fm1_irq_on();
    cf_post(CFE_CONFIG,0,0,0);
}
static void cf_send_config(void)                 /* the chord settings -> the audio side */
{
    cs.chord.algo = CF_ALG_TOP_LOCK;
    fm1_irq_off();
    cfc_shadow = cs.chord;
    cfh_shadow = ch;
    const cf_progression_t *source_p = cf_progression_get(cpa.on ? cpa.plan.idea : cpg.idea);
    cfsrc_shadow = cpg.explored && source_p && (cpa.on?cpa.plan.idea:cpg.idea)!=CF_P_DRAFT ? cf_progression_source(source_p) : 0;
    cfsrc_n = source_p && cfsrc_shadow ? source_p->n : 0;
    if((cpa.on?cpa.plan.idea:cpg.idea)==CF_P_DRAFT){cfsrc_shadow=cfc.source;cfsrc_n=cfc.source_n;}
    cfp_cfg.glide_always = cs.glide_always;
    cfp_cfg.vel = cs.vel;
    cfp_cfg.bass_on = cs.bass_on;
    cfp_cfg.bass_oct = cs.bass_oct;
    cfp_cfg.octaver = cs.octaver;
    cfp_cfg.midi_out = cs.midi_out;
    cfp_cfg.ch_chord = cs.ch_chord;
    cfp_cfg.ch_bass = cs.ch_bass;
    cfp_cfg.ch_layer = cs.ch_layer;
    cfp_cfg.ch_drums = cs.ch_drums;
    cfp_cfg.retrig = cs.retrig;
    cfp_cfg.notes_in = cs.notes_in;
    cfp_cfg.in_ch = (uint8_t)(cs.in_ch > CF_INCH_ALL ? cs.in_ch - CF_INCH_ALL : 0u);
    cfp_cfg.arp = cs.arp;
    cfp_cfg.bpm = cs.bpm;
    cfp_cfg.clock_in = cs.clock_in == 1u;
    cfp_cfg.clock_out = cs.clock_in == 2u;
    trk[CF_PART_BASS].p[P_LEVEL] = cs.bass_level;
    trk[CF_PART_CHORD].p[P_LEVEL] = cs.level_chord;
    trk[CF_PART_LAYER].p[P_LEVEL] = cs.level_layer;
    trk[CF_PART_DRUMS].p[P_LEVEL] = cs.level_drums;
    cf_key_config();
    fm1_irq_on();
    cf_fx_apply(&cs.fx);
    cf_post(CFE_CONFIG, 0, 0, 0);
    cpa_ui_dirty();
}
static void cf_click_sync(void)
{
    if(cpg.click_generation==cpa.click_generation)return;
    fm1_irq_off();cs.metro_level=cfm.level;cpg.click_generation=cpa.click_generation;fm1_irq_on();
}
static void cf_click_cancel_pending(void);
static void cf_send_metro(void)
{
    cf_click_sync();
    if(cs.metro_level && cs.metro_level!=cf_click_level) {
        cf_click_level=cs.metro_level;cpb_dirty=1;cpb_changed=cu.now;
    }
    if (!cpa_running()) sq_beats = (uint8_t)(cs.beats >= 2u && cs.beats <= 7u ? cs.beats : 4u);   /* (the sequencer's bar: cf_seqd.c) */
    cf_post(CFE_TEMPO, 0, 0, cs.bpm);
    cf_post(CFE_SIG, 0, 0, cs.beats);
    cf_post(CFE_METRO, 0x80u, cs.metro_level, (int16_t)cpg.click_generation);   /* (the click only while the beat runs, PLAY: the owner, 2026-10-19;
                                                  * it sounded all the time the METRONOME or the SEQUENCER was open) */
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int wrapi(int v, int n) { return ((v % n) + n) % n; }

/* the arpeggiator's names: a knob cell's (7 characters at most), the screen's title */
static const char *const CF_ARP_SHORT[CF_ARP_NMODES] = {"Up", "Down", "Incl", "Excl", "Random", "Order", "Repeat",
                                                        "StrumU", "StrumD", "Convrg", "Diverge", "LowPed", "HighPed"};
static const char *const CF_ARP_NAME[CF_ARP_NMODES] = {"Up", "Down", "Inclusive", "Exclusive", "Random", "Order",
                                                       "Repeat", "Strum up", "Strum down", "Converge", "Diverge", "Low pedal", "High pedal"};
static const char *const CF_ARP_RATE[CF_ARP_NRATES] = {"1/1", "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T",
                                                       "1/32"};

/* ------------------------------------------------------------ spelling --- */
static cf_spell_t cu_spell;
static void cu_spell_update(void)
{
    if (cs.chord.quantize == CF_Q_OFF)
        cf_key_spell(0, CF_SC_MAJOR, &cu_spell);
    else
        cf_key_spell(cs.chord.tonic, cs.chord.scale, &cu_spell);
}

/* GLOBAL > Reset's steps (cf_reset_poll below) */
static void cf_factory_erase(void);
static struct {
    uint8_t arm, fin, go;        /* K4 at Reset; the hold done (the last question); erasing */
    uint32_t hold, fin_ms, go_ms;   /* SAVE went down; the question came; the erase was asked */
} cfr;
#define CF_RESET_HOLD_MS 3000u
#define CF_RESET_ASK_MS 6000u

/* --------------------------------------------------------------- knobs --- */
static const char *const Q_NAME[3] = {"Off", "Basic", "Full"};
static const char *const ALG_NAME[CF_NALGS] = {"Top", "Top Lock", "Side", "Side Lock"};
#define CF_GLO_HARMONY 7u
#define CF_GLO_VARIATION 8u
#define CF_GLO_ACCOMP 9u
#define CF_GLO_BROWSE 10u
#define CF_GLO_RECORDINGS 11u
#define CF_GLO_LEGACY 12u
#define CF_GLO_ENGINE_LOCKS 13u
#define CF_GLO_NPAGES 14u
static int cge_prepare(cpa_plan_t *,cf_chord_cfg_t,unsigned,unsigned);
static int cge_snapshot(cf_song_t *);
static void cge_direct(unsigned),cge_sound_poll(void),cge_poll(void),cge_init(void);
static int cge_load(void);
static void cge_key_edit(void);

static const char *const CF_LEGACY_NAME[]={"Recordings","Metronome","Drum grid"};

static const cf_progression_t *cf_progression_current(void)
{
    if(cpg.idea==CF_P_DRAFT)cfd_view_update();
    cpg.idea = cf_progression_pick(cpg.idea, 0);
    return cf_progression_get(cpg.idea);
}
static void cf_progression_select(int d);
static void cf_progression_choose(unsigned next);
static void cfd_knob_info(int,cf_knob_t *);

static void kfmt(char *b, uint32_t n, const char *s) { str_cpy(b, s, n); }
static void kint(char *b, int v, const char *unit)
{
    char t[12];
    int k = 0, neg = v < 0, i;
    unsigned u = (unsigned)(neg ? -v : v);
    do {
        t[k++] = (char)('0' + u % 10u);
        u /= 10u;
    } while (u && k < 10);
    i = 0;
    if (neg)
        b[i++] = '-';
    while (k)
        b[i++] = t[--k];
    while (unit && *unit && i < 13)
        b[i++] = *unit++;
    b[i] = 0;
}
/* HOME view cycling contributed by Michael Norris (@norrismw), CoralFlower issue #8.
 * HOME's view (ChoralRoot's views, HOME tapped on HOME steps to the next): the scene; the chord, its name big and
 * the notes it voices under it; a keyboard with the chord's and the bass's notes lit. Kept in cs.seq_hint's top two
 * bits (those hints test bits 0..2 only; every older record has 0 there: the scene).
 * HV_FLOWER retains its legacy numeric identity for saved records; it now means the selected scene. */
enum { HV_FLOWER, HV_CHORD, HV_KEYS, HV_DIAG, HV_N };
#define HV_SHIFT 6u
static uint8_t home_boot_main; /* session-only: bypass the saved alternate HOME view */
static uint32_t home_view(void)
{
    if (home_boot_main) return HV_FLOWER;
    uint32_t v = (uint32_t)cs.seq_hint >> HV_SHIFT & 3u;
    return v < HV_N ? v : HV_FLOWER;
}
static int home_stage(void)
{ return cu.scr == SCR_HOME && home_view() == HV_FLOWER && cf_scene() == CF_SCENE_PROGRESSIONS; }
static const char *const HOME_VIEW_NAME[] = {"Stage", "Chord", "Keyboard", "Diagnostics"};
static void home_view_select(int d)
{
    uint32_t v = (uint32_t)wrapi((int)home_view() + d, HV_N);
    home_boot_main = 0;
    cs.seq_hint = (uint8_t)((cs.seq_hint & 63u) | v << HV_SHIFT);
}
static void home_view_next(void)
{
    static const char *const NAME[HV_N] = {"View: Scene", "View: Chord", "View: Keyboard", "View: Diagnostics"};
    uint32_t v = (home_view() + 1u) % HV_N;
    home_boot_main = 0;
    cs.seq_hint = (uint8_t)((cs.seq_hint & ((1u << HV_SHIFT) - 1u)) | v << HV_SHIFT);
    cf_message(v == HV_FLOWER ? CF_SCENE_NAME[cf_scene()] : NAME[v]);
}

static void kpc(char *b, int pc)
{
    char n[4];
    cf_pc_name(pc, &cu_spell, n);
    fb_signs(n, b, 8);
}

#include "cf_edit.c"                             /* (the ENV and EDIT screens' engine side) */
#include "cf_fx_presets_ui.c"
#include "cf_fxui.c"                             /* (the FX screen's controls) */
static void cv_turned(int d);
static void cpa_ui_record(void);
#include "cf_sequi.c"                            /* (the SEQUENCER screens' controls) */
#include "cf_dmui.c"                             /* (the drum machine's page, 0.6) */
#include "cf_accomp_ui.c"
#include "cf_browser_ui.c"
#include "cf_draft_ui.c"
#include "cf_lab_ui.c"
#include "cf_library_ui.c"
#include "cf_engine_ui.c"
#include "cf_studio_ui.c"
#include "cf_workspace.c"
#include "cf_variation.c"                         /* one-page variation and undo */

/* SELECT on Progressions: apply the idea's setup, without touching sounds,
 * modifiers, saved patterns or transport. Drum meter changes use its bar queue. */
static void cf_progression_choose(unsigned next)
{
    next=cf_progression_pick(next,0);
    if(cge_active() && next<cf_progression_count()){
        if(!cge_choose_progression(next) && cge_notice)cf_message(cge_notice);
        return;
    }
    if (sqk.on || cf_sn.sq_rec != 0xFFu) {
        cf_message("Finish recording to browse");
        return;
    }
    cpa_explore();
    if (cpa_running()) {
        cpa_plan_t plan;
        const cf_progression_t *p = cf_progression_get(next);
        cf_chord_cfg_t cfg = cs.chord; cfg.tonic = p->tonic; cfg.scale = p->scale;
        if (cpa_prepare_full(&plan, next, cfg, cpa_compatible(cpg.rhythm, p->beats), cpg.mode, cpg.repeat_off, 0))
            cpa_publish(&plan, 0, 1);
        return;
    }
    cpg.idea = cf_progression_pick(next, 0);
    const cf_progression_t *p = cf_progression_current();
    cpg.rhythm = cpa_compatible(cpg.rhythm, p->beats);
    cs.chord.tonic = p->tonic;
    cs.chord.scale = p->scale;
    if (cs.chord.quantize == CF_Q_OFF) cs.chord.quantize = CF_Q_BASIC;
    if (cs.beats != p->beats || (cf_sn.dm_on && dm_view < DM_N && DM_RHY[dm_view].beats != p->beats)) {
        uint32_t r = dm_nth(p->beats, 0);
        for (uint32_t i = 0; i < DM_N; i++)
            if (DM_RHY[i].beats == p->beats && DM_RHY[i].id == dmr.pos[p->beats]) r = i;
        if (cf_sn.dm_on && r < DM_N) dm_select((int)r - (int)dm_cur());
        else { cs.beats = p->beats; dm_view = 0xFF; cf_send_metro(); }
    }
    cu_spell_update();
    cf_send_config();
}

/* K1 changes the groove within the active/pending meter, so a groove choice
 * cannot accidentally change the progression's time signature. */
static void cf_progression_rhythm(int d)
{
    if (!d || sqk.on) return;
    cpa_ui_rhythm(d);
}

/* EDIT's part keys, as printed on the unit: MONO (upper key 8) the bass, POLY (9) the chords, the next (10) the LAYER */
static uint32_t edit_key_part(int u) { return u == 8 ? CF_PART_BASS : u == 9 ? CF_PART_CHORD : CF_PART_LAYER; }
static uint32_t env_part(void)
{
    return cu.env_tgt == 2u ? CF_PART_BASS : cu.env_tgt == 3u ? CF_PART_LAYER : CF_PART_CHORD;
}
/* SAVE's pages: the chord's sound, the bass's, the LAYER's (the sequencer's third sound: the owner, 2026-10-19) */
#define CF_SAVE_NPG 3u
static uint32_t cf_save_part(void) { return cu.page[SCR_SAVE] < CF_SAVE_NPG ? cu.page[SCR_SAVE] : 0u; }

/* the first chord button SEL can edit: the first that is not Off (Side has no Off) */
static uint8_t sel_first(void)
{
    uint8_t b = 0;
    while (b + 1u < CF_TOP_NBTN && cf_is_off(&cs.chord, b))
        b++;
    return b;
}

/* the chord button SEL edits (the callers leave Off alone: cf_is_off) */
static cf_form_t *sel_form(void)
{
    int top = cs.chord.algo == CF_ALG_TOP || cs.chord.algo == CF_ALG_TOP_LOCK;
    if (top)
        return &cs.chord.top[cu.sel_btn < CF_TOP_NBTN ? cu.sel_btn : sel_first()];
    return &cs.chord.side[cu.sel_btn < CF_SIDE_NBTN ? cu.sel_btn : 0];
}

static int cf_home_bank_info(int k, cf_knob_t *o);
static int cf_home_bank_turn(int k, int d);
static void cf_acc_knob_info(int k,cf_knob_t *o);
static void cf_arp_controls_info(int k,cf_knob_t *o,int timing,int studio)
{
    int strum=CF_ARP_STRUMMING(cs.arp.mode);
    if(timing){
        switch(k){
        case 0: kfmt(o->name,12,"GATE"); kint(o->val,cs.arp.gate,"%"); o->dis=(uint8_t)strum; o->bar=cs.arp.gate*255/100; break;
        case 1: kfmt(o->name,12,"SWING"); kint(o->val,cs.arp.swing,"%"); o->dis=(uint8_t)strum; o->bar=(cs.arp.swing-50)*255/25; break;
        case 2: kfmt(o->name,12,"LATCH"); kfmt(o->val,14,cs.arp.latch?"On":"Off"); o->bar=cs.arp.latch?255:0; break;
        default: kfmt(o->name,12,"GLIDE"); kint(o->val,cfe_glide_ms(trk[CF_PART_CHORD].p[P_GLIDE]),"ms");
            o->bar=trk[CF_PART_CHORD].p[P_GLIDE]*255/127;
        }
        return;
    }
    switch(k){
    case 0:
        if(studio){
            kfmt(o->name,12,"ARP FOR");
            kfmt(o->val,14,CGU_ARP_NAMES[cgu_arp_mode()]);
            o->bar=cgu_arp_mode()*255/CGU_ARP_BAND;
            o->dis=!cgu_arp_available();
        } else {
            kfmt(o->name,12,cge_selected()?(cu.page[SCR_ARP]==0?"PLAY IT":"LIVE KEYS"):"ON/OFF");
            kfmt(o->val,14,cge_selected() && cu.page[SCR_ARP]==0?CGE_ARP_NAMES[cge.arp_request]:cu.arp_on?"On":"Off");
        }
        break;
    case 1: kfmt(o->name,12,"MODE"); kfmt(o->val,14,CF_ARP_SHORT[cs.arp.mode % CF_ARP_NMODES]);
        o->bar=cs.arp.mode*255/(CF_ARP_NMODES-1); break;
    case 2: kfmt(o->name,12,"OCTAVE"); kint(o->val,cs.arp.oct,0); o->bar=(cs.arp.oct-1)*255/3; break;
    default:
        kfmt(o->name,12,"RATE");
        if(strum){
            kint(o->val,cs.arp.strum_ms," ms"); o->bar=(cs.arp.strum_ms-5)*255/245;
        } else {
            kfmt(o->val,14,CF_ARP_RATE[cs.arp.rate % CF_ARP_NRATES]); o->bar=cs.arp.rate*255/(CF_ARP_NRATES-1);
        }
    }
}

static void ui_knob_info(int k, cf_knob_t *o)
{
    o->bar = -1;
    switch (cu.scr) {
    case SCR_LAB: cfl_knob_info(k,o);break;
    case SCR_HOME:
        if (home_stage() && cf_home_bank_info(k, o)) break;
        if (k == 0 && (cf_scene() == CF_SCENE_PROGRESSIONS || cpa_running())) {
            kfmt(o->name, 12, "RHYTHM");
            kfmt(o->val, 14, cpa_rhythm_name(cpg.rhythm));
            o->dis = sqk.on;
            break;
        }
        switch (k) {
        case 0: kfmt(o->name, 12, "QUANTIZE"); kfmt(o->val, 14, Q_NAME[cs.chord.quantize]); break;
        case 1:
            kfmt(o->name, 12, "KEY");
            kpc(o->val, cs.chord.tonic);
            o->dis = cs.chord.quantize == CF_Q_OFF;
            break;
        case 2:
            kfmt(o->name, 12, "SCALE");
            kfmt(o->val, 14, CF_SCALES[cs.chord.scale].shrt);
            o->dis = cs.chord.quantize == CF_Q_OFF;
            break;
        default: cpa_tempo_info(o); break;
        }
        break;
    case SCR_SEL: {
        cf_form_t *f = sel_form();
        if (cf_is_off(&cs.chord, cu.sel_btn)) {
            if (k == 0) {
                kfmt(o->name, 12, "BUTTON");
                kfmt(o->val, 14, "Hold");
            }
            return;
        }
        if (ch.mode == CF_H_AUTO) {
            unsigned b = cu.sel_btn;
            if (k == 0) { kfmt(o->name, 12, "MODIFIER"); kfmt(o->val, 14, cf_modifier_name(ch.slot[b])); }
            if (k == 2) { kfmt(o->name, 12, "INV"); kfmt(o->val, 14, ch.inv[b] < 0 ? "Smooth" : ch.inv[b] == 0 ? "Root" : ch.inv[b] == 1 ? "1st" : ch.inv[b] == 2 ? "2nd" : "3rd"); }
            if (k == 3) { kfmt(o->name, 12, "BASS"); kint(o->val, ch.bass[b] + 1, " tone"); }
            break;
        }
        switch (k) {
        case 0: kfmt(o->name, 12, "TYPE"); kfmt(o->val, 14, cf_type_name(f->type)); break;
        case 1: kfmt(o->name, 12, "ADD"); kfmt(o->val, 14, cf_adds_name(cf_adds_index(f->add))); break;
        case 2:
            kfmt(o->name, 12, "INV");             /* (a knob cell holds 8 characters) */
            kfmt(o->val, 14, f->inv == CF_INV_AUTO ? "Auto" : f->inv == 0 ? "Root" : f->inv == 1 ? "1st" : f->inv == 2 ? "2nd" : "3rd");
            break;
        default: {
            static const char *const DEG[] = {"Root", "2nd tone", "3rd tone", "4th tone", "5th tone", "6th tone"};
            kfmt(o->name, 12, "BASS");
            kfmt(o->val, 14, DEG[f->bass < 6 ? f->bass : 0]);
            break;
        }
        }
        break;
    }
    case SCR_ENV: {
        static const char *const N[4] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
        int v = cf_env_get(env_part(), k);
        kfmt(o->name, 12, N[k]);
        cf_env_fmt(o->val, k, cf_env_val(env_part(), k));
        o->bar = (int16_t)(v * 255 / 99);
        break;
    }
    case SCR_EDIT:
        if (!cfe_info(cu.edit_part, cu.edit_sec, cu.page[SCR_EDIT], (uint32_t)k, o->name, o->val, &o->bar))
            o->name[0] = 0;
        break;
    case SCR_FX:
        cfx_knob_info(k, o);
        break;
    case SCR_BASS:
        switch (k) {
        case 0: kfmt(o->name, 12, "BASS"); kfmt(o->val, 14, cs.bass_on ? "On" : "Off"); break;
        case 1:
            kfmt(o->name, 12, "LEVEL");
            kint(o->val, cs.bass_level * 100 / 127, "%");
            o->bar = (int16_t)(cs.bass_level * 255 / 127);
            break;
        case 2:
            kfmt(o->name, 12, "OCTAVE");
            kint(o->val, cs.bass_oct, 0);
            if (cs.bass_oct > 0)
                o->val[0] = '+', kint(o->val + 1, cs.bass_oct, 0);
            break;
        default: kfmt(o->name, 12, "OCTAVER"); kfmt(o->val, 14, cs.octaver ? "On" : "Off"); break;
        }
        break;
    case SCR_SAVE: {
        uint32_t part = cf_save_part();
        if (k == 0) {
            kfmt(o->name, 12, "SAVE AS");
            kfmt(o->val, 14, cu.save_new ? "New" : "Replace");
            o->dis = cf_snd[part].kind == PK_INIT;     /* (INIT: only as new) */
        } else if (k == 1) {
            kfmt(o->name, 12, "LETTER");
            kint(o->val, cu.save_cur + 1, 0);
        } else if (k == 2) {
            char ch = cu.save_name[cu.save_cur];
            kfmt(o->name, 12, "CHAR");
            if (!ch || ch == ' ')
                kfmt(o->val, 14, "space");
            else
                o->val[0] = ch, o->val[1] = 0;
        } else {                                 /* ACTION: Save, or Delete (your sounds only) */
            kfmt(o->name, 12, "ACTION");
            kfmt(o->val, 14, cu.save_del ? "Delete" : "Save");
            o->dis = cf_snd[part].kind != PK_USER;
        }
        if (cu.save_del && k < 3)                /* (deleting: the name and the way do not matter) */
            o->dis = 1;
        break;
    }
    case SCR_ARP: cf_arp_controls_info(k,o,cu.page[SCR_ARP]==1,0); break;
    case SCR_PEDIT: cfd_knob_info(k,o); break;
    case SCR_ACC: cf_acc_knob_info(k,o); break;
    case SCR_SEQ: metro_knob_info(k, o); break;
    case SCR_DRUM: dm_knob_info(k, o); break;
    case SCR_SQ:
    case SCR_SQED: sq_knob_info(k, o); break;
    case SCR_GLO:
        switch (cu.page[SCR_GLO]) {
        case CF_GLO_ENGINE_LOCKS: {
            static const char *const names[]={"HARMONY","SOUND","GROOVE","FX"};
            kfmt(o->name,12,names[k]);kfmt(o->val,14,cge.locks&(1u<<k)?"Locked":"Open");break;
        }
        case CF_GLO_RECORDINGS:
            if (!k) { kfmt(o->name,12,"AUTO DEL"); kfmt(o->val,14,sq_pref.protect?"Off":"On"); }
            break;
        case CF_GLO_LEGACY: if(k==0){kfmt(o->name,12,"OPEN");kfmt(o->val,14,CF_LEGACY_NAME[cf_legacy_pick]);} break;
        case CF_GLO_BROWSE: cpb_knob_info(k,o); break;
        case CF_GLO_ACCOMP:
            if (k == 0) { kfmt(o->name, 12, "ACCOMPANY"); kfmt(o->val, 14, CPA_MODE_NAME[cpg.mode]); }
            if (k == 1) { kfmt(o->name, 12, "RHYTHM"); kfmt(o->val, 14, cpa_rhythm_name(cpg.rhythm)); }
            if (k == 2) cpa_tempo_info(o);
            if (k == 3) { kfmt(o->name, 12, "GLIDE"); kint(o->val, cfe_glide_ms(trk[0].p[P_GLIDE]), "ms"); }
            break;
        case CF_GLO_VARIATION:
            cvr_knob_info(k, o);
            break;
        case CF_GLO_HARMONY:
            if (k == 0) { kfmt(o->name, 12, "HARMONY"); kfmt(o->val, 14, ch.mode ? "On" : "Off"); }
            if (k == 1) { kfmt(o->name, 12, "RESET"); kfmt(o->val, 14, "Defaults"); }
            break;
        case 6:
            if(k==0) {kfmt(o->name,12,"PAGE");kfmt(o->val,14,CF_DIAG_PAGES[cfd.page]);}
            if(k==1) {kfmt(o->name,12,"DISPLAY");kfmt(o->val,14,cfd.hold?"Hold":"Live");}
            if(k==2) {kfmt(o->name,12,"SESSION");kfmt(o->val,14,"Turn: reset");}
            if(k==3) {kfmt(o->name,12,"REFRESH");kint(o->val,CF_DIAG_HZ[cfd.rate]," Hz");}
            break;
        case 5: /* Library: filter only; the loaded sound and its edits stay put. */
            if (k < 3u) {
                static const char *const PART[3] = {"CHORDS", "BASS", "LAYER"};
                kfmt(o->name, 12, PART[k]);
                kfmt(o->val, 14, SIG8_BANK_NAMES[cf_bank[k]]);
            } else {
                kfmt(o->name, 12, "RESET");
                kfmt(o->val, 14, "All banks");
            }
            break;
        case 0: /* Display */
            if (k == 0) { kfmt(o->name, 12, CF_STUDIO && !CF_SCENES?"ACCENT":"CHARACTER"); kfmt(o->val, 14, CF_SKIN_NAME[cf_skin%CF_SKIN_N]); }
            if (k == 3) { kfmt(o->name, 12, "MOTION"); kfmt(o->val, 14, cf_motion ? "On" : "Off"); }
            if (k == 1) { kfmt(o->name, 12, "HOME VIEW"); kfmt(o->val, 14, HOME_VIEW_NAME[home_view()]); }
            if (k == 2) { kfmt(o->name, 12, "BOOT SND"); kfmt(o->val, 14, cs.boot_sound ? "On" : "Off"); }
            break;
        case 1: /* MIDI: out on or off, the clock in (the channels: the next page, four of them since the sequencer's
                 * LAYER and DRUMS) */
            if (k == 0) { kfmt(o->name, 12, "MIDI OUT"); kfmt(o->val, 14, cs.midi_out ? "On" : "Off"); }
            if (k == 1) {                        /* MIDI clock: In (its tempo, its start / stop) or Out (the unit's) */
                kfmt(o->name, 12, "CLOCK");
                kfmt(o->val, 14, cs.clock_in == 2u ? "Out" : !cs.clock_in ? "Off" : cf_sn.ext_clock ? "In: lock" : "In");
            }
            if (k == 2) {                        /* an outside keyboard's notes (0.5): the FM-1's keys, roots, Direct */
                static const char *const NI[CF_NI_N] = {"FM-1 keys", "Roots", "Direct"};
                kfmt(o->name, 12, "NOTES IN");
                kfmt(o->val, 14, NI[cs.notes_in % CF_NI_N]);
            }
            if (k == 3) {                        /* the channel FM-1 keys and Roots listen to (0.5.1; Direct: CHORD and
                                                  * BASS CH, the next page) */
                kfmt(o->name, 12, "IN CH");
                if (cs.in_ch > CF_INCH_ALL)
                    kint(o->val, cs.in_ch - CF_INCH_ALL, 0);
                else
                    kfmt(o->val, 14, "All");
                o->dis = (uint8_t)(cs.notes_in == CF_NI_DIRECT);
            }
            break;
        case 2: /* Channels */
            if (k == 0) { kfmt(o->name, 12, "CHORD CH"); kint(o->val, cs.ch_chord + 1, 0); }
            if (k == 1) { kfmt(o->name, 12, "BASS CH"); kint(o->val, cs.ch_bass + 1, 0); }
            if (k == 2) { kfmt(o->name, 12, "LAYER CH"); kint(o->val, cs.ch_layer + 1, 0); }
            if (k == 3) { kfmt(o->name, 12, "DRUMS CH"); kint(o->val, cs.ch_drums + 1, 0); }
            break;
        case 4: /* Reset: K4 the first step */
            if (k == 3) {
                kfmt(o->name, 12, "ACTION");
                kfmt(o->val, 14, cfr.arm ? "Reset" : "Keep");
            }
            break;
        default: /* Play */
            if (k == 0) {
                kfmt(o->name, 12, "VELOCITY");
                kint(o->val, cs.vel, 0);
                o->bar = (int16_t)(cs.vel * 255 / 127);
            }
            if (k == 1) {
                kfmt(o->name, 12, "GLIDE");
                kfmt(o->val, 14, cs.glide_always ? "Always" : "Legato");
            }
            if (k == 2) { kfmt(o->name, 12, "LIGHTS"); kfmt(o->val, 14, cs.lights ? "Scale" : "Played"); }
            if (k == 3) {
                static const char *const RT[CF_NRT] = {"Wait", "Notes", "Slide", "Chord"};   /* (stored: STRIKE, MOVE) */
                kfmt(o->name, 12, "RETRIG");
                kfmt(o->val, 14, RT[cs.retrig % CF_NRT]);
            }
            break;
        }
        break;
    default:
        break;
    }
}

#define CF_MSG_QOFF "QUANTIZE is Off: turn it on"   /* (KEY or SCALE turned while they are dimmed) */
static void cf_xnotes_up_all(void);              /* (below: the outside notes held go up) */
static void cpa_glide(int d)
{
    /* GLIDE: the chord sound's (0.5, the owner: "Если переключается инструмент, глайд переключается на тот
     * что был у другого инструмента"): an edit of the sound, kept as the others are (a pattern's own sound:
     * in the pattern; SAVE) */
    /* steps of about a quarter more each: Off, 6, 13, 23, 33, 44, 57, 71, 87, 105, 131, 160, 192, 236, 285,
     * 349, 420, 510, 625, 768, 944, 1158, 1463 ms (voice.c's TIME: 1 + g^2 / 8 control blocks) */
    static const uint8_t GS[] = {0, 8, 12, 16, 19, 22, 25, 28, 31, 34, 38, 42, 46, 51, 56, 62, 68, 75, 83, 92,
                                 102, 113, 127};
    int g = trk[CF_PART_CHORD].p[P_GLIDE], i = 0;
    while (i + 1 < (int)NELEM(GS) && GS[i + 1] <= g)
        i++;
    if (d < 0 && GS[i] < g)
        i++;                             /* (a value between two steps: down goes to the lower) */
    g = GS[clampi(i + d, 0, (int)NELEM(GS) - 1)];
    fm1_irq_off();
    trk[CF_PART_CHORD].p[P_GLIDE] = (int16_t)g;
    fm1_irq_on();
    cf_edited(CF_PART_CHORD);

}

/* Shared by the Harmony settings page and the sixth HOME keyboard guide. */
static void cf_harmony_turn(int k, int d)
{
    if (!d || k > 1) return;
    if (k == 0) ch.mode = (uint8_t)clampi(ch.mode + d, 0, CF_H_AUTO);
    else { cf_harmony_defaults(&ch); ch.mode=CF_H_AUTO; cf_message("Harmony defaults restored"); }
    cf_send_config();
    if (ch.mode == CF_H_AUTO && !cf_harmony_supported(cs.chord.scale, cs.chord.quantize))
        cf_message("Auto needs a 7-note scale\nand Quantize Basic or Full");
}

#include "cf_home_banks.c"
#include "cf_acc_ui.c"

static void cf_arp_controls_turn(int k,int d,int timing,int studio)
{
    if (!timing) {
        if (k == 0){
            if(studio)cgu_arp_turn(d);
            else if(cge_selected() && cu.page[SCR_ARP]==0)cge_play_it((unsigned)clampi(cge.arp_request+(d>0?1:-1),0,2));
            else cu.arp_on = (uint8_t)(d > 0);
        }
        else if (k == 1)
            cs.arp.mode = (uint8_t)clampi(cs.arp.mode + d, 0, CF_ARP_NMODES - 1);
        else if (k == 2)
            cs.arp.oct = (uint8_t)clampi(cs.arp.oct + d, 1, 4);
        else if (CF_ARP_STRUMMING(cs.arp.mode))
            cs.arp.strum_ms = (uint8_t)clampi(cs.arp.strum_ms + d * (cs.arp.strum_ms < 40 ? 1 : 5), 5, 250);
        else
            cs.arp.rate = (uint8_t)clampi(cs.arp.rate + d, 0, (int)CF_ARP_NRATES - 1);
    } else {
        if (k == 0 && !CF_ARP_STRUMMING(cs.arp.mode)) cs.arp.gate=(uint8_t)clampi(cs.arp.gate+d*5,20,100);
        else if (k == 1 && !CF_ARP_STRUMMING(cs.arp.mode)) cs.arp.swing=(uint8_t)clampi(cs.arp.swing+d,50,75);
        else if (k == 2) cs.arp.latch=(uint8_t)(d>0);
        else if (k == 3) { cpa_glide(d); return; }
    }
    cf_send_config();
}

static void ui_knob_turn(int k, int d)
{
    if(cgu_glow.active)return;
    if(d && cu.scr==SCR_HOME && cf_home_bank==CF_HB_CORE && (k==1 || k==2))cge.key_pin=1;
    if(d && (cu.scr==SCR_ENV || cu.scr==SCR_EDIT || cu.scr==SCR_BASS))cge_direct(CGE_SOUND);
    if(cu.scr==SCR_LAB){cfl_turn(k,d);return;}
    cu.knob_ms[k] = cu.now ? cu.now : 1u;
    cu.knob_last = (uint8_t)k;
    switch (cu.scr) {
    case SCR_HOME:
        if (home_stage() && cf_home_bank_turn(k, d)) break;
        if (cpa_running() && (k == 1 || k == 2)) { cpa_home_key(k, d); break; }
        if (k == 3) { cpa_tempo(d); break; }
        if (k == 0 && (cf_scene() == CF_SCENE_PROGRESSIONS || cpa_running())) {
            cf_progression_rhythm(d);
            break;
        }
        if (k == 0) {
            cs.chord.quantize = (uint8_t)clampi(cs.chord.quantize + d, 0, 2);
            if (cs.chord.quantize != CF_Q_OFF && cu.pop == POP_MSG && !memcmp(cu.msg, CF_MSG_QOFF, sizeof CF_MSG_QOFF))
                cu.pop = POP_NONE;                /* (the hint below: done) */
        } else if (k == 1 && cs.chord.quantize != CF_Q_OFF)
            cs.chord.tonic = (uint8_t)wrapi(cs.chord.tonic + d, 12);
        else if (k == 2 && cs.chord.quantize != CF_Q_OFF)
            cs.chord.scale = (uint8_t)clampi(cs.chord.scale + d, 0, CF_NSCALES - 1);
        else if (k == 1 || k == 2)                /* dimmed (QUANTIZE Off): it says what wakes it, every turn (the
                                                  * owner, 2026-10-20, after a reboot: "key и scale заблокировались") */
            cf_message(CF_MSG_QOFF);

        cu_spell_update();
        cf_send_config();
        if (ch.mode == CF_H_AUTO && !cf_harmony_supported(cs.chord.scale, cs.chord.quantize))
            cf_message("Auto needs a 7-note scale\nand Quantize Basic or Full");
        break;
    case SCR_SEL: {
        cf_form_t *f = sel_form();
        if (cf_is_off(&cs.chord, cu.sel_btn))
            break;
        if (ch.mode == CF_H_AUTO) {
            unsigned b = cu.sel_btn;
            if (k == 0) ch.slot[b] = (uint8_t)clampi(ch.slot[b] + d, 0, CF_MOD_N - 1);
            if (k == 2) ch.inv[b] = (int8_t)clampi(ch.inv[b] + d, CF_INV_AUTO, 3);
            if (k == 3) ch.bass[b] = (uint8_t)clampi(ch.bass[b] + d, 0, 5);
            cf_send_config();
            break;
        }
        if (k == 0)
            f->type = (uint8_t)clampi(f->type + d, 0, CF_NTYPES - 1);
        else if (k == 1) {
            int i = cf_adds_index(f->add);
            f->add = CF_ADDS[clampi((i < 0 ? 0 : i) + d, 0, CF_NADDS - 1)];
        } else if (k == 2)
            f->inv = (int8_t)clampi(f->inv + d, CF_INV_AUTO, 3);
        else
            f->bass = (uint8_t)clampi(f->bass + d, 0, 5);
        cf_send_config();
        break;
    }
    case SCR_ENV:
        if (cu.env_tgt == 3u) {                  /* the LAYER's own */
            cf_env_step(CF_PART_LAYER, k, d);
            cf_edited(2u);
            break;
        }
        if (cu.env_tgt != 2u)
            cf_env_step(CF_PART_CHORD, k, d);
        if (cu.env_tgt == 2u)
            cf_env_step(CF_PART_BASS, k, d);
        else if (cu.env_tgt == 0u)                   /* linked: the bass takes the chord's value */
            cf_env_set_val(CF_PART_BASS, k, cf_env_val(CF_PART_CHORD, k));
        cf_edited(cu.env_tgt == 2u ? 1u : 0u);
        if (!cu.env_tgt)
            cf_edited(1u);
        break;
    case SCR_EDIT:
        cfe_turn(cu.edit_part, cu.edit_sec, cu.page[SCR_EDIT], (uint32_t)k, d);
        cf_edited(cu.edit_part);
        break;
    case SCR_FX:
        cfx_knob_turn(k, d);
        break;
    case SCR_BASS:
        if (k == 0)
            cs.bass_on = (uint8_t)(d > 0);
        else if (k == 1)
            cs.bass_level = (uint8_t)clampi(cs.bass_level + d * 2, 0, 127);
        else if (k == 2)
            cs.bass_oct = (int8_t)clampi(cs.bass_oct + d, -2, 2);
        else
            cs.octaver = (uint8_t)(d > 0);
        cf_send_config();
        break;
    case SCR_SAVE: {
        uint32_t part = cf_save_part(), n;
        if (k == 0 && cf_snd[part].kind != PK_INIT) {
            cu.save_new = (uint8_t)(d > 0);
            cf_save_name_init();
        } else if (k == 1) {
            cu.save_cur = (uint8_t)clampi(cu.save_cur + d, 0, 11);
            for (n = str_len(cu.save_name); n <= cu.save_cur; n++)   /* (past the end: spaces up to it) */
                cu.save_name[n] = ' ';
            cu.save_name[n > str_len(cu.save_name) ? n : str_len(cu.save_name)] = 0;
        } else if (k == 3) {
            cu.save_del = (uint8_t)(d > 0 && cf_snd[part].kind == PK_USER);
        } else if (k == 2) {
            const char *p;
            int i, nc = (int)sizeof CF_NAME_CHARS - 1;
            char ch = cu.save_name[cu.save_cur] ? cu.save_name[cu.save_cur] : ' ';
            for (p = CF_NAME_CHARS; *p && *p != ch; p++)
                ;
            i = *p ? (int)(p - CF_NAME_CHARS) : 0;
            cu.save_name[cu.save_cur] = CF_NAME_CHARS[wrapi(i + d, nc)];
            if (cu.save_cur >= str_len(cu.save_name))
                cu.save_name[cu.save_cur + 1u] = 0;
        }
        break;
    }
    case SCR_ARP: cf_arp_controls_turn(k,d,cu.page[SCR_ARP]==1,0); break;
    case SCR_PEDIT: cfd_turn(k,d); break;
    case SCR_SONGS: csl_turn(k,d); break;
    case SCR_ACC: cf_acc_knob_turn(k,d); break;
    case SCR_SEQ: metro_knob_turn(k, d); break;
    case SCR_DRUM: dm_knob_turn(k, d); break;
    case SCR_SQ:
    case SCR_SQED: sq_knob_turn(k, d); break;
    case SCR_GLO:
        switch (cu.page[SCR_GLO]) {
        case CF_GLO_ENGINE_LOCKS: if(d && (d&1))cge.locks^=1u<<k; break;
        case CF_GLO_RECORDINGS:
            if (!k && d) { sq_pref.protect=d<0; sq_pref_dirty=1;
                cf_message(sq_pref.protect?"Full: new takes are refused":"New takes replace the oldest"); }
            break;
        case CF_GLO_LEGACY: if(k==0)cf_legacy_pick=(uint8_t)wrapi(cf_legacy_pick+d,3); break;
        case CF_GLO_BROWSE: cpb_knob_turn(k,d); break;
        case CF_GLO_ACCOMP:
            if (k == 0) cpa_ui_mode(d);
            if (k == 1) cpa_ui_rhythm(d);
            if (k == 2) cpa_tempo(d);
            if (k == 3) cpa_glide(d);
            break;
        case CF_GLO_VARIATION:
            cvr_knob_turn(k, d);
            break;
        case CF_GLO_HARMONY:
            cf_harmony_turn(k, d);
            break;
        case 6:
            if(k==0) cfd.page=(uint8_t)clampi(cfd.page+d,0,DP_N-1);
            if(k==1) { if(d>0 && !cfd.hold) cf_diag_sample(cfd.v,cu.now); cfd.hold=(uint8_t)(d>0); }
            if(k==2) { cf_diag_reset(cu.now); cf_diag_sample(cfd.v,cu.now); }
            if(k==3) cfd.rate=(uint8_t)clampi(cfd.rate+(d>0?1:-1),0,2);
            break;
        case 5:
            if (k < 3u) cf_bank[k] = (uint8_t)clampi(cf_bank[k] + d, 0, SIG8_FILTER_MAX);
            else cf_bank[0] = cf_bank[1] = cf_bank[2] = 0;
            break;
        case 0:
            if (k == 0 || k == 3) {
                if (k == 0) cf_skin = (uint8_t)wrapi(cf_skin + d, CF_SKIN_N);
                else { cf_motion = (uint8_t)(d > 0); cf_home_bank_ms = 0; cf_motion_epoch=cu.now; }
                cpb_dirty = 1; cpb_changed = cu.now;
            }
            if (k == 1) home_view_select(d);
            if (k == 2) {
                cs.boot_sound = (uint8_t)(d > 0);
                cf_boot_pending=0;cf_boot_go = cs.boot_sound && !cr_safe ? 1 : 2; /* audition On; fade/cancel Off */
            }
            cf_palette_build();
            break;
        case 1:
            if (k == 0)
                cs.midi_out = (uint8_t)(d > 0);
            if (k == 1)                          /* Off, In, Out */
                cs.clock_in = (uint8_t)clampi(cs.clock_in + (d > 0 ? 1 : -1), 0, 2);
            if (k == 2 || k == 3)                /* (the outside notes held go up first: cf_xnotes_up_all) */
                cf_xnotes_up_all();
            if (k == 2)                          /* FM-1 keys, Roots, Direct */
                cs.notes_in = (uint8_t)clampi(cs.notes_in + (d > 0 ? 1 : -1), 0, CF_NI_N - 1);
            if (k == 3 && cs.notes_in != CF_NI_DIRECT)   /* All, 1 .. 16 */
                cs.in_ch = (uint8_t)clampi(cs.in_ch + d, CF_INCH_ALL, CF_INCH_ALL + 16);
            cf_send_config();
            break;
        case 2:
            if (k == 0)
                cs.ch_chord = (uint8_t)clampi(cs.ch_chord + d, 0, 15);
            if (k == 1)
                cs.ch_bass = (uint8_t)clampi(cs.ch_bass + d, 0, 15);
            if (k == 2)
                cs.ch_layer = (uint8_t)clampi(cs.ch_layer + d, 0, 15);
            if (k == 3)
                cs.ch_drums = (uint8_t)clampi(cs.ch_drums + d, 0, 15);
            cf_send_config();
            break;
        case 4:                                  /* Reset: K4 arms it (the steps after: SAVE) */
            if (k == 3 && !cfr.go) {
                cfr.arm = (uint8_t)(d > 0);
                cfr.fin = 0;
                cfr.hold = 0;
            }
            break;
        default:
            if (k == 0)
                cs.vel = (uint8_t)clampi(cs.vel + d * 4, 1, 127);
            if (k == 1)
                cs.glide_always = (uint8_t)(d > 0);
            if (k == 2)
                cs.lights = (uint8_t)(d > 0);
            if (k == 3)
                {                        /* Wait, Move, Strike: from holding the chord to striking it */
                    static const uint8_t ORD[CF_NRT] = {CF_RT_WAIT, CF_RT_MOVE, CF_RT_STRIKE, CF_RT_CHORD};
                    int i = 0;
                    while (i < CF_NRT - 1 && ORD[i] != cs.retrig)
                        i++;
                    cs.retrig = ORD[clampi(i + (d > 0 ? 1 : -1), 0, CF_NRT - 1)];
                }
            cf_send_config();
            break;
        }
        break;
    default:
        break;
    }
}

/* --------------------------------------------------------- the left side --- */
static void cf_pop(uint32_t kind, uint32_t ms)
{
    cu.pop = (uint8_t)kind;
    cu.pop_until = cu.now + ms;
}
static void cf_message(const char *s)
{
    uint32_t i;
    str_cpy(cu.msg, s, sizeof cu.msg);
    for (i = 0; cu.msg[i] && cu.msg[i] != '\n'; i++)
        ;
    cf_pop(POP_MSG, cu.msg[i] ? 2000u : 1200u);  /* (two lines: longer to read; 2 s, as the first take's hint) */
}
/* what the kept files (upreset.c, fm6_store.c) say goes into the message popup */
static void ui_message(const char *s) { cf_message(s); }
static void ui_say(const char *a, const char *b)
{
    char m[24];
    str_cpy(m, a, sizeof m);
    str_cpy(m + str_len(m), b, (uint32_t)(sizeof m - str_len(m)));
    cf_message(m);
}

static uint32_t screen_pages(uint32_t scr)
{
    if (scr == SCR_PEDIT)return CFD_NTAB;
    if (scr == SCR_ACC)return CF_ACC_N;
    if (scr == SCR_GLO)
        return CF_GLO_NPAGES;
    if (scr == SCR_ENV)
        return 3u;                               /* chord + bass, chord, bass */
    if (scr == SCR_ARP)
        return 2u;                               /* Pattern, Feel */
    if (scr == SCR_SAVE)
        return CF_SAVE_NPG;                      /* the chord's sound, the bass's, the LAYER's (a pattern's copy: OCT+
                                                  * held on the SEQUENCER, cf_sequi.c) */
    if (scr == SCR_EDIT)
        return cfe_npages(cu.edit_part, cu.edit_sec);
    return 1u;
}

/* a page turned (SELECT): which way, when; the picture slides in from that side, that arrow lights (cf_views.c) */
static uint32_t cv_turn_ms;
static int8_t cv_turn_dir;
static void cv_turned(int d)
{
    cv_turn_ms = cu.now ? cu.now : 1u;
    cv_turn_dir = (int8_t)(d > 0 ? 1 : -1);
}

static void ui_select(int d)                     /* SELECT: the bass sound, or a screen's page */
{
    if(cgu_glow.active){cpa_tempo(d);return;}
    if(cgs_home()){cgu_select(d);return;}
    if(home_stage() && cf_home_bank==CF_HB_FLOW){
        if(cge_pins_open)cge_pin_move(d);
        return;
    }
    if(home_stage() && (cge_selected() || (cf_home_bank==CF_HB_ENGINE && !cpa_running()))){
        cge_preview_turn(d);
        return;
    }
    if(cu.scr==SCR_LAB){cfl_select(d);return;}
    cf_nav_dir=d<0?-1:1;
    if(cu.scr==SCR_SONGS){csl_select(d);return;}
    if(cu.scr==SCR_PEDIT){
        if(cfwd.have && !cfdu.conflict && !cfdu.confirm){
            if(cfdu.studio)cu.page[SCR_PEDIT]=(uint8_t)wrapi(cu.page[SCR_PEDIT]+d,CFD_NTAB);
            else cfdu.focus=(uint8_t)wrapi(cfdu.focus+d,cfwd.song.n);
        }
        return;
    }
    if(cu.scr==SCR_ACC){cf_acc_select(d);return;}
    if(cpb_page()) { cf_progression_select(d); return; }
    if (cu.scr == SCR_HOME && home_view() == HV_FLOWER && cf_scene() == CF_SCENE_PROGRESSIONS) {
        cf_progression_select(d);
        return;
    }
    if (cu.scr == SCR_DRUM) {                    /* the drum machine: the rhythm */
        dm_select(d);
        return;
    }
    if (cu.scr == SCR_SEQ) {                     /* the METRONOME: where its SEQ goes, the SEQUENCER or the drum machine
                                                  * (it took the bass sound, as HOME's SELECT; the bass sound stays on
                                                  * HOME and the other pages) */
        int v = clampi(dm_seq_to + d, 0, 1);
        if (v != dm_seq_to)
            cv_turned(d);
        dm_seq_to = (uint8_t)v;
        return;
    }
    if (cu.scr == SCR_SQ) {                      /* SEQUENCER: the track, the song's place */
        sq_pick(d);
        return;
    }
    if (cu.scr == SCR_SQED) {                    /* a track: its step (DRUMS: its bar) */
        sq_carry_turn(d);
        return;
    }
    if (cu.scr == SCR_ENV) {                     /* chord + bass (linked: the bass follows), chord, bass, the LAYER */
        cu.env_tgt = (uint8_t)wrapi(cu.env_tgt + d, 4);
        cv_turned(d);
        if (cu.env_tgt < 3u) {                   /* (the LAYER's page leaves the chord and bass linked or not) */
            cs.env_link = (uint8_t)(cu.env_tgt == 0u);
            if (cs.env_link)
                cf_env_follow(), cf_edited(1u);
        }
        cf_send_config();                        /* (the keys play the LAYER on its page) */
        return;
    }
    if (cu.scr == SCR_EDIT) {
        int was = cu.page[SCR_EDIT];
        cu.page[SCR_EDIT] = (uint8_t)clampi(cu.page[SCR_EDIT] + d, 0, (int)screen_pages(SCR_EDIT) - 1);
        if (cu.page[SCR_EDIT] != was)
            cv_turned(d);
        return;
    }
    if (cu.scr == SCR_FX) { cfx_select(d); return; }
    if (screen_pages(cu.scr) > 1u) {
        cu.page[cu.scr] = (uint8_t)wrapi(cu.page[cu.scr] + d, (int)screen_pages(cu.scr));
        cv_turned(d);
        if (cu.scr == SCR_SAVE)
            cf_send_config();                    /* (the LAYER's page: the keys play its sound) */
        if (cu.scr == SCR_SAVE) {                /* the other part: its sound, its name */
            uint32_t part = cf_save_part();
            cu.save_del = 0;
            cu.save_new = cf_snd[part].kind == PK_INIT || !(cu.edited[part] || cf_snd[part].kind == PK_USER);
            cf_save_name_init();
        }
        return;
    }
    if (cu.scr == SCR_SEL) {                     /* SEL: SELECT picks the button to edit, as the keys do */
        int nb = (cs.chord.algo == CF_ALG_TOP || cs.chord.algo == CF_ALG_TOP_LOCK) ? CF_TOP_NBTN : CF_SIDE_NBTN;
        cu.sel_btn = (uint8_t)wrapi(cu.sel_btn + d, nb);
        return;
    }
    if(d)cge_direct(CGE_SOUND);
    if (!cf_snd_step(CF_PART_BASS, d)) return;
    if (cs.env_link)
        cf_env_follow();
    ui_snd_heard(CF_PART_BASS);
    cf_pop(POP_SOUND_BASS, 1200);
}
static void ui_presets(int d)                    /* PRESETS: the chord sound (a sequencer's track open: its sound) */
{
    if(cgu_glow.active){cgu_perform_turn(d);return;}
    if(cgs_home()){
        if(cgu.tab==CGU_STUDIO && cgs.zoom && !cgu.band_panel && cgs.focus==CGS_DRUMS)cgu_perform_turn(d);
        else if(cgu.tab==CGU_SOUND){
            if(cgu.rack_controls==CGU_RACK_MIX)return;
            if(cgs.zoom && !cgu.rack_controls && cgu.sound_page==CGU_PRESETS)cfx_select(d);
            else cgu_sound_step(cgs.focus,d);
        }
        else if(cgu.tab==CGU_STUDIO)cgs_genre_turn(d);
        return;
    }
    if (dm_holding()) {                          /* (EDIT held in DRUMS: a rhythm for them, cf_dmui.c) */
        dm_hold_turn(d);
        return;
    }
    if (cu.scr == SCR_SQED || (cu.scr == SCR_SQ && squ.pat < SQ_NPAT)) {
        sq_presets(d);                           /* (the pattern's own sound for the frame's track: 0.5) */
        return;
    }
    if(d)cge_direct(CGE_SOUND);
    if (!cf_snd_step(CF_PART_CHORD, d)) return;
    if (cs.env_link)                             /* linked: the bass takes the new sound's envelope */
        cf_env_follow();
    ui_snd_heard(CF_PART_CHORD);
    cf_pop(POP_SOUND_CHORD, 1200);
}
static void ui_algo(int d)                       /* ALGORITHM: scene, effect, or workspace tab */
{
    if(cgu_glow.active){
        cgu_glow.view=(uint8_t)wrapi(cgu_glow.view+d%CGU_GLO_VIEWS,CGU_GLO_VIEWS);
        return;
    }
#if !CF_SCENES
    if(cgs_home()){cgu_focus(d);return;}
    if(home_stage()) {
        if(!d)return;
        if(cge_pins_open){cge_pin_move(d);return;}
        cf_nav_dir=d<0?-1:1;
        cf_home_bank=(uint8_t)wrapi((int)cf_home_bank+d%CF_HB_N,CF_HB_N);
        cf_home_bank_ms=cf_motion?(cu.now?cu.now:1u):0;
        if(cu.pop==POP_SOUND_CHORD || cu.pop==POP_SOUND_BASS)cu.pop=POP_NONE;
        return;
    }
#endif
    if(cu.scr==SCR_LAB){cfl_tab(d);return;}
    if(cu.scr==SCR_ARP){cu.page[SCR_ARP]=(uint8_t)wrapi(cu.page[SCR_ARP]+d,cge_selected()?3:2);return;}
    cf_nav_dir=d<0?-1:1;
    if(cu.scr==SCR_SONGS)return;
    if(cu.scr==SCR_PEDIT){
        if(cfwd.have && !cfdu.conflict && !cfdu.confirm){
            if(cfdu.studio)cfdu.focus=(uint8_t)wrapi(cfdu.focus+d,cfwd.song.n);
            else cu.page[SCR_PEDIT]=(uint8_t)wrapi(cu.page[SCR_PEDIT]+d,CFD_NTAB);
        }
        return;
    }
    if(cu.scr==SCR_ACC){cu.page[SCR_ACC]=(uint8_t)wrapi(cu.page[SCR_ACC]+d,CF_ACC_N);cf_acc_focus=0;cf_acc_page_ms=cf_motion?(cu.now?cu.now:1u):0;return;}
    if (cu.scr == SCR_FX) {
        if (fxpu.mode != FXP_MANAGE) cfx_view(d);
        return;
    }
    if (cu.scr == SCR_SQ) {                      /* SEQUENCER: the pattern, then the song */
        sq_algo(d);
        return;
    }
    if (!d) return;
#if CF_SCENES
    int pos = cf_scene() == CF_SCENE_PROGRESSIONS ? 0 : (int)cf_scene() + 1;
    pos = wrapi(pos + d % CF_SCENE_N, CF_SCENE_N);
    cf_scene_set(pos ? (uint32_t)(pos - 1) : CF_SCENE_PROGRESSIONS);
#endif
    if (cu.scr == SCR_HOME)
        cs.seq_hint &= (uint8_t)~(3u << HV_SHIFT); /* show the scene immediately, keeping hints and scene selection */
    cf_message(CF_SCENE_NAME[cf_scene()]);         /* no CONFIG event: a held chord is untouched */
}

/* --------------------------------------------------------------- buttons --- */
/* the sound of the sequencer's track left last (the chord's, the bass's, the LAYER's; 0xFF: none yet): EDIT, ENV and
 * SAVE open on it (the owner, 2026-10-19: what was edited before HOME is what the editors take) */
static uint8_t cu_ctx = 0xFF, cu_env_prev;
static void ui_ctx_from_track(uint32_t trk)
{
    static const uint8_t PART[SQ_NTRK] = {CF_PART_CHORD, CF_PART_BASS, CF_PART_LAYER, 0xFF};
    uint8_t p = PART[trk % SQ_NTRK];
    if (p == 0xFFu)
        return;                                  /* (DRUMS: the editors stay as they were) */
    cu_ctx = p;
    cu.edit_part = p;
    cu.page[SCR_EDIT] = 0;
    if (p == CF_PART_LAYER && cu.env_tgt != 3u) {   /* (ENV: the LAYER's page; back to where it was after) */
        cu_env_prev = cu.env_tgt;
        cu.env_tgt = 3;
    } else if (p != CF_PART_LAYER && cu.env_tgt == 3u) {
        cu.env_tgt = cu_env_prev < 3u ? cu_env_prev : (cs.env_link ? 0u : 1u);
    }
}

static void ui_open(uint32_t scr)
{
    uint32_t was = cu.scr;
    int was_studio=cgu_play_part()>=0;
    if(was==SCR_LAB)cla_request(0);
    if (sq_screen(was) && !sq_screen(scr) && scr != SCR_SEQ)
        ui_ctx_from_track(squ.trk);
    cu.scr = (uint8_t)(scr == was && scr != SCR_HOME ? SCR_HOME : scr);
    if(cu.scr!=SCR_PEDIT && cu.scr!=SCR_SONGS)cfdu.studio=0;
    if(cu.scr!=SCR_HOME){cge_pins_open=0;cge_preview=255;}
    if (cu.scr == SCR_HOME && was != SCR_HOME) {
        int from_jam=cf_home_bank==CF_HB_ENGINE || cf_home_bank==CF_HB_FLOW;
        if(CF_STUDIO && !CF_SCENES){
            cf_home_bank=CF_HB_ENGINE;
            home_boot_main=1;
            cf_scene_set(CF_SCENE_PROGRESSIONS);
        } else cf_home_bank=from_jam && (was==SCR_ARP || was==SCR_LAB)?CF_HB_ENGINE:CF_HB_CORE;
        cf_home_bank_ms=0;
    }
    if(cgu_glow.active)cgu_glow_open(0);
    if(cu.scr==SCR_HOME && was==SCR_FX && cgu.return_screen==SCR_FX)cgu_sound_page(CGU_PRESETS);
    if(cu.scr!=cgu.return_screen)cgu.return_screen=0;
    if(cu.scr==SCR_ACC && was!=SCR_ACC)cf_acc_page_ms=0;
    if (cu.scr == SCR_SEQ && was != SCR_SEQ)
        dm_seq_to = 0;                           /* (the carousel starts on the SEQUENCER: SEQ twice opens it, as before) */
    if (cu.scr == SCR_SEL && cf_is_off(&cs.chord, cu.sel_btn))
        cu.sel_btn = sel_first();
    if ((was == SCR_SEQ || sq_screen(was)) != (cu.scr == SCR_SEQ || sq_screen(cu.scr)))
        cf_send_metro();
    if (sq_screen(was) || sq_screen(cu.scr) || was == SCR_EDIT || cu.scr == SCR_EDIT || was == SCR_ENV ||
        cu.scr == SCR_ENV || was == SCR_SAVE || cu.scr == SCR_SAVE)   /* (the LAYER open or left: where the keys play,
                                                  * cf_send_config) */
        cf_send_config();
    else if(was_studio || cgu_play_part()>=0)cf_keys_changed();
    squ.help = squ.help_hold = 0;
    if (cu.scr != was) {                         /* a hold begun on one screen does nothing on the next: OCT+ held in a
                                                  * track and let go on the SEQUENCER copied a pattern, SEL held cleared
                                                  * a track there at once, REC let go on HOME started a take (a review,
                                                  * 2026-10-19) */
        squ.oct_down = squ.sel_down = 0;
        squ.ocpy = squ.cpy = 0;
        cu.rec_hold = 0;
    }
}

/* GLOBAL > Reset: everything the user made goes (cf_backup.c cf_factory_erase), after three steps, each its own
 * gesture (the owner, 2026-10-19: "полный резет, с подтверждениями несколькими"): K4 to Reset; SAVE held 3 s; SAVE
 * once more within 6 s. Leaving the page, letting SAVE go early or the 6 s passing undoes them */
static int cf_reset_page(void) { return cu.scr == SCR_GLO && cu.page[SCR_GLO] == 4u; }
static void cf_reset_poll(void)                  /* every frame */
{
    uint32_t now = cu.now;
    if (cfr.go) {                                /* (a frame or two of "Resetting" on the screen first) */
        if (now - cfr.go_ms > 250u && cfr.go == 1u) {
            cfr.go = 2;
            cf_factory_erase();                  /* (the unit restarts; the emulator says what to do) */
            cf_message("Reset done: restart");
        }
        return;
    }
    if (!cf_reset_page()) {
        cfr.arm = cfr.fin = 0;
        cfr.hold = 0;
        return;
    }
    if (cfr.hold) {
        if (!((fm1_in.buttons >> panel.btn[B_SAVE]) & 1u)) {
            cfr.hold = 0;                        /* (let go early: nothing) */
            cf_message("Hold SAVE 3 seconds");
        } else if (now - cfr.hold >= CF_RESET_HOLD_MS) {
            cfr.hold = 0;
            cfr.fin = 1;
            cfr.fin_ms = now;
        }
    }
    if (cfr.fin && now - cfr.fin_ms > CF_RESET_ASK_MS) {
        cfr.fin = cfr.arm = 0;
        cf_message("Reset cancelled");
    }
}
/* SAVE on the Reset page: the next step, or how to begin */
static void cf_reset_save(void)
{
    if (cfr.go)
        return;
    if (cfr.fin) {
        cfr.fin = 0;
        cfr.go = 1;
        cfr.go_ms = cu.now;
        cf_post(CFE_SEQ, 0, 0, 0);               /* (the sequencer stops) */
    } else if (cfr.arm) {
        cfr.hold = cu.now ? cu.now : 1u;
    } else {
        cf_message("Turn ACTION to Reset");
    }
}

/* REC went down: the audio side marks the moment, as the scan saw it (the frame lets a button through later than the
 * keys: a chord struck just after REC can get there first), and the chord begun since is the take's (0.6.1) */
static void cf_rec_down(void)
{
    uint32_t age = (fm1_ticks() - CF_BTN_T0(panel.btn[B_REC] % 14u)) / FM1_TICKS_PER_US / 1000u;
    cf_post(CFE_SEQ, 8, age <= 250u ? age : 0u, 0);
}
static void ui_button(uint32_t b)
{
    if (cfr.go)                                  /* (erasing: no button any more) */
        return;
    if (cf_reset_page() && b == B_SAVE) {
        cf_reset_save();
        return;
    }
    if (cfr.fin && b != B_SAVE) {                /* (the last question: any other button says no) */
        cfr.fin = cfr.arm = 0;
        cf_message("Reset cancelled");
        return;
    }
    if(b==B_GLO && cgu_glow_available()){
        cgu_glow_open(!cgu_glow.active);
        return;
    }
    if(cgu_glow.active){
        unsigned bit=1u<<cgs.focus;
        if(b==B_HOME){cgu_glow_open(0);return;}
        if(b==B_SCL){cgs.solo^=(uint8_t)bit;return;}
        if(b==B_EDIT){cgs.muted^=(uint8_t)bit;return;}
        if(b==B_SAVE){
            cgu_glow_open(0);
            if(cu.scr!=SCR_GLO){
                if(cgs_home())cgu.return_screen=SCR_GLO;
                cvr_context_from_screen();ui_open(SCR_GLO);
            }
            return;
        }
        if(b==B_PLAY && !sqp.on && !sqk.on && cf_sn.sq_rec==255 && (!cpa.on || cgs_active())){
            if(cpa_commanded)cpa_ui_play();
            else cgs_new_take(1);
            return;
        }
        if(b==B_REC)cgu_glow_open(0);
        else if(b!=B_PLAY)return;
    }
    if(cu.scr==SCR_LAB) {
        if(b==B_SCL){if(clu.tab==CFL_COMPARE || clu.tab==CFL_NEXT)cfl_audition();else {ui_open(SCR_HOME);home_boot_main=1;}return;}
        if(b==B_EDIT){cfl_edit();return;}
        if(b==B_PLAY){cla_request(0);cpa_ui_play();return;}
        if(b==B_SAVE){cla_request(0);csl_open();return;}
        cla_request(0);
    }
    if(cgs_home()){
        if(b==B_SEQ && cgu.tab==CGU_STUDIO){
            cgu_studio_seq();return;
        }
        if(b==B_SAVE && cgu.tab==CGU_RECORD){
            if(!cgu.rack_controls)return;
            if(sqk.on || cf_sn.sq_on || cf_sn.sq_rec!=255)cf_message("Stop the take before clearing");
            else if(sq_has(cgu.take_slot) || (sq_unavailable&(1u<<cgu.take_slot)))cgu.clear_down=cu.now?cu.now:1u;
            return;
        }
        if(b==B_ARP){
            cgu_open_arp();return;
        }
        if(b==B_FX){
            if(cgu.tab!=CGU_SOUND)cgu_tab_turn(CGU_SOUND-cgu.tab);
            if(!cgs.zoom || cgu.rack_controls || cgu.sound_page!=CGU_PRESETS){
                cgu.rack_controls=0;
                cgu_set_zoom(1);
                cgu_sound_page(CGU_PRESETS);
            }
            return;
        }
        if(b==B_SEQ && cgu.tab==CGU_SOUND){
            if(cgu.rack_controls==CGU_RACK_MIX)return;
            if(cgs.zoom && !cgu.rack_controls)cgu_sound_page_turn(1);
            else cgu.all_sounds^=1u;
            return;
        }
        if(cgu.tab==CGU_STUDIO || cgu.tab==CGU_SOUND){
            if(b==B_EDIT && cgs.zoom && cgu.tab==CGU_SOUND && !cgu.rack_controls && cgu.sound_page==CGU_PRESETS){
                cfx_sync();fxpu.mode=FXP_EDIT;cfxu.view=CFX_INDIVIDUAL;
                cu.fx_pos=0;
                for(unsigned i=0;i<CFX_N;i++)if(cs.fx.order[i]==CFX_FILTER)cu.fx_pos=(uint8_t)i;
                cgu.return_screen=SCR_FX;ui_open(SCR_FX);return;
            }
            if(b==B_ENV && cgs.zoom){
                unsigned page=cgu.sound_page==CGU_ENVELOPE?CGU_TONE:CGU_ENVELOPE;
                if(cgu.tab!=CGU_SOUND)cgu_tab_turn(CGU_SOUND-cgu.tab);
                cgu.band_panel=cgu.rack_controls=0;cgu_set_zoom(1);cgu_sound_page(page);
                return;
            }
            if(b==B_SAVE && cgs.zoom && cgu.tab==CGU_SOUND && !cgu.rack_controls && cgu.sound_page==CGU_PRESETS){
                cgu.return_screen=SCR_FX;fxp_manage_open();ui_open(SCR_FX);return;
            }
            if(b==B_LFO && cgs.zoom && !cgu.band_panel && !cgu.rack_controls && cgs.focus!=CGS_DRUMS){
                cu.edit_part=cgs.focus;cgu.return_screen=SCR_EDIT;ui_open(SCR_EDIT);return;
            }
        }
        if(b==B_SCL){
#if CHASE_LIBRARY && FELUCCA_VA
            if(cgu.tab==CGU_SOUND && cgs.zoom && cgu.rack_controls==CGU_RACK_LIBRARY && !cgu.all_sounds){cgu_apply_palette();return;}
#endif
            if(cgu.tab==CGU_SOUND && cgs.zoom && !cgu.rack_controls && cgu.sound_page==CGU_PRESETS)cfx_recall();
            else if(cgu_progression_panel())(void)cgs_select_progression(cgu.progression_pick,0);
            else cgu_set_zoom(!cgs.zoom);
            return;
        }
        if(b==B_HOME){
            if(cgs.zoom)cgu_set_zoom(0);
            else cgu_home();
            return;
        }
        if(b==B_EDIT){cgu_edit();return;}
        if(b==B_REC && cgu.tab!=CGU_RECORD)cgu_tab_turn(CGU_RECORD-cgu.tab);
    }
    if(cgs_home() && b==B_PLAY && !cpa_running() && !cpa_commanded && !sqp.on && !sqk.on){
        cgs_new_take(1);
        return;
    }
    if(b==B_SCL && home_stage() && cf_home_bank==CF_HB_FLOW){cge_pin_select();return;}
    if(b==B_SCL && home_stage() && (cge_selected() || cf_home_bank==CF_HB_ENGINE) && cge_preview_apply())return;
    if(b==B_SCL && home_stage() && cf_home_bank==CF_HB_ENGINE){cfl_open();return;}
    if(b==B_PLAY && home_stage() && (cf_home_bank==CF_HB_ENGINE || cf_home_bank==CF_HB_FLOW) &&
       !cpa_running() && !cpa_commanded && !sqp.on && !sqk.on){
        cge_notice=0;
        if((cge_selected() && cge.vibe_request==cge.current.vibe) || cge_load())cpa_ui_play();
        return;
    }
    if(b==B_SCL && cpb_home()){cfl_open();return;}
    if(b==B_SCL && cu.scr==SCR_GLO && cu.page[SCR_GLO]==CF_GLO_HARMONY){ui_open(SCR_SEL);return;}
    if(b==B_EDIT && (cu.scr==SCR_PEDIT || cu.scr==SCR_ACC || cpb_home())) {
        if(cu.scr==SCR_PEDIT){ui_open(SCR_HOME);home_boot_main=1;}else cfd_open();return;
    }
    if(cu.scr==SCR_PEDIT && b==B_SCL){cfd_execute();return;}
    if(cu.scr==SCR_PEDIT && cfdu.studio && b==B_PLAY){
        if(!cfdu.confirm && (!cpa_commanded || cpa_running()) && cgs_select_progression(1,!cpa_running())){
            cgu.progression_pick=1;
            ui_open(SCR_HOME);
        }
        return;
    }
    if(b==B_SAVE && cvr_page_active()){cvr_save();return;}
    if(b==B_SAVE && (cu.scr==SCR_PEDIT || cu.scr==SCR_ACC || cpb_home())){csl_open();return;}
    if(cu.scr==SCR_SONGS) {
        if(b==B_SAVE){cslu.mode=CSL_LIST;return;}
        if(b==B_SCL){csl_execute();return;}
        if(b==B_EDIT){csl_edit();return;}
        if(b==B_REC){cf_message("HOME to start recording");return;}
        if(b==B_PLAY && !cpa_running() && !cpa_commanded && !cf_sn.sq_on){cf_message("SEL loads the selected Song");return;}
    }
    if(cu.scr==SCR_PEDIT && b==B_PLAY && !cpa.on && !sqk.on && cf_sn.sq_rec==255)cfd_select_source();
    if(cu.scr==SCR_GLO && cu.page[SCR_GLO]==CF_GLO_LEGACY && b==B_SCL) {
        static const uint8_t scr[]={SCR_SQ,SCR_SEQ,SCR_DRUM};ui_open(scr[cf_legacy_pick]);cu.pop=POP_NONE;return;
    }
    if(cu.scr==SCR_ACC && b==B_SCL){if(cf_acc_tab()==CF_ACC_GROOVE)cf_acc_restore();return;}
    if (b == B_PLAY && (cu.scr==SCR_PEDIT || cu.scr==SCR_ACC || cpa_running() || cpa_commanded || cpb_page() || (cu.scr == SCR_HOME && cf_scene() == CF_SCENE_PROGRESSIONS && !sqk.on && !cf_sn.sq_on))) {
        cpa_ui_play(); return;
    }
    if (cu.scr == SCR_DRUM) {                    /* the rhythm machine: the SEQUENCER's grammar (the owner on rc3: "когда в
                                                  * неё заходишь не должны нажиматься кнопки других вкладок, как и в
                                                  * секвенсоре", "гло должна быть с подсказками"): HOME and SEQ leave, the
                                                  * other pages' buttons do nothing and are dark, GLO shows the help */
        if (b == B_FX || b == B_ENV || b == B_LFO || b == B_ARP || b == B_SAVE)
            return;
        if (b == B_EDIT) {                       /* (EDIT opens, as in the SEQUENCER: the rhythm as a new pattern, its
                                                  * DRUMS; rc5) */
            dm_make_pattern();
            return;
        }
        if (b == B_GLO) {
            squ.glo_down = cu.now ? cu.now : 1u;
            return;
        }
    }
    if (sq_screen(cu.scr)) {                     /* the sequencer's own: EDIT / SEL open, SEL clears, SAVE back, GLO */
        if (cu.scr == SCR_SQED && b == B_ARP) {  /* ARP in a track: its own arpeggiator on / off (CHORDS, LAYER) */
            sq_arp_toggle();
            return;
        }
        if (cu.scr == SCR_SQED && b == B_EDIT) { /* EDIT held in a track: SELECT moves the frame's chord */
            sq_edit_press();
            return;
        }
        if (cu.scr == SCR_SQ && b == B_SAVE) {
            sq_clear_slot = squ.pat; sq_clear_down = cu.now ? cu.now : 1u;
            cf_message("Hold SAVE 3s: clear recording"); cu.pop_until = cu.now + 3500u; return;
        }
        if (b == B_FX || b == B_ENV || b == B_LFO || b == B_ARP)   /* (SAVE: nothing on the SEQUENCER; a pattern's copy is OCT+'s) */
            return;                              /* (no way out to another screen here but HOME and SEQ: the owner,
                                                  * 2026-10-19; their lights are off) */
        if (cu.scr == SCR_SQ && b == B_EDIT) {
            if (sq_open()) {
                ui_open(SCR_SQED);
                squ.nofr = 0;                    /* (opened: the frame where it was) */
                sq_hint_once(SQH_TRACK);         /* (the first track opened: the way back) */
            }
            return;
        }
        if (cu.scr == SCR_SQ && b == B_SCL) {      /* SEL takes one thing out of the frame, never many: on the song its
                                                  * pattern (the pattern stays); a track: nothing at a tap, held 1 s it
                                                  * is cleared (sq_sel_poll) */
            if (squ.pat == SQ_SONGPG) {
                sq_song_remove();
            } else {
                squ.sel_down = cu.now ? cu.now : 1u;
                squ.cpy = 0;
            }
            return;
        }
        if (cu.scr == SCR_SQED && b == B_SCL) {   /* (a tap clears on its release; held, SELECT copies the bar:
                                                  * sq_sel_poll) */
            squ.sel_down = cu.now ? cu.now : 1u;
            squ.cpy = 0;
            return;
        }
        if (cu.scr == SCR_SQED && b == B_SAVE) {
            ui_open(SCR_SQ);
            return;
        }
        if (b == B_GLO) {
            squ.glo_down = cu.now ? cu.now : 1u;
            return;
        }
        if ((b == B_PLAY || b == B_REC) && dm_holding()) {   /* (EDIT held in DRUMS: called off, cf_dmui.c) */
            dm_hold_cancel();
            return;
        }
        if (b == B_PLAY) {                       /* the pattern shown round, or the song from the place picked */
            sq_play();
            return;
        }
        if (b == B_REC) {                        /* a tap: a take; held 1 s: cancel the new take */
            cu.rec_hold = cu.now ? cu.now : 1u;
            cu.rec_fired = 0;
            cf_rec_down();
            return;
        }
    }
    switch (b) {
    case B_HOME:
        if(CF_STUDIO && !CF_SCENES){
            int from_rack=cgu.return_screen && cgu.return_screen==cu.scr;
            ui_open(SCR_HOME);
            home_boot_main=1;
            cf_home_bank=CF_HB_ENGINE;
            cf_scene_set(CF_SCENE_PROGRESSIONS);
            if(!from_rack)cgu_home();
            cf_keys_changed();
            break;
        }
        if (home_stage()) {
            if(cge_pins_open){cge_pins_open=0;break;}
            cf_nav_dir=1;
            cf_home_bank = (uint8_t)((cf_home_bank + 1u) % CF_HB_N);
            cf_home_bank_ms = cf_motion ? (cu.now ? cu.now : 1u) : 0u;
            if (cu.pop == POP_SOUND_CHORD || cu.pop == POP_SOUND_BASS) cu.pop = POP_NONE;
        } else {
            if(cu.scr==SCR_HOME)cf_home_bank=CF_HB_CORE;
            ui_open(SCR_HOME); home_boot_main = 1;
            cf_scene_set(CF_SCENE_PROGRESSIONS);
            cf_home_bank_ms = 0;
        }
        break;
    case B_SCL:                                  /* (the drum machine: SEL takes the rhythm's own KIT and SWING back) */
        if (cu.scr == SCR_FX) { if (fxpu.mode == FXP_BROWSE) cfx_recall(); }
        else if (cu.scr == SCR_DRUM)
            dm_sel();
        else
            ui_open(SCR_SEL);
        break;
    case B_ENV: ui_open(SCR_ENV); break;
    case B_LFO: ui_open(SCR_BASS); break;
    case B_EDIT: ui_open(SCR_EDIT); break;
    case B_GLO:
        if(cpb_home())cpbu.menu_hold=cu.now?cu.now:1u;
        cvr_context_from_screen(); ui_open(SCR_GLO); break;
    case B_FX:
        if (cu.scr == SCR_FX && fxpu.mode == FXP_MANAGE) { fxpu.mode = FXP_BROWSE; cu.save_hold = 0; }
        else if (cu.scr == SCR_FX) ui_open(SCR_HOME);
        else { cfx_sync(); fxpu.mode = FXP_BROWSE; cfxu.view = CFX_MACROS; ui_open(SCR_FX); }
        break;
    case B_ARP:
        if(home_stage() && (cf_home_bank==CF_HB_ENGINE || cf_home_bank==CF_HB_FLOW))cu.page[SCR_ARP]=0;
        ui_open(SCR_ARP); break;
    case B_SEQ:                                  /* METRONOME, again the SEQUENCER (or the drum machine: the METRONOME's
                                                  * carousel), again HOME */
        if(cu.scr==SCR_ACC) {ui_open(SCR_HOME);home_boot_main=1;}
        else if(cu.scr==SCR_SEQ)ui_open(dm_seq_to?SCR_DRUM:SCR_SQ);
        else if(sq_screen(cu.scr) || cu.scr==SCR_DRUM){ui_open(SCR_HOME);home_boot_main=1;}
        else {cu.page[SCR_ACC]=CF_ACC_GROOVE;cf_acc_focus=0;cf_acc_page_ms=0;ui_open(SCR_ACC);}
        break;
    case B_SAVE:
        if(cpb_home() || cpb_page()) {
            cpbu.idea=cpb_home()?cf_progression_pick(cpg.idea,0):cpb_target(); cpbu.save_hold=cu.now?cu.now:1u; cpbu.fired=0; break;
        }
        if (cu.scr == SCR_GLO && cu.page[SCR_GLO] == CF_GLO_VARIATION) { cvr_save(); break; }
        if (cu.scr == SCR_FX) {
            if (fxpu.mode != FXP_MANAGE) fxp_manage_open();
            else cu.save_hold = cu.now ? cu.now : 1u;
            break;
        }
        /* on SAVE: held 1 s saves, a tap goes HOME (cf_save_poll) */
        if (cu.scr == SCR_SAVE) {
            cu.save_hold = cu.now ? cu.now : 1u;
        } else {
            cf_save_open();
        }
        break;
    case B_PLAY:                                 /* the last pattern plays / stops (cf_sequi.c sq_home_play) */
        if (cu.scr == SCR_DRUM)
            dm_page_play();
        else
            sq_home_play();
        break;
    case B_REC:                                  /* a tap records; a hold cancels the current new take */
        cu.rec_hold = cu.now ? cu.now : 1u;
        cu.rec_fired = 0;
        cf_rec_down();
        break;
    case B_OCTDN:
    case B_OCTUP:
        if (b == B_OCTUP && (cu.scr == SCR_SQED || (cu.scr == SCR_SQ && squ.pat < SQ_NPAT)) &&
            !((fm1_in.buttons >> panel.btn[B_OCTDN]) & 1u)) {   /* held with SELECT turned (a track) it copies the
                                                  * frame's bar, with ALGORITHM turned (SEQUENCER) the pattern; a tap is
                                                  * OCT+ on its release (sq_oct_poll) */
            squ.oct_down = cu.now ? cu.now : 1u;
            squ.ocpy = 0;
            break;
        }
        if ((fm1_in.buttons & ((1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]))) ==
            ((1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]))) {
            cs.chord.octave = 0;                 /* both: back to 0, and every note let go */
            squ.oct_down = 0;                    /* (the OCT+ waiting for its release on the sequencer's screens: not
                                                  * after this; it left the octave at +1) */
            squ.ocpy = 0;
            cf_post(CFE_PANIC, 0, 0, 0);
        } else {
            cs.chord.octave = (int8_t)clampi(cs.chord.octave + (b == B_OCTUP ? 1 : -1), -3, 3);
        }
        cf_send_config();
        cf_pop(POP_OCTAVE, 900);
        break;
    default: break;
    }
}

/* ---------------------------------------------------------------- SAVE --- */
/* SAVE keeps a part's sound (cf_sound.c cf_snd_save): SELECT the part (the one being edited, else the chord, opens),
 * K1 over itself or as new (a sound not edited: as new), K2 the letter, K3 what it is; SAVE held 1 s saves */
static void cf_save_name_init(void)
{
    uint32_t part = cf_save_part(), n;
    str_cpy(cu.save_name, cf_snd[part].name, sizeof cu.save_name);
    n = str_len(cu.save_name);
    if (cu.save_new && n + 2u <= 12u)            /* a new one: its name with a 2, to tell them apart */
        str_cpy(cu.save_name + n, " 2", 3);
    cu.save_cur = (uint8_t)(str_len(cu.save_name) ? str_len(cu.save_name) - 1u : 0u);
}
static void cf_save_open(void)
{
    uint32_t part = cu.scr == SCR_EDIT ? cu.edit_part : cu.scr == SCR_ENV ? env_part() : cu_ctx < 3u ? cu_ctx :
                    (!cu.edited[0] && cu.edited[1]) ? 1u : (!cu.edited[0] && cu.edited[2]) ? 2u : 0u;
    cu.page[SCR_SAVE] = (uint8_t)part;
    cu.save_new = cf_snd[part].kind == PK_INIT || !(cu.edited[part] || cf_snd[part].kind == PK_USER);
    cu.save_hold = 0;
    cu.save_del = 0;
    cf_save_name_init();
    ui_open(SCR_SAVE);
}
/* a user sound replaced (m: it now) or deleted (m 0): every pattern that keeps it as its own takes it as it is now,
 * or goes back to HOME's sound (its tweak on it with it), so no pattern keeps a sound that is gone (its letter on HOME,
 * PRESETS turning its sound) */
static void ui_snd_slot_changed(uint32_t slot, const cf_snd_t *m)
{
    uint32_t p, t;
    for (p = 0; p < SQ_NPAT; p++)
        for (t = 0; t < 3u; t++) {
            const cf_sref_t *r = &cfps.r[p][t];
            if (!r->own || r->kind != PK_USER || r->slot != slot)
                continue;
            if (m) {
                cfps_set(p, t, m);
            } else {
                cfps_set(p, t, 0);
                cfk_drop(p + 1u, t);
            }
        }
}
static void cf_save_delete(void)
{
    uint32_t part = cf_save_part();
    char b[24], nm[13];
    int res;
    cf_sref_t home[3];
    uint32_t cur = sq_cur_pat(), slot = cf_snd[part].slot;
    str_cpy(nm, cf_snd[part].name, sizeof nm);
    sq_home_keep(home);
    res = cf_snd_delete(part);
    if (res) {
        cf_message(res == -1 ? "Only your sounds delete" : res == -3 ? "Safe mode: no saving" : "Delete error");
        return;
    }
    sq_home_put(home);                           /* (the row moved: HOME's sounds follow, as what they are) */
    cfk_drop(cur < SQ_NPAT && cfps.r[cur][part].own ? cur + 1u : 0u, part);
    ui_snd_slot_changed(slot, 0);                /* (only a user sound deletes: its slot) */
    cu.edited[part] = 0;
    cu.save_del = 0;
    str_cpy(b, "Deleted ", sizeof b);
    str_cpy(b + str_len(b), nm, 13);
    cf_message(b);
    ui_open(SCR_HOME);
}
static void cf_save_do(void)
{
    uint32_t part = cf_save_part(), i, n;
    char name[13], b[24], num[4];
    int res;
    str_cpy(name, cu.save_name, sizeof name);
    for (n = str_len(name); n && name[n - 1u] == ' '; n--)   /* (no trailing spaces) */
        name[n - 1u] = 0;
    for (i = 0; name[i] == ' '; i++)
        ;
    {
        cf_sref_t home[3];
        uint32_t cur = sq_cur_pat();
        int own = cur < SQ_NPAT && cfps.r[cur][part].own;
        sq_home_keep(home);
        res = cf_snd_save(part, cu.save_new, name + i);
        if (res) {
            cf_message(res == -1 ? "No free slot (32 used)" : res == -2 ? "INIT: save it as new" :
                       res == -3 ? "Safe mode: no saving" : "Save error");
            return;
        }
        sq_home_put(home);                       /* (the row moved: HOME's sounds follow, as what they are) */
        if (!cu.save_new && cf_snd[part].kind == PK_USER)
            ui_snd_slot_changed(cf_snd[part].slot, &cf_snd[part]);   /* (replaced, maybe renamed: who keeps it) */
        if (own) {                               /* (the pattern's own sound is the saved one now, its tweak in it) */
            cfps_set(cur, part, &cf_snd[part]);
            cfk_drop(cur + 1u, part);
            cfk_take(part, cur + 1u);
        } else {
            if (part == CF_PART_LAYER)
                cs.snd_layer = (uint16_t)cf_snd_pos(part);
            else
                cs.snd[part] = (uint16_t)cf_snd_pos(part);
            cfk_drop(0, part);
            cfk_take(part, 0);
        }
    }
    cu.edited[part] = 0;
    snd_num(cf_snd_pos(part), num);
    str_cpy(b, "Saved ", sizeof b);
    str_cpy(b + str_len(b), num, 4);
    str_cpy(b + str_len(b), " ", 2);
    str_cpy(b + str_len(b), cf_snd[part].name, 13);
    cf_message(b);
    ui_open(SCR_HOME);
}
/* every frame: PLAY and REC, a tap (on its release) or a hold. PLAY held 0.6 s: the LOOPER screen; REC tapped: the
 * loop's action and the LOOPER screen; REC held 1 s: the last layer undone (as ChoralRoot's) */
static void cf_rec_poll(void)
{
    if (!cf_sn.ac_on && cf_sn.ext_clock && cf_sn.ext_bpm >= 30u && cf_sn.ext_bpm <= 300u && cf_sn.ext_bpm != cs.bpm) {
        cs.bpm = cf_sn.ext_bpm;                  /* (MIDI clock in: the tempo it makes, on the knobs; the audio side
                                                  * has it already) */
        fm1_irq_off();
        cfp_cfg.bpm = cs.bpm;
        fm1_irq_on();
    }
    if (cu.rec_hold && sq_screen(cu.scr)) {      /* the sequencer: tap records, hold cancels a new take */
        int held = (fm1_in.buttons >> panel.btn[B_REC]) & 1u;
        if (held && !cu.rec_fired && cu.now - cu.rec_hold >= 1000u) {
            cu.rec_fired = 1;
            sq_home_cancel();
            if (cu.scr == SCR_SQED && sq_empty(sq_cur()))
                ui_open(SCR_SQ);                 /* (the pattern back to nothing: its tracks) */
            else if (cu.scr == SCR_SQED)
                sq_clamp();
        } else if (!held) {
            if (!cu.rec_fired)
                sq_rec_tap();
            cu.rec_hold = 0;
        }
        return;
    }
    if (cu.rec_hold) {                           /* away from the sequencer: tap a new take, hold cancels it */
        int held = (fm1_in.buttons >> panel.btn[B_REC]) & 1u;
        if (held && !cu.rec_fired && cu.now - cu.rec_hold >= 1000u) {
            cu.rec_fired = 1;
            sq_home_cancel();
        } else if (!held) {
            if (!cu.rec_fired) {
                if (cu.scr == SCR_DRUM)
                    dm_page_rec();
                else
                    sq_home_rec();
            }
            cu.rec_hold = 0;
        }
    }
}

/* every frame: SAVE held on the SAVE screen */
static void cf_save_poll(void)
{
    int held;
    if (!cu.save_hold)
        return;
    held = (fm1_in.buttons >> panel.btn[B_SAVE]) & 1u;
    if (cu.scr == SCR_FX && fxpu.mode == FXP_MANAGE) {
        if (held && cu.now - cu.save_hold >= 1000u) { cu.save_hold = 0; fxp_manage_do(); }
        else if (!held) { cu.save_hold = 0; fxpu.mode = FXP_BROWSE; }
    } else if (cu.scr != SCR_SAVE) {
        cu.save_hold = 0;
    } else if (held && cu.now - cu.save_hold >= 1000u) {
        cu.save_hold = 0;
        if (cu.save_del)
            cf_save_delete();
        else
            cf_save_do();
    } else if (!held) {                          /* a tap: back HOME, as every screen's own button */
        cu.save_hold = 0;
        ui_open(SCR_HOME);
    }
}

/* ------------------------------------------------------------- the keys --- */
/* on SEL a chord button pressed is also picked for editing (it still plays) */
static void ui_note_key_aged(int key, int down, int age_ms);
/* HOME's PRESETS / SELECT changed the sound you hear (0.5): the current pattern's own, if it has one, else HOME's */
static void ui_snd_heard(uint32_t part)
{
    if(part<2)cge_direct(CGE_SOUND);
    uint32_t cur = sq_cur_pat();
    if (cur < SQ_NPAT && cfps.r[cur][part].own) {
        cfps_set(cur, part, &cf_snd[part]);
        cfk_drop(cur + 1u, part);                /* (another sound: its tweak goes) */
        cfk_take(part, cur + 1u);
        return;
    }
    if (part == CF_PART_CHORD)
        cs.snd[0] = (uint16_t)cf_snd_pos(CF_PART_CHORD);
    else if (part == CF_PART_BASS)
        cs.snd[1] = (uint16_t)cf_snd_pos(CF_PART_BASS);
    cfk_drop(0, part);
    cfk_take(part, 0);
}
static void ui_note_key(int key, int down) { ui_note_key_aged(key, down, -1); }
static void ui_note_key_aged(int key, int down, int age_ms)   /* age_ms: -1 the scan's (CF_KEY_T0) */
{
    int idx;
    if(cgu_glow_key((unsigned)key,down))return;
    if (sq_key(key, down))                       /* (DRUMS: the keys pick the drum and set the steps) */
        return;
    if(down && key>=0 && key<CF_NKEYS && cgu_play_part()==CGS_DRUMS){
        int lane=0;
        for(int i=0;i<key;i++)lane+=cf_key_is_upper(i)==cf_key_is_upper(key);
        sq_eaten|=1u<<key;
        cf_post(CFE_SEQ,2,(unsigned)lane%SQ_NLANE,(int)(cgu_kit()|100u<<8));
        return;
    }
    if (down && key >= 0 && key < CF_NKEYS && home_stage() &&
        (cf_home_bank==CF_HB_LEARN || (cgs_home() && cgu.tab==CGU_HELP && cgu.topic==CGU_HARMONY)))
        cf_learn_focus = (uint8_t)key; /* observe the normal play path; never consume the key */
    if (cu.scr == SCR_EDIT && cf_key_is_upper(key)) {   /* EDIT: OP1..OP6 PIT GLO pick, MONO / POLY: bass / chords */
        int u = 0, k;
        for (k = 0; k < key; k++)
            u += cf_key_is_upper(k);
        if (down) {
            if (u < CFE_NSEC) {
                cu.edit_sec = (uint8_t)u;
                cu.page[SCR_EDIT] = (uint8_t)clampi(cu.page[SCR_EDIT], 0, (int)cfe_npages(cu.edit_part, (uint32_t)u) - 1);
            } else if (u >= 8 && u <= 10) {      /* MONO the bass's sound, POLY the chord's (the keys' print: the
                                                  * owner, 2026-10-19), the next the LAYER's */
                cu.edit_part = (uint8_t)edit_key_part(u);
                cu.page[SCR_EDIT] = 0;
                cf_send_config();                /* (the keys play the sound edited: the LAYER's too) */
            }
        }
        return;
    }
    if (down && cu.scr == SCR_SEL && cf_key_role(cs.chord.algo, key, &idx) == CF_ROLE_BUTTON &&
        !cf_is_off(&cs.chord, idx))
        cu.sel_btn = (uint8_t)idx;
    {                                            /* (a key down goes with its age: how long ago the scan saw it closed,
                                                  * hal/fm1_input.h note_t0. A frame may draw for tens of ms before the
                                                  * keys are read: a take puts the chord where it was played, not where
                                                  * the frame let it through. The owner, 2026-10-20: a take from HOME
                                                  * "квантизируется не супер по таймингу") */
        uint32_t age = !down ? 0u : age_ms >= 0 ? (uint32_t)age_ms : (fm1_ticks() - CF_KEY_T0(key % 27)) / FM1_TICKS_PER_US / 1000u;
        cf_post(CFE_KEY, (uint32_t)key, age <= 250u ? age : 0u, down);   /* (longer: stale, none) */
    }
}

/* the keys, between the frame's tile rows too. Keys that changed in one scan went at once: the ones let go first,
 * then the chord buttons pressed, then the roots, so the new chord sounds with its new buttons (in key order a root
 * could come first and strike with the old ones: the owner heard the last chord slip in) */
/* an outside keyboard's notes (NOTES IN FM-1 keys or Roots; cf_out.c cf_xq): FM-1 keys folds each note by octaves
 * into F3..G5 and presses that key (the FM-1's own key down too: nothing more); Roots sends it as a root at its
 * pitch, but on the SEQUENCER's DRUMS row, where the keys are drum pads, as a key. A note goes up the way it went down */
static void cf_xnote_up(uint32_t n)              /* outside note n let go: its root, or the FM-1 key it pressed */
{
    uint32_t v = cu.xnote[n & 127u], k;
    cu.xnote[n & 127u] = 0;
    if (v == 0xFFu) {
        if (cu.xroots)
            cu.xroots--;
        cf_post(CFE_ROOT, n, 0, 0);
    } else if (v) {
        k = v - 1u;
        if (cu.xcnt[k] && --cu.xcnt[k] == 0u) {
            cu.xkeys &= ~(1u << k);
            if (!((cu.pkeys >> k) & 1u))
                ui_note_key((int)k, 0);
        }
    }
}
/* NOTES IN or IN CH changed: every outside note still down goes up (its note-off may no longer get through: a key
 * held while IN CH moved to another channel stayed down for good) */
static void cf_xnotes_up_all(void)
{
    uint32_t n;
    for (n = 0; n < 128u; n++)
        if (cu.xnote[n])
            cf_xnote_up(n);
}
static void cf_xq_drain(void)
{
    uint32_t w = cf_xq_w;
    while (cf_xq_r != w) {
        uint32_t e = cf_xq[cf_xq_r % CF_XQ], n = e & 0x7Fu, down = (e >> 7) & 1u, k;
        uint32_t age = fm1_ms - cf_xq_ms[cf_xq_r % CF_XQ];
        cf_xq_r++;
        cu.idle_ms = 0;
        if (down) {
            if (cu.xnote[n])
                continue;                        /* (already down: a repeated note-on) */
            if (cs.notes_in == CF_NI_ROOTS && !sq_frame_trk(SQ_DRUMS)) {
                cu.xnote[n] = 0xFF;
                cu.xroots++;
                cf_post(CFE_ROOT, n, age <= 250u ? age : 0u, 1);
                continue;
            }
            k = cf_panel_pitch_key(n);
            cu.xnote[n] = (uint8_t)(k + 1u);
            if (cu.xcnt[k]++ == 0u) {
                cu.xkeys |= 1u << k;
                if (!((cu.pkeys >> k) & 1u))
                    ui_note_key_aged((int)k, 1, age <= 250u ? (int)age : 0);
            }
        } else {
            cf_xnote_up(n);
        }
    }
}

static int cgu_glow_available(void)
{
    return CF_STUDIO && !CF_SCENES && !cr_safe && !cu.upd_left &&
        !cfr.go && !cfr.fin && !cf_reset_page();
}

static void cf_ui_pump(void)
{
    /* tap: a key pressed and let go since the last read (the scan keeps its press, hal/fm1_input.h notes_pressed): a
     * frame that drew longer than the press lost it. Right after power-on (the intro, the first full redraw) a quick
     * tap on a chord button went that way, and the roots played single notes (the owner, 2026-10-21: "после включения
     * если выбрать тип аккорда и начать играть, то сначала играются по одной ноте") */
    uint32_t now = fm1_in.notes, tap = fm1_input_note_edges() & ~now & ~cu.pkeys & ((1u << CF_NKEYS) - 1u);
    uint32_t ch = ((now ^ cu.pkeys) | tap) & ~cu.xkeys, k, pass;   /* (a key held from outside too: no change) */
    if (!cu.booted)
        return;
    if (ch) {
        for (pass = 0; pass < 3u; pass++)
            for (k = 0; k < CF_NKEYS; k++)
                if ((ch >> k) & 1u) {
                    int down = (int)(((now | tap) >> k) & 1u), idx;
                    int btn = cf_key_role(cs.chord.algo, (int)k, &idx) == CF_ROLE_BUTTON;
                    if ((uint32_t)(down ? (btn ? 1 : 2) : 0) == pass)
                        ui_note_key((int)k, down);
                }
        for (k = 0; k < CF_NKEYS; k++)           /* (a tap's release, after the roots it went with) */
            if ((tap >> k) & 1u & ~(cu.xkeys >> k))
                ui_note_key((int)k, 0);
        cu.idle_ms = 0;
    }
    cu.pkeys = now;
    cf_xq_drain();
    cu.keys = cu.pkeys | cu.xkeys;
}

static void cf_ui_input(void)
{
    uint32_t rel = 0, pr = fm1_input_edges(&rel), i;
    int32_t d;
    uint32_t glo=1u<<panel.btn[B_GLO];
    if((pr&glo) && cgu_glow_available()){
        ui_button(B_GLO);
        pr&=~glo;
        cu.idle_ms=0;
    }
    sq_home_poll();                              /* finish the current take in whole bars */
    sq_snd_poll();                               /* (the current pattern's sounds: 0.5) */
    cf_glide_migrate();                          /* (the old GLIDE into HOME's sound, once) */
    cf_ui_pump();
    cpb_hold_poll();
    cf_save_poll();
    cf_reset_poll();
    cf_rec_poll();
    sq_glo_poll();
    sq_write_poll();
    sq_rec_poll();
    sq_sel_poll();
    sq_oct_poll();
    sq_edit_poll();
    sq_save_poll();
    sq_clear_poll();
    cgu_clear_poll();
    dm_poll();                                   /* (the drum machine: what was chosen last, its settings kept) */
    for (i = 0; i < 14u; i++)
        if ((pr >> i) & 1u) {
            uint32_t b = panel_btn_of(i);
            if (b < NB)
                ui_button(b);
            cu.idle_ms = 0;
        }
    if ((d = panel_enc(EN_SELECT)) != 0)
        ui_select(d), cu.idle_ms = 0;
    if ((d = panel_enc(EN_PRESET)) != 0)
        ui_presets(d), cu.idle_ms = 0;
    if ((d = panel_enc(EN_ALGO)) != 0)
        ui_algo(d), cu.idle_ms = 0;
    for (i = 0; i < 4u; i++)
        if ((d = panel_enc(EN_K1 + i)) != 0)
            ui_knob_turn((int)i, d), cu.idle_ms = 0;
    cvr_poll();                                  /* queued variation, after input/cancel */
}

/* ------------------------------------------------------------ key caps --- */
static void ui_keycap(int key, cf_keycap_t *o)
{
    int idx, role = cf_key_role(cs.chord.algo, key, &idx), down = (cu.keys >> key) & 1u;
    const cf_chord_out_t *out = &cf_sn.out;
    if (sq_keycap(key, o))
        return;
    if (cu.scr == SCR_EDIT && cf_key_is_upper(key)) {
        int u = 0, k;
        for (k = 0; k < key; k++)
            u += cf_key_is_upper(k);
        if (u < CFE_NSEC) {
            str_cpy(o->top, cfe_key_name(cu.edit_part, (uint32_t)u), 8);
            o->style = u == cu.edit_sec ? KC_ON : KC_BTN;
        } else if (u >= 8 && u <= 10) {
            str_cpy(o->top, u == 8 ? "BAS" : u == 9 ? "CHD" : "LAY", 8);
            o->style = edit_key_part(u) == cu.edit_part ? KC_SOUNDING : KC_BTN;
        } else {
            o->style = KC_OFF;
        }
        return;
    }
    if (role == CF_ROLE_BUTTON) {
        const cf_form_t *f = cf_button_form(&cs.chord, idx);
        int hold = cf_is_off(&cs.chord, idx), on = hold ? out->hold : (out->buttons >> idx) & 1u;
        o->btn = 1;
        if (hold)
            str_cpy(o->top, "HOLD", 8);          /* (the Off form's key: HOLD since 0.5, lit while on) */
        else if (ch.mode == CF_H_AUTO)
            str_cpy(o->top, cf_modifier_label(ch.slot[idx]), 8);
        else
            cf_form_label(f, o->top);
        if (!hold && ch.mode == CF_H_AUTO && ((out->conflict >> idx) & 1u))
            str_cpy(o->bot, "!", 8);
        /* held and in the chord: pressed; latched (Lock): lit; held but left out of the chord (a minor button with
         * a major one, cf_chord.c cfc_used): plain, so it plainly does not count */
        o->style = down && on ? KC_DOWN : on ? KC_ON : KC_BTN;
        if (cu.scr == SCR_SEL && idx == cu.sel_btn && !down)
            o->style = KC_SOUNDING;
        return;
    }
    {
        int n = cf_key_root_note(&cs.chord, key), pc = n % 12, i, sounding = 0;
        char nm[4];
        cf_pc_name(pc, &cu_spell, nm);
        str_cpy(o->top, nm, 8);
        if (cs.chord.quantize != CF_Q_OFF)
            cf_numeral(cs.chord.scale, cs.chord.tonic, pc, o->bot);
        if (o->bot[0]) {                         /* the degree as a digit, its flat or sharp kept (bVII -> b7) */
            int d = 0, j, v = 0, k = 0;
            char acc = o->bot[0] == 'b' || o->bot[0] == '#' ? o->bot[0] : 0;
            for (j = 0; o->bot[j]; j++) {
                char ch = o->bot[j];
                if (ch == 'I' || ch == 'i')
                    v++;
                else if (ch == 'V' || ch == 'v')
                    d = v ? 5 - v : 5, v = 0;
            }
            d += v;
            if (acc)
                o->bot[k++] = acc;
            o->bot[k++] = (char)('0' + (d >= 1 && d <= 7 ? d : 0));
            o->bot[k] = 0;
        }
        for (i = 0; i < cf_sn.n; i++)
            sounding |= cf_sn.note[i] % 12 == pc;
        o->style = down ? KC_DOWN : (out->sounding && out->root_pc == pc) ? KC_SOUNDING :
                   (cs.chord.quantize != CF_Q_OFF && pc == cs.chord.tonic) ? KC_SCALE : KC_ROOT;
        (void)sounding;
    }
}

/* ----------------------------------------------------------- the lights --- */
static uint8_t cu_led_pos[41];
static void cf_led_init(void)
{
    uint32_t id, p, r;
    for (id = 0; id < 41u; id++) {
        cu_led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][p] == (int8_t)id)
                    cu_led_pos[id] = (uint8_t)((p << 3) | r);
    }
}
static void cf_led(uint8_t *m, uint32_t id, int on)
{
    uint8_t q = cu_led_pos[id % 41u];
    if (q != 0xFF && on)
        m[q >> 3] |= (uint8_t)(1u << (q & 7u));
}

/* The guide follows gestures, not the beat or redraws. Keep the next step across
 * rests; repeated chords consume one card per root press, including with HOLD.
 * Selection/key/scale changes and leaving the progression view start it over. */
static struct {
    uint32_t press;
    uint16_t matches;
    uint8_t valid, idea, tonic, scale, quantize, current, next, src, seq;
} cpg_follow;

static uint16_t cf_progression_cue(uint16_t *playing)
{
    const cf_chord_out_t *o = &cf_sn.out;
    *playing = 0;
    if (!(home_stage() || cu.scr == SCR_ACC) ||
        !cf_harmony_supported(cs.chord.scale, cs.chord.quantize)) {
        cpg_follow.valid = 0;
        cpg_follow.press = cf_sn.root_press;
        cpg_follow.seq = cf_sn.sq_dser;
        return 0;
    }
    if (cf_sn.ac_on) {
        cpg_follow.valid = 0;
        cpg_follow.press = cf_sn.root_press;
        cpg_follow.seq = cf_sn.sq_dser;
        if(o->sounding && !o->single)*playing|=(uint16_t)(1u<<o->root_pc);
        if(cf_sn.sq_dn && !cf_sn.sq_dsingle)*playing|=(uint16_t)(1u<<(cf_sn.sq_droot%12));
        if (!cf_sn.ac_n || cf_sn.ac_slot >= cf_sn.ac_n || cf_sn.ac_paused || cf_sn.ac_lost) return 0;
        /* Drums-only still advances the phrase. Guide its current/next roots
         * from the shared plan without creating melodic notes. Live roots own
         * the steady cue while played; the upcoming cue stays clock-driven. */
        if (cf_sn.ac_mode == CPA_DRUMS && !cf_sn.ac_waiting && !o->sounding)
            *playing |= (uint16_t)(1u << (cf_sn.ac_root[cf_sn.ac_slot] % 12));
        unsigned next = cf_sn.ac_waiting ? cf_sn.ac_slot : (cf_sn.ac_slot + 1u) % cf_sn.ac_n;
        if (!cf_sn.ac_waiting && !next && cf_sn.ac_repeat_off) return 0;
        return (uint16_t)(1u<<(cf_sn.ac_root[next]%12));
    }
    const cf_progression_t *p = cf_progression_current();
    uint8_t src = o->sounding && o->n ? 1u : cf_sn.sq_on && cf_sn.sq_dn ? 2u : 0u;
    uint16_t matches = src == 1u ? (o->single ? 0 : cf_progression_highlight(p, cs.chord.scale,
        cs.chord.tonic, o->root_pc, o->mask)) : src == 2u ? (cf_sn.sq_dsingle ? 0 :
        cf_progression_highlight(p, cs.chord.scale, cs.chord.tonic, cf_sn.sq_droot % 12, cf_sn.sq_dmask)) : 0;
    if(src==1u && !o->single)*playing=(uint16_t)(1u<<o->root_pc);
    else if(src==2u && !cf_sn.sq_dsingle)*playing=(uint16_t)(1u<<(cf_sn.sq_droot%12));
    int reset = !cpg_follow.valid || cpg_follow.idea != cpg.idea || cpg_follow.tonic != cs.chord.tonic ||
        cpg_follow.scale != cs.chord.scale || cpg_follow.quantize != cs.chord.quantize;
    int gesture = src == 1u ? cf_sn.root_press != cpg_follow.press :
        src == 2u && cf_sn.sq_dser != cpg_follow.seq;
    if (reset) {
        cpg_follow.valid = 1;
        cpg_follow.idea = cpg.idea;
        cpg_follow.tonic = cs.chord.tonic;
        cpg_follow.scale = cs.chord.scale;
        cpg_follow.quantize = cs.chord.quantize;
        cpg_follow.current = CF_P_NONE;
        cpg_follow.next = 0;
    }
    int correction = matches != cpg_follow.matches && (cpg_follow.current == CF_P_NONE ||
        !(matches & (1u << cpg_follow.current)));
    if (matches && (gesture || (!reset && (src != cpg_follow.src || correction)))) {
        unsigned step = cpg_follow.next;
        /* Prefer the expected card. Replaying the current chord waits there;
         * another matching chord resynchronizes forward through the idea. */
        if (!(matches & (1u << step)) && cpg_follow.current < p->n &&
            (matches & (1u << cpg_follow.current))) step = cpg_follow.current;
        for (unsigned i = 0; i < p->n; i++, step = (step + 1u) % p->n)
            if (matches & (1u << step)) {
                cpg_follow.current = (uint8_t)step;
                cpg_follow.next = (uint8_t)((step + 1u) % p->n);
                break;
            }
    }
    cpg_follow.src = src;
    cpg_follow.matches = matches;
    cpg_follow.press = cf_sn.root_press;
    cpg_follow.seq = cf_sn.sq_dser;
    int root=cf_progression_root(p,cpg_follow.next,cs.chord.scale,cs.chord.tonic);
    return (uint16_t)(1u<<root);
}

/* the sequencer's lights. The buttons say what works here: EDIT blinks on the tracks (open one), SAVE (back) and SEL
 * (clear, when there is something to clear) are up in a track, REC goes dark (it does nothing here), PLAY is up when
 * there is something to play. The keys: DRUMS as a drum machine's (the lower keys the drum's steps in the bar, a
 * running light on the step that plays; the upper keys the drums, the one picked lit, those that strike in the bar
 * dim, each flashing as it sounds); CHORDS / BASS / LAYER light as on HOME (below). Returns 1 when the key is lit
 * here (the keys' rules below skip it) */
static int sq_key_led(uint32_t k, uint8_t *lit, uint8_t *mid, uint8_t *dim, uint8_t *faint)
{
    const sq_pat_t *p = sq_cur();
    uint32_t id = 14u + k;
    if (cu.scr == SCR_SQ && squ.pat == SQ_NPAT) {   /* the song: the patterns' keys (the frame's lit, the others with
                                                  * steps dim, the empty ones faint); the lower keys dark */
        int j, u = 0;
        if (!cf_key_is_upper((int)k))
            return 1;
        for (j = 0; j < (int)k; j++)
            u += cf_key_is_upper(j);
        if (u < (int)SQ_NPAT)
            cf_led((cu.keys >> k) & 1u || (sq_st.song.n && sq_st.song.pat[squ.slot] == u) ? lit :
                   sq_has((uint32_t)u) ? dim : faint, id, 1);
        return 1;
    }
    if (cu.scr == SCR_SQ && squ.pat < SQ_NPAT && squ.trk == SQ_DRUMS) {   /* the Drums row: the drums on the upper
                                                  * keys (the picked one lit, each flashing as it sounds), the lower
                                                  * keys dark */
        int j, i = 0;
        if (!cf_key_is_upper((int)k))
            return 1;
        for (j = 0; j < (int)k; j++)
            i += cf_key_is_upper(j);
        if (i < (int)SQ_NLANE)
            cf_led((cu.keys >> k) & 1u || i == squ.lane ||
                   ((int32_t)(cf_sn.now_ms - cf_sn.sq_lane_t[i]) < 110 && cf_sn.sq_lane_t[i]) ? lit : faint, id, 1);
        return 1;
    }
    if (cu.scr != SCR_SQED || (cu.keys >> k) & 1u)
        return 0;
    if (squ.trk == SQ_DRUMS) {
        int j, i = 0, st = SQ_ST(p);
        for (j = 0; j < (int)k; j++)
            i += cf_key_is_upper(j) == cf_key_is_upper((int)k);
        if (cf_key_is_upper((int)k)) {
            if (i >= (int)SQ_NLANE)
                return 1;
            if ((int32_t)(cf_sn.now_ms - cf_sn.sq_lane_t[i]) < 110 && cf_sn.sq_lane_t[i])
                cf_led(lit, id, 1);
            else
                cf_led(i == squ.lane ? lit : p->hit[i][squ.bar] ? dim : faint, id, 1);
            return 1;
        }
        if (i >= st)
            return 1;
        {                                        /* the picked drum's hits full, the beats lit less under them (the
                                                  * owner, 2026-10-20: "доли слегка подсвечиваются на клавишах, а
                                                  * поставленные на дорожку барабаны светятся во всю"); the running
                                                  * light full on an empty step and dark on a hit, so it shows there
                                                  * too */
            int hit = (p->hit[squ.lane][squ.bar] >> i) & 1u;
            if (cf_sn.sq_on && cf_sn.sq_pat == squ.pat &&   /* (the song's too) */
                sq_trk_bar(p, SQ_DRUMS, cf_sn.sq_n) == squ.bar && cf_sn.sq_bt &&
                (int)(cf_sn.sq_t * (uint32_t)st / cf_sn.sq_bt) == i)
                cf_led(lit, id, !hit);
            else if (hit)
                cf_led(lit, id, 1);
            else if ((i * cs.beats) % st == 0)  /* (the middle level: dim was hard to see in daylight, "ещё бы чуть
                                                  * поярче", the owner, 2026-10-20; nothing lies between dim and it) */
                cf_led(mid, id, 1);
        }
        return 1;
    }
    /* CHORDS / BASS / LAYER: the keys light as on HOME, as the screen's caps do: what they play and write (held, the
     * buttons latched in Lock, the scale). They showed the step under the frame (blinking) or the chord playing while
     * the caps showed the keys' own: a latched m9 on the screen, the step's sus2 sus4 on the keys (the owner,
     * 2026-10-19, a video; "как на HOME"). The step's chord is its name over the grid and its flower */
    return 0;
}

/* Fade across the panel's four PWM brightness levels. The foreground owns
 * these envelopes; the audio snapshot contributes only held notes and hits. */
static struct {
    uint32_t at;
    uint8_t part, level[CF_NKEYS];
} cgu_key_fade;

static void cgu_led_update(int part)
{
    if(part<0){cgu_key_fade.part=0;return;}
    unsigned dt=cu.now-cgu_key_fade.at;
    cgu_key_fade.at=cu.now;
    if(cgu_key_fade.part!=part+1){
        cgu_key_fade.part=(uint8_t)(part+1);
        memset(cgu_key_fade.level,0,sizeof cgu_key_fade.level);
        dt=16;
    }
    if(dt>255u)dt=255u;
    unsigned lane=0;
    for(unsigned k=0;k<CF_NKEYS;k++){
        unsigned on=(cf_sn.studio_keys>>k)&1u;
        if(part==CGS_DRUMS){
            on=0;
            if(cf_key_is_upper(k) && lane<SQ_NLANE){
                unsigned hit=cf_sn.sq_lane_t[lane++];
                on=hit && cf_sn.now_ms-hit<80u;
            }
        }
        int step=on?(int)dt*3:-(int)dt;
        cgu_key_fade.level[k]=(uint8_t)clampi(cgu_key_fade.level[k]+step,0,255);
    }
}

/* PLAY has a second physical emitter, outside the button's orange matrix cell. */
#ifndef LED_PLAY_GREEN
#define LED_PLAY_GREEN ((8u << 3) | 1u)
#endif
static void cf_leds(void)
{
    uint8_t lit[FM1_NCOL] = {0}, mid[FM1_NCOL] = {0}, dim[FM1_NCOL] = {0}, faint[FM1_NCOL] = {0};
    static const uint8_t SCR_BTN[SCR_N] = {B_HOME, B_SCL, B_ENV, B_LFO, B_EDIT, B_GLO, B_FX, B_ARP, B_SEQ, B_SAVE,
                                           B_SEQ, B_SEQ, B_SEQ, B_SEQ, B_EDIT, B_SCL, B_SAVE};
    const cf_chord_out_t *out = &cf_sn.out;
    uint32_t k, blink = (cu.now / 250u) & 1u, sq = sq_screen(cu.scr), rm = cu.scr == SCR_DRUM;
    uint16_t auto_notes = 0;
    uint16_t guide_playing, guide_next = cf_progression_cue(&guide_playing);
    /* Only roots guide the hand: generated thirds/fifths must not hide the next root. */
    if(out->automatic && !out->fallback && out->sounding && !out->single)auto_notes=(uint16_t)(1u<<out->root_pc);
    auto_notes|=guide_playing;
    /* buttons: the open screen lit, the others dim; PLAY with the beat. The sequencer: only what works there is up
     * (HOME, SEQ, GLO here; EDIT, SAVE, SEL, PLAY, REC below); the other screens' buttons do nothing there, dark */
    for (k = 0; k < SCR_N; k++)                  /* (the rhythm machine: as the sequencer, cf_dmui.c) */
        if ((!sq && !rm) || SCR_BTN[k] == B_HOME || SCR_BTN[k] == B_SEQ || SCR_BTN[k] == B_GLO)
            cf_led(k == cu.scr ? lit : dim, panel.btn[SCR_BTN[k]], 1);
    if (rm && dm_tweaked(dm_cur()))              /* (SEL: the rhythm's own KIT and SWING back, when it has the user's) */
        cf_led(dim, panel.btn[B_SCL], 1);
    if (rm)                                      /* (EDIT: the rhythm as a new pattern; no empty letter: faint) */
        cf_led(sq_home_free() >= 0 ? mid : faint, panel.btn[B_EDIT], 1);
    if (sq) {                                    /* the sequencer: what works here (sq_key_led's note) */
        const sq_pat_t *p = sq_cur();
        int has = squ.pat == SQ_NPAT ? 1 : !sq_empty(p) && (p->nev || sq_drums(p));
        if (cu.scr == SCR_SQ && squ.pat == SQ_NPAT) {   /* the song: EDIT and SEL work on the frame's pattern */
            has = sq_st.song.n != 0;
            cf_led(has ? mid : faint, panel.btn[B_EDIT], 1);
            cf_led(has ? mid : faint, panel.btn[B_SCL], 1);
        } else if (cu.scr == SCR_SQ) {           /* (SEL held clears the track: up when it has something) */
            cf_led(blink ? mid : dim, panel.btn[B_EDIT], 1);
            if (sq_track_has(p, squ.trk))
                cf_led(dim, panel.btn[B_SCL], 1);
        } else {
            int clear = squ.trk != SQ_DRUMS && !squ.nofr && sq_at(p, squ.trk, squ.bar, squ.step) >= 0;
            /* (DRUMS: SEL nothing) */
            cf_led(mid, panel.btn[B_SAVE], 1);
            if (squ.mv || clear)                 /* (EDIT held + SELECT moves the frame's chord: up on a chord) */
                cf_led(squ.mv ? lit : mid, panel.btn[B_EDIT], 1);
            if (squ.trk != SQ_DRUMS)
                cf_led(clear ? mid : faint, panel.btn[B_SCL], 1);
            if (squ.trk == SQ_CHORDS || squ.trk == SQ_LAYER)   /* (ARP: the track's own, lit while on) */
                cf_led((p->arp >> squ.trk) & 1u ? lit : dim, panel.btn[B_ARP], 1);
        }
        if (cf_sn.sq_rec != 0xFFu)               /* REC: a take lit, its count-in blinking; on a track: ready (dim) */
            cf_led(cf_sn.sq_cin ? (blink ? lit : faint) : lit, panel.btn[B_REC], 1);
        else if (squ.pat < SQ_NPAT)
            cf_led(dim, panel.btn[B_REC], 1);
    } else {
        if (sqk.on)                              /* REC: a take from here lit, its count-in blinking (as in a track) */
            cf_led(cf_sn.sq_cin || !sqk.seen ? (blink ? lit : faint) : lit, panel.btn[B_REC], 1);
        else
            cf_led(dim, panel.btn[B_REC], 1);
        if (cf_sn.sq_on)                         /* the sequencer plays elsewhere: SEQ up, as ARP is */
            cf_led(mid, panel.btn[B_SEQ], 1);
    }
    /* One truthful transport color on every screen. Never mix the orange and
     * green emitters; clock wait/loss pulses orange rather than implying play. */
    int waiting=cf_sn.ac_on && (cf_sn.ac_waiting || cf_sn.ac_lost);
    int playing=cf_sn.running && !(cf_sn.ac_on && (cf_sn.ac_paused || waiting));
    if(playing)lit[LED_PLAY_GREEN>>3]|=(uint8_t)(1u<<(LED_PLAY_GREEN&7u));
    else cf_led(waiting && !blink ? faint : lit,panel.btn[B_PLAY],1);
    if (cgu_glow.active)cf_led(lit,panel.btn[B_GLO],1);
    if ((cgu_play_part()>=0 ? cgu_arp_mode()!=CGU_ARP_OFF : cu.arp_on) && !sq && !rm)                        /* the arpeggiator plays: ARP lit wherever it opens (not on the
                                                  * sequencer: it does nothing there) */
        cf_led(lit, panel.btn[B_ARP], 1);
    cf_led(cs.chord.octave < 0 ? (cs.chord.octave < -1 ? lit : mid) : faint, panel.btn[B_OCTDN], 1);
    cf_led(cs.chord.octave > 0 ? (cs.chord.octave > 1 ? lit : mid) : faint, panel.btn[B_OCTUP], 1);
    /* keys: held and Auto chord tones lit; effective chord buttons lit, the others faint;
     * the chord's root mid otherwise; the scale dim */
    int studio=cgu_play_part();
    cgu_led_update(studio);
    for (k = 0; k < CF_NKEYS; k++) {
        int idx, role = cf_key_role(cs.chord.algo, (int)k, &idx), id = (int)(14u + k);
        if(cgu_glow.active){
            int black=cf_key_is_upper(k);
            unsigned place=cgu_glow_place(k,black);
            if(black){
                if(place<CGS_NPART){
                    unsigned bit=1u<<cgu_part(place);
                    cf_led(cgs_mute_mask()&bit?faint:lit,(uint32_t)id,1);
                } else if(place>=6u && place<10u)
                    cf_led(cgs.drum_mutes&(1u<<(place-6u))?faint:lit,(uint32_t)id,1);
            } else if(cgu_glow_ready(place))
                cf_led(cgu_glow.keys&(1u<<k)?lit:mid,(uint32_t)id,1);
            continue;
        }
        if(studio>=0){
            unsigned level=cgu_key_fade.level[k];
            if(level)cf_led(level>=192u?lit:level>=112u?mid:level>=48u?dim:faint,(uint32_t)id,1);
            else if(studio!=CGS_DRUMS && role==CF_ROLE_BUTTON &&
                    (cf_is_off(&cs.chord,idx)?out->hold:(out->buttons>>idx)&1u))
                cf_led(dim,(uint32_t)id,1);
            continue;
        }
        if (sq_key_led(k, lit, mid, dim, faint))
            continue;
        int next_root=role==CF_ROLE_ROOT && ((guide_next>>(cf_key_root_note(&cs.chord,(int)k)%12))&1u);
        if (((cu.keys >> k) & 1u) && !next_root) {
            cf_led(lit, (uint32_t)id, 1);
            continue;
        }
        if (cu.scr == SCR_EDIT && cf_key_is_upper((int)k)) {
            int u = 0, j;
            for (j = 0; j < (int)k; j++)
                u += cf_key_is_upper(j);
            if (u == cu.edit_sec || (u >= 8 && u <= 10 && edit_key_part(u) == cu.edit_part))
                cf_led(lit, (uint32_t)id, 1);
            else if (u < 11)
                cf_led(blink ? dim : faint, (uint32_t)id, 1);
            continue;
        }
        if (role == CF_ROLE_BUTTON) {
            if (cf_is_off(&cs.chord, idx) ? out->hold : (out->buttons >> idx) & 1u)   /* (HOLD: lit while on, as on the
                                                                                       * screen) */
                cf_led(lit, (uint32_t)id, 1);
            else if (cu.scr == SCR_SEL && idx == cu.sel_btn)
                cf_led(blink ? mid : dim, (uint32_t)id, 1);
            else
                cf_led(faint, (uint32_t)id, 1);
        } else if (role == CF_ROLE_ROOT) {
            int pc = cf_key_root_note(&cs.chord, (int)k) % 12;
            if ((guide_next >> pc) & 1u) {
                /* Next wins over current/held, including repeated roots. */
                if((cu.now/250u)&1u)cf_led(lit,(uint32_t)id,1);
                else if((auto_notes>>pc)&1u || ((cu.keys>>k)&1u))cf_led(dim,(uint32_t)id,1);
            } else if ((auto_notes >> pc) & 1u)
                cf_led(lit, (uint32_t)id, 1);
            else if (out->sounding && pc == out->root_pc)
                cf_led(mid, (uint32_t)id, 1);
            else if (cs.lights && cs.chord.quantize != CF_Q_OFF && pc == cs.chord.tonic)
                cf_led(dim, (uint32_t)id, 1);
        }
    }
    for (k = 0; k < FM1_NCOL; k++) {
        fm1_led_faint[k] = faint[k];
        fm1_led_dim[k] = dim[k];
        fm1_led_dim2[k] = mid[k];
        fm1_led[k] = lit[k];
    }
}

/* the GLIDE of 0.4.x / 0.5-rc1, one setting for every sound: HOME's chord sound takes it, as an edit of it, the
 * first time it is the one playing (not a pattern's own: 0.5, each sound its own, cf_play.c cfp_glide_g); the setting
 * goes to 0 for good. Every frame until then (cheap) */
static void cf_glide_migrate(void)
{
    uint32_t want, g = 0;
    if (!cs.glide_ms || cfk_own[CF_PART_CHORD] != 0u || cf_stage[CF_PART_CHORD].ready)
        return;
    want = (uint32_t)cs.glide_ms * FS / (1000u * CTL);
    while (g < 127u && 1u + g * g / 8u < want)
        g++;
    if (!trk[CF_PART_CHORD].p[P_GLIDE] && g) {
        fm1_irq_off();
        trk[CF_PART_CHORD].p[P_GLIDE] = (int16_t)g;
        fm1_irq_on();
        cf_edited(CF_PART_CHORD);
    }
    cs.glide_ms = 0;
}

/* ------------------------------------------------------------ the plant --- */
#if CF_SCENES
static uint32_t cu_last_serial;
static uint8_t cu_last_onset, cu_last_arp, cu_last_src;
/* a sequencer chord's flower quality from its pitch classes over the root (the keys' come from their type) */
static uint8_t cf_fl_quality(uint32_t mask, int single)
{
    if (single)
        return FL_Q_SINGLE;
    if (mask & 1u << 4)
        return (mask & 1u << 8) && !(mask & 1u << 7) ? FL_Q_AUG : FL_Q_MAJOR;
    if (mask & 1u << 3)
        return (mask & 1u << 6) && !(mask & 1u << 7) ? FL_Q_DIM : FL_Q_MINOR;
    return mask & 1u << 5 ? FL_Q_SUS4 : mask & 1u << 2 ? FL_Q_SUS2 : FL_Q_MAJOR;
}
static void cf_feed_plant(uint32_t dt)
{
    const cf_chord_out_t *o = &cf_sn.out;
    uint32_t i, lvl = 0, src, ser;
    if (!cf_scene_animated()) {
        /* No animation tick, geometry rebuild, note particles or voice scans.
         * Resync the current chord on return, without replaying old onsets. */
        cu_last_src = 0xFFu;
        cu_last_onset = cf_sn.onset;
        cu_last_arp = cf_sn.arp_steps;
        return;
    }
    if (cu_last_src == 0xFFu) {
        memset(flo.pulse_on, 0, sizeof flo.pulse_on);
        flo.level = flo.level_in = 0;
    }
    /* Scene selection is independent of the fixed Top Lock playing mode. */
    flora_set_species(&flo, cf_scene() == CF_SCENE_BASS ? FL_SP_CORAL : FL_SP_PLANT);
    flora_set_key(&flo, cs.chord.tonic);
    flo.backdrop = cs.backdrop;
    /* what the flower shows: the keys' chord while they sound, else the sequencer's CHORDS track's while it plays,
     * as if played by hand (0.5, the owner: "то, что играется в секвенсоре я бы хотел, чтобы на главной
     * отображалось на цветочке"), else nothing */
    src = o->sounding && o->n ? 1u : (cf_sn.sq_on || cf_sn.ac_on) && cf_sn.sq_dn ? 2u : 0u;
    ser = src == 2u ? cf_sn.sq_dser : o->serial;
    if (src != cu_last_src || ser != cu_last_serial) {
        fl_chord_t c;
        int strike = src == 2u && (ser != cu_last_serial || cu_last_src != 2u);
        cu_last_src = (uint8_t)src;
        cu_last_serial = ser;
        if (src == 1u) {
            static const uint8_t QT[CF_NTYPES] = {FL_Q_SINGLE, FL_Q_MAJOR, FL_Q_MINOR, FL_Q_DIM, FL_Q_AUG,
                                                   FL_Q_SUS2, FL_Q_SUS4, FL_Q_MAJOR};
            c.n = (uint8_t)(o->n > 8 ? 8 : o->n);
            for (i = 0; i < c.n; i++)
                c.note[i] = o->note[i];
            c.root = o->root_pc;
            c.quality = o->single ? FL_Q_SINGLE : QT[o->type < CF_NTYPES ? o->type : 1];
            c.bass = (int8_t)(cf_sn.nb ? cf_sn.bass[cf_sn.nb - 1] : -1);
            flora_set_chord(&flo, &c);
        } else if (src == 2u) {
            c.n = cf_sn.sq_dn;
            for (i = 0; i < c.n; i++)
                c.note[i] = cf_sn.sq_dnote[i];
            c.root = (uint8_t)(cf_sn.sq_droot % 12u);
            c.quality = cf_fl_quality(cf_sn.sq_dmask, cf_sn.sq_dsingle);
            c.bass = (int8_t)(cf_sn.sq_bn ? cf_sn.sq_bass[cf_sn.sq_bn - 1] : -1);
            flora_set_chord(&flo, &c);
            if (strike)                          /* (each chord the sequencer strikes sparkles, as the keys' onset) */
                for (i = 0; i < c.n; i++)
                    flora_note_on(&flo, c.note[i], cs.vel);
        } else {
            c.n = 0;
            flora_set_chord(&flo, &c);
        }
    }
    if (cf_sn.onset != cu_last_onset) {
        cu_last_onset = cf_sn.onset;
        if (!cf_sn.arp_on)                       /* (the arpeggiator: a pulse a note, below) */
            for (i = 0; i < cf_sn.n; i++)
                flora_note_on(&flo, cf_sn.note[i], cs.vel);
    }
    if (cf_sn.arp_steps != cu_last_arp) {        /* each note the arpeggiator plays sparkles out of its branch */
        cu_last_arp = cf_sn.arp_steps;
        if (cf_sn.arp_on)
            flora_note_on(&flo, cf_sn.arp_note, cs.vel);
    }
    for (i = 0; i < NVOICE; i++) {               /* loudness: the chord part's strongest voice */
        int32_t e = trk[CF_PART_CHORD].v[i].active ? trk[CF_PART_CHORD].v[i].env_out : 0;
        if (e < 0)
            e = -e;
        if ((uint32_t)e > lvl)
            lvl = (uint32_t)e;
    }
    flora_set_level(&flo, (uint16_t)(lvl > 32767u ? 65535u : lvl * 2u));
    flora_tick(&flo, dt);
}

#else
static void cf_feed_plant(uint32_t dt) { (void)dt; }
#endif

/* the sound editor's part (fm6_store.c: a DX7 voice received goes there), -1 when it is not open */
static int cf_editing_part(void) { return cu.scr == SCR_EDIT ? (int)cu.edit_part : -1; }
static void cf_edited(uint32_t part)
{
    if (part < 3u) {
        cu.edited[part] = 1;
        cfk_on[part] = 1;                        /* (kept over a power-off: cf_settings.c) */
        cfk_gen[part] = cf_snd_gen[part];
    }
}

/* ------------------------------------------------------------------ init --- */
static void cf_ui_init(void)
{
    cf_sound_browse_defaults();cge_init();
    if(CF_STUDIO){
        cgs_init((uint32_t)fm1_ticks());
        cgu_home();
        cgu.topic=0;
        cgu.zoom_ms=0;
        cgu.action=0;
    }
    cpg.idea = CF_P_NONE; cpg.repeat_off=0; cpg.click_generation=cpa.click_generation;
    cpb_load();
    if (!cfs_loaded)                             /* (cf_splash read the settings record: they stay) */
        cs_defaults(&cs);
    cfd_load();
    csl_load();
    cvr_init();
    home_boot_main = !CF_SCENES;
    cf_home_bank = CF_SCENES?CF_HB_CORE:CF_HB_ENGINE; cf_home_bank_ms = 0;
    cu.scr = SCR_HOME;
    cu.sel_btn = sel_first();
    cf_led_init();
    cf_palette_build();
    cu_spell_update();
    cf_out_init();
    if (!cr_safe) {
        sq_load();                               /* three recordings; legacy store stays archived */
        cfps_load();                             /* .. their own sounds (0.5) */
        cfk_load();                              /* .. the sounds' tweaks (put on them as they load: cfk_take) */
        sq_from_loop();                          /* (a loop kept by 0.3.1 or before, or restored: a pattern now) */
        dm_load();                               /* (the drum machine's settings: in the loop's object, after it) */
        sq_resume();                             /* (what PLAY on HOME played before the power-off) */
    } else {
        sq_defaults();
    }
    if (cfs_loaded) {                            /* the sounds the parts had before the power-off */
        cf_snd_load_index(CF_PART_CHORD, cs.snd[0]);
        cf_snd_load_index(CF_PART_BASS, cs.snd[1]);
    } else {                                     /* a new unit: Signature 8 chords, a round bass */
        if (!cf_snd_find(CF_PART_CHORD, ENGI_FM6, "EMBER KEYS"))
            cf_snd_load_index(CF_PART_CHORD, 1);
#if FELUCCA_VA
        if (!cf_snd_find(CF_PART_BASS, ENGI_VA, "DEEP SUB"))
#endif
            cf_snd_load_index(CF_PART_BASS, 0);
    }
    cs.snd[0] = (uint16_t)cf_snd_pos(CF_PART_CHORD);
    cs.snd[1] = (uint16_t)cf_snd_pos(CF_PART_BASS);
    if (cs.snd_layer != 0xFFFFu)                 /* saved Layer identity; fresh settings use GHOST CHOIR */
        cf_snd_load_index(CF_PART_LAYER, cs.snd_layer);
#if FELUCCA_VA
    else if (!cf_snd_find(CF_PART_LAYER, ENGI_VA, "GHOST CHOIR"))
#else
    else
#endif
        cf_snd_load_index(CF_PART_LAYER, 2);
    cs.snd_layer = (uint16_t)cf_snd_pos(CF_PART_LAYER);
    cf_drums_init();
    sq_home_poll();                              /* (the pattern PLAY plays, if none was kept) */
    sq_snd_force = 1;
    sq_snd_poll();                               /* (.. its own sounds, every sound's tweak on it: 0.5) */
    cf_glide_migrate();
    cu.env_tgt = cs.env_link ? 0u : 1u;
    cu.arp_on = cs.arp_keep;                     /* (the arpeggiator as it was at the power-off: cf_settings.c) */
    if (cs.env_link)
        cf_env_follow();
    cf_click_sound = cs.click;
    cf_send_config();
    cf_send_metro();
#if CF_SCENES
    if (!flo.nseg)
        flora_init(&flo, 0xC0A1F10Eu, FLW / 2, FLH + 1);   /* (the stem comes up from the bottom edge) */
#else
    if(CF_STUDIO && !cr_safe)cgs_prepare_start();
    /* Start the visible intro after sound/store loading, not before hardware initialization. */
    cf_rakt_begin();
    cf_boot_pending = cs.boot_sound && !cr_safe;
    cf_boot_ready_ms = fm1_ms; /* let startup config/overload requests settle */
#endif
    cu.idle_ms = 0x7FFFFFFF;                     /* (the intro runs until the first touch) */
    fb_full = 1;                                 /* (main.c cleared the panel after the splash) */
    cu.last_ms = fm1_ms;
    cpg.rhythm = cpa_compatible(dm_cur(), cs.beats);
    cu.booted = 1;
}

static void cf_ui_frame(void)                    /* ui_leds: the snapshot, the plant, the lights */
{
    uint32_t now = fm1_ms, dt = now - cu.last_ms;
    cu.last_ms = now;
    cu.frame_dt = dt;
    cu.now = now;
    cu.idle_ms += dt;
    if(cf_boot_pending && now-cf_boot_ready_ms>=180u) {
        cf_boot_pending=0;
        if(cs.boot_sound && !cr_safe && cu.idle_ms>=now-cf_boot_ready_ms)cf_boot_go=1;
    }
    cu.frames++;
    cf_diag_poll(now);
    cf_snapshot();
    cpa_ui_poll();cge_poll();
    if(CF_STUDIO)cgs_poll();
    cfd_ui_poll();
    cfl_poll();
    sq_tempo_poll();
    cf_feed_plant(dt);
    cf_leds();
    if (cu.pop && (int32_t)(cu.now - cu.pop_until) > 0)
        cu.pop = POP_NONE;
}
