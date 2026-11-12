/* SPDX-License-Identifier: GPL-3.0-only
 * FX readout: ALGORITHM selects Macros / Individual. SELECT browses factory
 * recipes on Macros (SEL recalls), or selects an effect on Individual.
 * Macro anchors are foreground-only; the actual resulting chain is persisted
 * in the existing settings record. View/browse changes never touch audio.
 * Included by cf_ui.c after its helpers. */

static const char *const CFX_NAME[CFX_N] = {"Filter", "Reverb", "Delay", "Distortion", "Chorus", "Phaser"};
static const char *const CFX_SHORT[CFX_N] = {"FLT", "REV", "DLY", "DST", "CHO", "PHS"};
static const char *const CFX_KNOB[CFX_N][3] = {
    {"TYPE", "CUTOFF", "RESO"}, {"TYPE", "DECAY", "MIX"}, {"TIME", "FDBK", "MIX"},
    {"DRIVE", "TONE", "LEVEL"}, {"RATE", "DEPTH", "MIX"}, {"RATE", "DEPTH", "MIX"}};
static const char *const CFX_FTYPE[3] = {"Low pass", "Band pass", "High pass"};
static const char *const CFX_FTYPE_S[3] = {"LPF", "BPF", "HPF"};   /* (a knob cell holds 7 characters) */
static const char *const CFX_RTYPE[3] = {"Room", "Hall", "Plate"};


enum { CFX_MACROS, CFX_INDIVIDUAL, CFX_CUSTOM = 255 };
static struct {
    cf_fx_t anchor, seen;
    uint8_t ready, view, browse, loaded, edited;
    uint8_t value[4], at[4];
    uint16_t level;
    uint32_t vis_ms;
} cfxu;
static const char *const CFX_MACRO_NAME[4] = {"1 Space", "2 Echo", "3 Motion", "4 Warmth"};

static int cfx_equal(const cf_fx_t *a, const cf_fx_t *b) { return !memcmp(a, b, sizeof *a); }
static uint8_t cfx_identify(const cf_fx_t *s)
{
    for (uint32_t i = 0; i < FXP_FACTORY_N; i++)
        if (cfx_equal(s, &FXP_FACTORY[i].fx)) return (uint8_t)i;
    for (uint32_t i = 0; i < FXP_SLOTS; i++)
        if ((fxp_bank.used & (1u << i)) && cfx_equal(s, &fxp_bank.slot[i].fx)) return (uint8_t)(FXP_FACTORY_N + i);
    return CFX_CUSTOM;
}
static void cfx_anchor(void)
{
    const cf_fx_t *fx = fxp_active_chain();
    cfxu.anchor = cfxu.seen = *fx;
    uint32_t on = fx->on;
    cfxu.value[0] = on & (1u << CFX_REVERB) ? (uint8_t)clampi((fx->v[CFX_REVERB][1] + fx->v[CFX_REVERB][2]) / 2, 1, 99) : 0;
    cfxu.value[1] = on & (1u << CFX_DELAY) ? (uint8_t)clampi((fx->v[CFX_DELAY][1] + fx->v[CFX_DELAY][2]) / 2, 1, 99) : 0;
    uint32_t motion = 0;
    for (uint32_t e = CFX_CHORUS; e <= CFX_PHASER; e++)
        if (on & (1u << e)) {
            uint32_t v = (fx->v[e][1] + fx->v[e][2]) / 2u;
            if (v > motion) motion = v;
        }
    cfxu.value[2] = (uint8_t)(on & ((1u << CFX_CHORUS) | (1u << CFX_PHASER)) ? clampi((int)motion, 1, 99) : 0);
    cfxu.value[3] = on & ((1u << CFX_FILTER) | (1u << CFX_DIST)) ? 50 : 0;
    memcpy(cfxu.at, cfxu.value, sizeof cfxu.at);
}
static void cfx_init(void)
{
    memset(&cfxu, 0, sizeof cfxu);
    cfxu.ready = 1;
    cfxu.loaded = cfx_identify(fxp_active_chain());
    cfxu.browse = cfxu.loaded == CFX_CUSTOM ? 0 : cfxu.loaded; fxpu.pick = cfxu.browse;
    cfx_anchor();
}
static void cfx_sync(void)
{
    if (!cfxu.ready) cfx_init();
    else if (!cfx_equal(fxp_active_chain(), &cfxu.seen)) {
        uint8_t exact = cfx_identify(fxp_active_chain());
        if (exact != CFX_CUSTOM) cfxu.loaded = exact;
        else cfxu.loaded = CFX_CUSTOM;
        cfxu.edited = 0;
        cfx_anchor();
    }
    if (cfxu.loaded != CFX_CUSTOM && cfxu.loaded >= FXP_FACTORY_N &&
        !(fxp_bank.used & (1u << (cfxu.loaded - FXP_FACTORY_N)))) {
        /* Deleting the loaded user record keeps its audible RAM copy. */
        cfxu.loaded = CFX_CUSTOM;
    }
}
static void cfx_scope(unsigned scope)
{
    if (scope == fxp_scope) return;
    fxp_context[fxp_scope].pick = (uint8_t)(fxpu.pick + 1u);
    fxp_context[fxp_scope].prev = fxpu.prev;
    fxp_context[fxp_scope].previous = fxpu.previous;
    fxp_context[fxp_scope].bypass = fxpu.prev_bypass;
    fxp_scope = scope;
    cfx_init();
    if (fxp_context[scope].pick) fxpu.pick = fxp_context[scope].pick - 1u;
    fxpu.prev = fxp_context[scope].prev;
    fxpu.previous = fxp_context[scope].previous;
    fxpu.prev_bypass = fxp_context[scope].bypass;
}

static void cfx_recall(void)
{
    cfx_sync();
    if (!fxp_pick()) { fxp_recall(1); return; }
    fxp_recall(0);
    cfxu.loaded = fxpu.pick;
    cfxu.edited = 0;
    cfx_anchor();
}
static void cfx_select(int d)
{
    cfx_sync();
    if (fxpu.mode == FXP_MANAGE) { cu.save_hold = 0; fxpu.slot = (uint8_t)clampi(fxpu.slot + d, 0, FXP_SLOTS - 1); }
    else if (fxpu.mode == FXP_BROWSE) { fxp_select(d); cfxu.browse = fxpu.pick; }
    else cu.fx_pos = (uint8_t)clampi(cu.fx_pos + d, 0, CFX_N - 1);
}
static void cfx_view(int d)
{
    cfx_sync();
    if (d) { cfxu.view = d > 0 ? CFX_INDIVIDUAL : CFX_MACROS; fxpu.mode = cfxu.view == CFX_MACROS ? FXP_BROWSE : FXP_EDIT; cu.save_hold = 0; }
}

/* A bounded curve passes through the actual hand-edited value at its anchor.
 * The groups own separate effects; no stale recipe overwrite on the next turn. */
static uint8_t cfx_curve(int value, int at, int base, int lo, int hi)
{
    if (value == at) return (uint8_t)base;
    if (value < at) return (uint8_t)(lo + (base - lo) * value / at);
    return (uint8_t)(base + (hi - base) * (value - at) / (100 - at));
}
static void cfx_switch(cf_fx_t *fx, uint32_t mask, int on)
{
    fx->on = (uint8_t)(on ? fx->on | mask : fx->on & ~mask);
}
static void cfx_macro_turn(int k, int d)
{
    cfx_sync();
    int m = clampi(cfxu.value[k] + d, 0, 100), at = cfxu.at[k];
    if (m == cfxu.value[k]) return;
    cf_fx_t fx = *fxp_active_chain();
    const cf_fx_t *a = &cfxu.anchor;
    uint8_t *v;
    if (k <= 1) {
        uint32_t e = k == 0 ? CFX_REVERB : CFX_DELAY;
        v = fx.v[e];
        int enabled = (a->on >> e) & 1u;
        /* Types and repeat timing remain constant throughout a macro gesture. */
        v[1] = cfx_curve(m, at, enabled ? a->v[e][1] : 0, 0, e == CFX_DELAY ? 85 : 100);
        v[2] = cfx_curve(m, at, enabled ? a->v[e][2] : 0, 0, e == CFX_DELAY ? 75 : 90);
        cfx_switch(&fx, 1u << e, m > 0);
    } else if (k == 2) {
        uint32_t mask = a->on & ((1u << CFX_CHORUS) | (1u << CFX_PHASER));
        if (!mask) mask = 1u << CFX_CHORUS;
        for (uint32_t e = CFX_CHORUS; e <= CFX_PHASER; e++) if (mask & (1u << e)) {
            v = fx.v[e];
            int enabled = (a->on >> e) & 1u;
            v[1] = cfx_curve(m, at, enabled ? a->v[e][1] : 0, 0, 100);
            v[2] = cfx_curve(m, at, enabled ? a->v[e][2] : 0, 0, 100);
        }
        cfx_switch(&fx, mask, m > 0);
    } else {
        const uint8_t *f = a->v[CFX_FILTER], *ds = a->v[CFX_DIST];
        int filter = (a->on >> CFX_FILTER) & 1u, drive = (a->on >> CFX_DIST) & 1u;
        v = fx.v[CFX_FILTER];
        v[0] = filter ? f[0] : 0;
        int base = filter ? f[1] : 99;
        int low = v[0] == 2 ? 0 : 99;
        int high = v[0] == 0 ? base / 2 : clampi(base - 12, 0, 99);
        v[1] = cfx_curve(m, at, base, low, high);
        v = fx.v[CFX_DIST];
        int old = drive ? ds[0] : 0;
        v[0] = cfx_curve(m, at, old, 0, 100);
        v[1] = cfx_curve(m, at, drive ? ds[1] : 100, 100, 10);
        v[2] = (uint8_t)clampi((drive ? ds[2] : 70) * (100 + old) / (100 + v[0]), 0, 100);
        cfx_switch(&fx, (1u << CFX_FILTER) | (1u << CFX_DIST), m > 0);
    }
    if (m == at) {
        uint32_t mask = k == 0 ? 1u << CFX_REVERB : k == 1 ? 1u << CFX_DELAY :
                        k == 2 ? (1u << CFX_CHORUS) | (1u << CFX_PHASER) :
                                 (1u << CFX_FILTER) | (1u << CFX_DIST);
        for (uint32_t e = 0; e < CFX_N; e++) if (mask & (1u << e))
            memcpy(fx.v[e], a->v[e], 3);
        fx.on = (uint8_t)((fx.on & ~mask) | (a->on & mask));
    }
    cfxu.value[k] = (uint8_t)m;
    cfxu.edited = 1;
    cfxu.seen = fx;
    fxp_publish(&fx, 0, 0);
}

/* main already publishes post-MASTER activity, including tails and mute. */
static void cfx_visual_tick(uint32_t dt, uint32_t energy, int active)
{
    if (!cf_motion) { cfxu.level = 0; cfxu.vis_ms = 0; return; }
    if (cu.scr != SCR_FX || fxpu.mode == FXP_MANAGE) return;
    cfxu.level = (uint16_t)energy;
    if (active) cfxu.vis_ms += dt > 80u ? 80u : dt;
}

static uint32_t cfx_sel(void) { return fxp_active_chain()->order[cu.fx_pos % CFX_N] % CFX_N; }
static int cfx_is_on(uint32_t e) { return (int)((fxp_active_chain()->on >> e) & 1u); }

/* "1004 Hz", "2.5 kHz", "12 kHz": 7 characters at most */
static void cfx_hz(char *b, uint32_t hz)
{
    if (hz < 1000u) {
        kint(b, (int)hz, " Hz");
    } else if (hz >= 9950u) {
        kint(b, (int)((hz + 500u) / 1000u), " kHz");
    } else {
        fmt_fix(b, (int32_t)((hz + 50u) / 100u), 1);
        str_cpy(b + str_len(b), " kHz", 5);
    }
}
/* a slow rate: "0.35 Hz", "4.2 Hz" (x100 in) */
static void cfx_rate(char *b, uint32_t hz100)
{
    if (hz100 < 100u)                            /* (fmt_fix writes ".35") */
        b[0] = '0', fmt_fix(b + 1, (int32_t)hz100, 2);
    else if (hz100 < 1000u)
        fmt_fix(b, (int32_t)hz100, 2);
    else
        fmt_fix(b, (int32_t)(hz100 / 10u), 1);
    str_cpy(b + str_len(b), " Hz", 4);
}

static void cfx_knob_info(int knob, cf_knob_t *o)
{
    if (fxpu.mode == FXP_MANAGE) { fxp_knob_info(knob, o); return; }
    cfx_sync();
    cfxu.view = fxpu.mode == FXP_BROWSE ? CFX_MACROS : CFX_INDIVIDUAL;
    if (cfxu.view == CFX_MACROS) {
        kfmt(o->name, 12, CFX_MACRO_NAME[knob]);
        kint(o->val, cfxu.value[knob], "%");
        o->bar = (int16_t)(cfxu.value[knob] * 255 / 100);
        return;
    }
    uint32_t e = cfx_sel();
    const uint8_t *v = fxp_active_chain()->v[e];
    int k = knob - 1;                            /* (KNOB 1 the switch, KNOB 2-4 the three values) */
    if (knob == 0) {
        kfmt(o->name, 12, "1 ON/OFF");
        kfmt(o->val, 14, cfx_is_on(e) ? "On" : "Off");
        return;
    }
    o->name[0] = (char)('1' + knob); o->name[1] = ' ';
    kfmt(o->name + 2, 10, CFX_KNOB[e][k]);
    o->dis = !cfx_is_on(e);
    o->bar = (int16_t)(v[k] * 255 / 100);
    if (k == 0 && (e == CFX_FILTER || e == CFX_REVERB)) {
        kfmt(o->val, 14, (e == CFX_FILTER ? CFX_FTYPE_S : CFX_RTYPE)[v[0] > 2u ? 2u : v[0]]);
        o->bar = -1;
    } else if (e == CFX_FILTER && k == 1) {
        cfx_hz(o->val, CFX_HZ[v[1] > 99u ? 99u : v[1]]);
        o->bar = (int16_t)(v[1] * 255 / 99);
    } else if (e == CFX_DELAY && k == 0) {
        kint(o->val, (int)xf_ms(v[0] * 127u / 100u), " ms");
    } else if (e == CFX_CHORUS && k == 0) {
        cfx_rate(o->val, LFO_HZ_X100[(8u + v[0]) & 127u]);
    } else if (e == CFX_PHASER && k == 0) {
        cfx_rate(o->val, (uint32_t)(((uint64_t)CFX_PRATE[v[0] > 100u ? 100u : v[0]] * (FS / 32u) * 100u) >> 32));
    } else {
        kint(o->val, v[k], "%");
    }
}

static void cfx_knob_turn(int knob, int d)
{
    if (fxpu.mode == FXP_MANAGE) { fxp_knob_turn(knob, d); return; }
    cfx_sync();
    cfxu.view = fxpu.mode == FXP_BROWSE ? CFX_MACROS : CFX_INDIVIDUAL;
    if (!d) return;
    if (cfxu.view == CFX_MACROS) { cfx_macro_turn(knob, d); return; }
    cf_fx_t fx = *fxp_active_chain();
    uint32_t e = cfx_sel();
    uint8_t *v = fx.v[e];
    int k = knob - 1;
    if (knob == 0)
        fx.on = (uint8_t)(d > 0 ? fx.on | (1u << e) : fx.on & ~(1u << e));
    else if (k == 0 && (e == CFX_FILTER || e == CFX_REVERB))
        v[0] = (uint8_t)clampi(v[0] + (d > 0 ? 1 : -1), 0, 2);
    else
        v[k] = (uint8_t)clampi(v[k] + d, 0, e == CFX_FILTER && k == 1 ? 99 : 100);
    fxp_publish(&fx, 0, 0);
    cfxu.edited = 1;
    cfx_anchor();
}

static const char *cfx_loaded_name(void)
{
    if (cfxu.loaded == CFX_CUSTOM) return "Custom";
    if (cfxu.loaded < FXP_FACTORY_N) return FXP_FACTORY[cfxu.loaded].name;
    return fxp_bank.slot[cfxu.loaded - FXP_FACTORY_N].name;
}
static const char *const CFX_CAPTION[] = {
    "Dry reference", "Dark and rounded", "Forward grit", "Short double", "Stereo repeats",
    "Slow chorus", "Hollow sweep", "Narrow and boxy", "Bright reflections", "Long ambient wash",
    "Liquid ambience", "Dark handset", "Thin raspy handset"
};
