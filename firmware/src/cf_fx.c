/* SPDX-License-Identifier: GPL-3.0-only
 * CoralFlower: the effects, as the stock FM-1 has them: Filter, Reverb, Delay, Distortion, Chorus and Phaser in one
 * chain whose order the player sets, on the master bus (both parts, after they are mixed: fx.c's FX_MIX_HOOK, before
 * the master and its limiter). Each has an On/Off and three values 0..100, the stock set:
 *   Filter      TYPE (low pass, band pass, high pass), CUTOFF (100 Hz .. 20 kHz in 100 steps), RESONANCE
 *   Reverb      TYPE (room, hall, plate), DECAY, MIX
 *   Delay       TIME (16 .. 740 ms), FEEDBACK, MIX
 *   Distortion  DRIVE, TONE, LEVEL
 *   Chorus      RATE, DEPTH, MIX
 *   Phaser      RATE, DEPTH, MIX
 * The reverb, the delay and the chorus are ChoralRoot's (cr_fx.c: xr_run, xd_run, xc_block, their state and their
 * parameters xfp[]); here each is fed the bus in mono and its stereo wet goes back into the bus by MIX, in the chain's
 * order. The filter (a state-variable filter in the topology-preserving form, after Zavalishin's "The Art of VA Filter
 * Design"), the distortion (dsp.c's soft clip and a one-pole tone) and the phaser (six first-order all-passes a side
 * on a slow sine, the right side a quarter turn on) are new.
 * MIX: the reverb and the delay keep most of the dry sound (dry 1 - mix / 2, wet x mix); the chorus and the phaser
 * blend (dry 1 - mix / 2, wet mix / 2: at 100 the two halves, where their effect is strongest). An effect switched off
 * takes no new sound, but a reverb's or a delay's tail rings out.
 * Studio processes independent musician buses before summing them (cf_fx_studio.c).
 * Included after fx.c (cr_fx.c's functions), before cf_state.h (cf_set_t holds a cf_fx_t). */

#if CR_FX_DELAY_DS != 1 || CR_FX_REVERB_DS != 2
#error "cf_fx.c: the delay at the full rate, the reverb at half (cr_fx.c's defaults)"
#endif

#include "cf_fx_preset.c"

/* a new unit's chain (the owner's settings): the classic order, a short delay (75 ms, 35 %, mix 8 %) and a touch of
 * hall (decay 40 %, mix 5 %) on, the rest off */
static const cf_fx_t CFX_DEFAULT = {
    {CFX_FILTER, CFX_DIST, CFX_CHORUS, CFX_PHASER, CFX_DELAY, CFX_REVERB},
    (1u << CFX_REVERB) | (1u << CFX_DELAY),
    {{0, 99, 10}, {1, 40, 5}, {40, 35, 8}, {40, 50, 70}, {30, 45, 50}, {30, 60, 60}},
};

/* tables (python, in the comment of each): the filter's g = tan(pi fc / fs) in Q16 for 100 cutoffs 100 Hz .. 20 kHz
 * (log), and their Hz; its k = 1 / Q in Q14 for resonance 0..100 (Q 0.5 .. 12); the distortion tone's one-pole k in
 * Q15 for 700 Hz .. 16 kHz; the phaser's all-pass a = (t - 1) / (t + 1), t = tan(pi f / fs), in Q15 for 64 corners
 * 120 Hz .. 7 kHz; its LFO's phase step a block of 32 for 0.04 .. 6 Hz */
static const int32_t CFX_G[100] = {
    467, 493, 520, 548, 578, 610, 644, 679,
    716, 756, 797, 841, 887, 936, 988, 1042,
    1099, 1160, 1224, 1291, 1362, 1437, 1516, 1599,
    1687, 1780, 1878, 1981, 2090, 2205, 2326, 2454,
    2589, 2732, 2882, 3041, 3208, 3385, 3571, 3768,
    3976, 4195, 4426, 4670, 4928, 5200, 5487, 5791,
    6111, 6449, 6806, 7183, 7581, 8002, 8447, 8917,
    9413, 9939, 10494, 11082, 11703, 12361, 13058, 13797,
    14579, 15409, 16290, 17225, 18219, 19276, 20401, 21600,
    22879, 24246, 25708, 27275, 28958, 30768, 32720, 34832,
    37123, 39616, 42341, 45332, 48633, 52296, 56391, 61004,
    66249, 72279, 79301, 87608, 97624, 109989, 125716, 146515,
    175510, 219087, 292679, 445566,
};
/* (its Hz, for the screen) */
static const uint16_t CFX_HZ[100] = {
    100, 105, 111, 117, 124, 131, 138, 145, 153, 162,
    171, 180, 190, 201, 212, 223, 235, 248, 262, 276,
    292, 308, 325, 342, 361, 381, 402, 424, 448, 472,
    498, 525, 554, 585, 617, 651, 687, 724, 764, 806,
    851, 897, 947, 999, 1054, 1112, 1173, 1237, 1305, 1377,
    1453, 1532, 1617, 1706, 1799, 1898, 2003, 2113, 2229, 2351,
    2481, 2617, 2761, 2913, 3073, 3242, 3420, 3608, 3806, 4016,
    4236, 4469, 4715, 4974, 5248, 5536, 5840, 6162, 6500, 6858,
    7235, 7632, 8052, 8495, 8962, 9454, 9974, 10522, 11101, 11711,
    12355, 13034, 13751, 14507, 15304, 16146, 17033, 17970, 18958, 20000,
};
static const uint16_t CFX_K[101] = {
    32768, 31743, 30750, 29788, 28856, 27954, 27079, 26232, 25412, 24617,
    23847, 23101, 22378, 21678, 21000, 20343, 19707, 19090, 18493, 17915,
    17354, 16811, 16286, 15776, 15283, 14805, 14342, 13893, 13458, 13037,
    12630, 12234, 11852, 11481, 11122, 10774, 10437, 10110, 9794, 9488,
    9191, 8904, 8625, 8355, 8094, 7841, 7595, 7358, 7128, 6905,
    6689, 6480, 6277, 6080, 5890, 5706, 5528, 5355, 5187, 5025,
    4868, 4715, 4568, 4425, 4287, 4153, 4023, 3897, 3775, 3657,
    3542, 3432, 3324, 3220, 3120, 3022, 2927, 2836, 2747, 2661,
    2578, 2497, 2419, 2344, 2270, 2199, 2130, 2064, 1999, 1937,
    1876, 1817, 1761, 1706, 1652, 1600, 1550, 1502, 1455, 1409,
    1365,
};
static const uint16_t CFX_TONE[101] = {
    3110, 3204, 3301, 3400, 3502, 3607, 3715, 3826, 3940, 4057,
    4177, 4301, 4428, 4559, 4693, 4830, 4972, 5117, 5265, 5418,
    5575, 5736, 5900, 6069, 6243, 6420, 6602, 6789, 6980, 7175,
    7376, 7581, 7790, 8005, 8225, 8449, 8678, 8913, 9152, 9397,
    9647, 9902, 10162, 10427, 10697, 10973, 11253, 11539, 11830, 12126,
    12427, 12733, 13044, 13360, 13680, 14005, 14335, 14669, 15007, 15350,
    15696, 16046, 16400, 16757, 17117, 17481, 17847, 18215, 18586, 18959,
    19333, 19708, 20085, 20461, 20839, 21216, 21592, 21968, 22342, 22715,
    23086, 23454, 23819, 24181, 24538, 24892, 25241, 25585, 25923, 26256,
    26582, 26901, 27213, 27518, 27815, 28103, 28384, 28655, 28918, 29171,
    29415,
};
static const int16_t CFX_PA[64] = {
    -32212, -32176, -32137, -32095, -32051, -32003, -31953, -31899,
    -31842, -31782, -31717, -31648, -31575, -31497, -31413, -31325,
    -31231, -31131, -31025, -30912, -30792, -30664, -30529, -30385,
    -30232, -30069, -29897, -29714, -29521, -29315, -29097, -28866,
    -28622, -28363, -28090, -27800, -27494, -27171, -26829, -26469,
    -26088, -25687, -25265, -24820, -24352, -23859, -23341, -22797,
    -22226, -21627, -20999, -20341, -19652, -18931, -18176, -17387,
    -16563, -15701, -14802, -13862, -12880, -11854, -10783, -9662,
};
static const uint32_t CFX_PRATE[101] = {
    124661, 131067, 137801, 144882, 152326, 160153,
    168382, 177034, 186131, 195695, 205750, 216322,
    227438, 239124, 251411, 264329, 277911, 292191,
    307205, 322990, 339586, 357035, 375380, 394668,
    414947, 436269, 458685, 482254, 507034, 533087,
    560478, 589277, 619556, 651390, 684861, 720051,
    757049, 795949, 836847, 879846, 925055, 972587,
    1022562, 1075104, 1130346, 1188426, 1249491, 1313694,
    1381195, 1452165, 1526781, 1605232, 1687713, 1774433,
    1865609, 1961469, 2062255, 2168220, 2279629, 2396763,
    2519916, 2649396, 2785530, 2928659, 3079142, 3237357,
    3403702, 3578594, 3762472, 3955799, 4159059, 4372764,
    4597449, 4833680, 5082048, 5343179, 5617727, 5906382,
    6209869, 6528950, 6864426, 7217141, 7587978, 7977871,
    8387797, 8818786, 9271921, 9748339, 10249237, 10775873,
    11329569, 11911715, 12523773, 13167281, 13843854, 14555192,
    15303080, 16089397, 16916117, 17785316, 18699177,
};

static const uint16_t CFX_DRIVE[101] = {   /* the distortion's drive, Q8: 1 .. 40 times, exponential */
    256, 266, 276, 286, 297, 308, 319, 331, 344, 357,
    370, 384, 399, 414, 429, 445, 462, 479, 497, 516,
    535, 555, 576, 598, 620, 644, 668, 693, 719, 746,
    774, 803, 833, 865, 897, 931, 966, 1002, 1040, 1079,
    1120, 1162, 1205, 1251, 1298, 1346, 1397, 1449, 1504, 1560,
    1619, 1680, 1743, 1809, 1877, 1947, 2020, 2096, 2175, 2257,
    2341, 2429, 2521, 2615, 2714, 2816, 2921, 3031, 3145, 3263,
    3386, 3513, 3645, 3782, 3924, 4072, 4225, 4384, 4548, 4719,
    4897, 5081, 5271, 5470, 5675, 5888, 6110, 6339, 6577, 6825,
    7081, 7347, 7623, 7910, 8207, 8515, 8835, 9167, 9512, 9869,
    10240,
};

static volatile uint32_t cfx_quiet;  /* blocks since the chain's output was last above -54 dBFS: a settings save waits
                                      * for a second of it (cf_settings.c), so a flash erase (the audio stops ~45 ms
                                      * on the unit) never lands in a reverb's or a delay's tail */
static cf_fx_t cfx_set;              /* the chain as the ISR uses it (cf_fx_apply copies cs.fx in, IRQ off) */
static uint8_t cfx_ready;
static uint16_t cfx_space=32768; /* transient Engine wet multiplier; never written to settings */
static cf_fx_t cfx_target;
static volatile uint8_t cfx_pending, cfx_bypass;
static int32_t cfx_blend = 32768;  /* 0 dry, 32768 processed; a preset swaps only at dry */
static int32_t cfx_dry[2][CTL] __attribute__((section(".pool")));
/* 10 ms per leg at 44.1 kHz, one DSP chain throughout. */
#define CFX_FADE_STEP 75


typedef struct {
    uint32_t hash;                   /* of cfx_set: what the values below were worked out from */
    int32_t fg, fg_t, fa1, fa2, fa3, fk;   /* filter: g now and its target (Q16, gliding), the TPT's a1..a3 (Q15), k
                                            * (Q14) */
    int32_t fs[2][2];                /* .. each side's two integrators */
    int32_t dg, dk, dl, dlp[2];      /* distortion: drive (Q8), the tone's k (Q15), level (Q8); the tone's states */
    uint32_t pph, pinc;              /* phaser: the LFO's phase, its step a block */
    int32_t pdep, px[2][6], py[2][6], pfb[2];
    int32_t wg[CFX_N], wt[CFX_N];    /* each effect's wet gain now and set (Q15, eased a block at a time) */
} cfx_dsp_t;
static cfx_dsp_t cfx __attribute__((section(".pool")));
static int32_t cfx_m[CTL] __attribute__((section(".pool")));
static int32_t cfx_wl[CTL] __attribute__((section(".pool")));
static int32_t cfx_wr[CTL] __attribute__((section(".pool")));

#define CFX_MUL(a, b, sh) ((int32_t)(((int64_t)(a) * (b)) >> (sh)))

static uint32_t cfx_fnv(const void *p, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t h = 2166136261u;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}

/* the UI's chain -> the ISR's (after any change, and at power-on) */
static void cf_fx_apply(const cf_fx_t *s)
{
    fm1_irq_off();
    if (cfx_pending) cfx_target = *s;
    else cfx_set = *s;
    cfx_ready = 1;
    fm1_irq_on();
}

/* Preset changes are published atomically; the ISR performs the bounded fade.
 * Keep delay/reverb buffers alive. Rapid recalls replace only the pending target. */
static void cf_fx_recall(const cf_fx_t *s)
{
    fm1_irq_off();
    cfx_target = *s; cfx_pending = 1; cfx_ready = 1;
    fm1_irq_on();
}
static void cf_fx_bypass(int on)
{
    fm1_irq_off(); cfx_bypass = on != 0; fm1_irq_on();
}

static void cv_coefs(uint32_t type, uint32_t decay);   /* (the reverb, below) */

/* the values -> what each effect runs on (a change of the set: once) */
static void cfx_coefs(cfx_dsp_t *dsp, const cf_fx_t *s, int master)
{
    const uint8_t *f = s->v[CFX_FILTER], *rv = s->v[CFX_REVERB], *dl = s->v[CFX_DELAY], *ds = s->v[CFX_DIST];
    const uint8_t *ch = s->v[CFX_CHORUS], *ph = s->v[CFX_PHASER];
    uint32_t t = rv[0] > 2u ? 2u : rv[0], e;
    int32_t m;
    dsp->fg_t = CFX_G[f[1] > 99u ? 99u : f[1]];
    dsp->fk = CFX_K[f[2] > 100u ? 100u : f[2]];
    if (master) {
        cv_coefs(t, rv[1]);
        xfp[XP_DSYNC] = 0;                                  /* the stock delay's time is free, in ms */
        xfp[XP_DMODE] = 1;                                  /* ping-pong */
        xfp[XP_DTIME] = (int16_t)(dl[0] * 127u / 100u);
        xfp[XP_DFDBK] = (int16_t)(dl[1] * 118u / 100u);     /* (to 0.92: a long tail, never a runaway) */
        song.g[G_CRATE] = (int16_t)(8 + ch[0] * 100u / 100u);
        song.g[G_CDEPTH] = (int16_t)(ch[1] * 127u / 100u);
    }
    dsp->dg = CFX_DRIVE[ds[0] > 100u ? 100u : ds[0]];
    dsp->dk = CFX_TONE[ds[1] > 100u ? 100u : ds[1]];
    dsp->dl = (int32_t)ds[2] * 300 / 100;                /* level 0 .. 1.17 */
    dsp->pinc = CFX_PRATE[ph[0] > 100u ? 100u : ph[0]];
    dsp->pdep = (int32_t)ph[1] * 32 * 256 / 100;         /* the sweep: up to 32 of the 64 corners each way */
    for (e = 0; e < CFX_N; e++) {
        m = e == CFX_DIST || e == CFX_FILTER ? 0 : (int32_t)s->v[e][2];
        dsp->wt[e] = m * 32768 / 100;
    }
}

/* the filter's coefficients from g (Q16) and k (Q14): a1 = 1 / (1 + g (g + k)), a2 = g a1, a3 = g a2 */
static void cfx_filter_coefs(cfx_dsp_t *dsp)
{
    int32_t g = dsp->fg;
    uint32_t den = 65536u + (uint32_t)(((int64_t)g * (g + dsp->fk * 4)) >> 16);
    dsp->fa1 = (int32_t)((1u << 31) / den);
    dsp->fa2 = CFX_MUL(g, dsp->fa1, 16);
    dsp->fa3 = CFX_MUL(g, dsp->fa2, 16);
}

static void cfx_filter(cfx_dsp_t *dsp, unsigned type, int32_t *l, int32_t *r, uint32_t n)
{
    int32_t a1, a2, a3, k = dsp->fk;
    uint32_t c, i;
    if (dsp->fg != dsp->fg_t || !dsp->fa1) {               /* the cutoff glides: a quarter of the way a block */
        int32_t d = dsp->fg_t - dsp->fg;
        dsp->fg += d / 4 ? d / 4 : d;
        cfx_filter_coefs(dsp);
    }
    a1 = dsp->fa1;
    a2 = dsp->fa2;
    a3 = dsp->fa3;
    for (c = 0; c < 2u; c++) {
        int32_t *b = c ? r : l, s0 = dsp->fs[c][0], s1 = dsp->fs[c][1];
        for (i = 0; i < n; i++) {
            int32_t x = b[i], v3 = x - s1;
            int32_t v1 = (int32_t)(((int64_t)a1 * s0 + (int64_t)a2 * v3) >> 15);
            int32_t v2 = s1 + (int32_t)(((int64_t)a2 * s0 + (int64_t)a3 * v3) >> 15);
            s0 = 2 * v1 - s0;
            s1 = 2 * v2 - s1;
            b[i] = type == 0 ? v2 : type == 1 ? CFX_MUL(k, v1, 14) : x - CFX_MUL(k, v1, 14) - v2;
        }
        dsp->fs[c][0] = s0;
        dsp->fs[c][1] = s1;
    }
}

static void cfx_dist(cfx_dsp_t *dsp, int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t c, i;
    for (c = 0; c < 2u; c++) {
        int32_t *b = c ? r : l, lp = dsp->dlp[c];
        for (i = 0; i < n; i++) {
            int32_t x = CFX_MUL(b[i], dsp->dg, 8), y;
            y = softclip(x < -(1 << 20) ? -(1 << 20) : x > (1 << 20) ? (1 << 20) : x);
            lp += CFX_MUL(y - lp, dsp->dk, 15);
            b[i] = CFX_MUL(lp, dsp->dl, 8);
        }
        dsp->dlp[c] = lp;
    }
}

/* the wet back into the bus: dry x (1 - w / 2) + wet x w x boost, w the eased wet gain (Q15) */
static void cfx_mix(cfx_dsp_t *dsp, int32_t *l, int32_t *r, uint32_t e, uint32_t n, int32_t boost_q8, int blend)
{
    int32_t w = dsp->wg[e], dg, wg;
    uint32_t i;
    int32_t target=dsp->wt[e];
    if(e==CFX_REVERB || e==CFX_DELAY || e==CFX_CHORUS)target=CFX_MUL(target,cfx_space,15);
    if (w != target)
        dsp->wg[e] = w = w + (target - w) / 8 + (target > w ? 1 : target < w ? -1 : 0);
    dg = 32768 - w / 2;
    wg = CFX_MUL(blend ? w / 2 : w, boost_q8, 8);
    for (i = 0; i < n; i++) {
        l[i] = (int32_t)(((int64_t)l[i] * dg + (int64_t)cfx_wl[i] * wg) >> 15);
        r[i] = (int32_t)(((int64_t)r[i] * dg + (int64_t)cfx_wr[i] * wg) >> 15);
    }
}

/* the bus in mono for the send-style effects (feed 0: silence, the effect's tail only) and the wet buffers cleared */
static int32_t cfx_send(const int32_t *l, const int32_t *r, uint32_t n, int feed)
{
    int32_t any = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        cfx_m[i] = feed ? (l[i] + r[i]) >> 1 : 0;
        any |= cfx_m[i];
        cfx_wl[i] = cfx_wr[i] = 0;
    }
    return any;
}

/* cr_fx.c's coefficients follow xfp[] and the tempo (xf_block's own first part) */
static void cfx_xf_prepare(void)
{
    uint32_t i, h = xf_bpm;
    for (i = 0; i < XP_N; i++)
        h = h * 31u + (uint16_t)xfp[i];
    if (h == xf.hash && xf.on)
        return;
    xf.hash = h;
    xf_coefs();
    if (!xf.on) {                                       /* power-on: everything that glides starts where it is set */
        for (i = 0; i < 8u; i++) {
            xf.r_cur[i] = xf.r_tgt[i];
            xf.r_gc[i] = xf.r_g[i];
        }
        xf.d_cur[0] = xf.d_tgt[0];
        xf.d_cur[1] = xf.d_tgt[1];
        xf.d_fbc = xf.d_fb;
        xf.d_kc = xf.d_k;
        xf.r_kc = xf.r_k;
        xf.d_pm = xf.d_ping ? 256 : 0;
        xf.r_igc = xf.r_ig;
        xf.r_wc = xf.r_w;
        xf.r_pcur = xf.r_pre;
        xf.on = 1;
    }
}

static void cfx_delay(int32_t *l, int32_t *r, uint32_t n, int feed)
{
    if (!cfx_send(l, r, n, feed) && !xf.d_live)
        return;
    xf.d_ph[0] += (uint32_t)(((uint64_t)330u << 32) * CTL / (1000u * FS));   /* the wow (xf_block's) */
    xf.d_ph[1] += (uint32_t)(((uint64_t)3100u << 32) * CTL / (1000u * FS));
    xf.d_wt = ((osc_sine(xf.d_ph[0]) + (osc_sine(xf.d_ph[1]) >> 3)) * (xf.d_wow >> 3)) >> 12;
    XF_CALL(xd_run)(cfx_m, cfx_wl, cfx_wr, n, XF_IN);
    if (!xf.d_live)
        xf.d_lp[0] = xf.d_lp[1] = xf.d_hp[0] = xf.d_hp[1] = 0;
    cfx_mix(&cfx, l, r, CFX_DELAY, n, 768, 0);
}

/* ---- the reverb: Dattorro's plate (J. Dattorro, "Effect Design, Part 1: Reverberator and Other Filters", JAES
 * 1997), in integers, at 22.05 kHz between cr_fx.c's half-band filters (the wet's band ends near 10 kHz anyway). The
 * input: a pre-delay, a low cut at 200 Hz and a high cut at 8 kHz (two one-poles each), the bandwidth one-pole, four
 * diffusing all-passes; then the figure-eight tank, two halves each of a swinging all-pass, a delay, the damping
 * one-pole, the decay, an all-pass and a delay, each feeding the other; seven taps a side make the stereo out. FoMni
 * (GPL-3.0) runs the same figure in floats; this is written anew in fixed point. The type sets SIZE (every length and
 * tap scaled: a room is a smaller plate), the pre-delay, the damping and the bandwidth, and the span the DECAY value
 * covers. Lines int16 (the input a quarter of the bus: room for the tank's sums), states int32, gains Q15. */
#define CV_DS(n) ((n) * 22050 / 29761)            /* the paper's lengths (29761 Hz) at 22.05 kHz */
#define CV_PRE 1103                               /* the longest pre-delay: 50 ms */
#ifndef CV_EXC
#define CV_EXC 3                                  /* the swinging all-passes' excursion, samples (12 made the tail
                                                   * of a few pure tones swell and dip by up to 10 dB) */
#endif
enum { V_PRE, V_IN1, V_IN2, V_IN3, V_IN4, V_APL, V_D1L, V_AP2L, V_D2L, V_APR, V_D1R, V_AP2R, V_D2R, V_N };
static const uint16_t CV_L[V_N] = {CV_PRE, CV_DS(142), CV_DS(107), CV_DS(379), CV_DS(277), CV_DS(672), CV_DS(4453),
                                   CV_DS(1800), CV_DS(3720), CV_DS(908), CV_DS(4217), CV_DS(2656), CV_DS(3163)};
#define CV_TOTAL (CV_PRE + CV_DS(142) + CV_DS(107) + CV_DS(379) + CV_DS(277) + CV_DS(672) + CV_DS(4453) + \
                  CV_DS(1800) + CV_DS(3720) + CV_DS(908) + CV_DS(4217) + CV_DS(2656) + CV_DS(3163) + 2 * V_N + \
                  2 * (CV_EXC + 2))           /* (cv_init: 2 more a line, the swing's room on the two that swing) */
/* the output taps: line, the paper's position, sign (left, then right) */
static const struct { uint8_t line; uint16_t pos; int8_t sign; } CV_TAP[14] = {
    {V_D1R, 266, 1}, {V_D1R, 2974, 1}, {V_AP2R, 1913, -1}, {V_D2R, 1996, 1}, {V_D1L, 1990, -1}, {V_AP2L, 187, -1},
    {V_D2L, 1066, -1},
    {V_D1L, 353, 1}, {V_D1L, 3627, 1}, {V_AP2L, 1228, -1}, {V_D2L, 2673, 1}, {V_D1R, 2111, -1}, {V_AP2R, 335, -1},
    {V_D2R, 121, -1}};
/* the types: size (Q15 of the paper's), pre-delay (samples), the tank's damping (one-pole k, Q15). The owner found the
 * first tuning muddy (its tail was 94 % between 200 and 500 Hz, nothing above 1 kHz, 3.7 s at DECAY 45): now the
 * damping is light (the 8 kHz input cut does the rest), the input gets a little presence above 1 kHz, a low cut inside
 * the tank makes the low mids die first, the pre-delay keeps the wet off the note, and DECAY is an RT60 (CV_DEC) */
static const struct { uint16_t size, pre, damp; } CV_TYPE[3] = {
    {13800, 220, 26840},                         /* Room: a small plate, close (10 ms), 0.3 .. 2.5 s */
    {32767, 882, 29415},                         /* Hall: the full size, 40 ms, 0.8 .. 7 s */
    {24500, 330, 30872},                         /* Plate: bright and dense, 15 ms, 0.5 .. 5 s */
};
static const uint16_t CV_DEC[3][101] = {   /* the tank's decay (Q15) for DECAY 0..100: RT60 from the type's span, the
                                                * half-loop at its size; dec = 10^(-1.5 Th / RT60), two passes a half */
    {5637, 5849, 6064, 6283, 6504, 6729, 6956, 7186, 7418, 7653, 7890, 8130, 8371, 8614, 8859, 9105, 9353, 9603, 9853, 10105, 10357, 10610, 10864, 11119, 11374, 11629, 11885, 12140, 12396, 12651, 12906, 13161, 13415, 13669, 13922, 14174, 14426, 14676, 14926, 15174, 15421, 15667, 15911, 16154, 16396, 16636, 16874, 17111, 17345, 17578, 17810, 18039, 18266, 18492, 18715, 18936, 19155, 19372, 19587, 19800, 20010, 20218, 20424, 20628, 20829, 21028, 21224, 21419, 21611, 21800, 21987, 22172, 22355, 22535, 22712, 22888, 23061, 23231, 23399, 23565, 23729, 23890, 24049, 24206, 24360, 24512, 24662, 24809, 24954, 25097, 25238, 25377, 25513, 25648, 25780, 25910, 26038, 26164, 26287, 26409, 26529},
    {6836, 7070, 7307, 7546, 7787, 8031, 8277, 8525, 8775, 9027, 9280, 9535, 9790, 10048, 10306, 10565, 10824, 11085, 11346, 11607, 11868, 12130, 12391, 12652, 12913, 13174, 13434, 13693, 13952, 14210, 14467, 14723, 14978, 15232, 15484, 15735, 15985, 16233, 16480, 16725, 16968, 17209, 17448, 17686, 17922, 18155, 18387, 18616, 18843, 19068, 19291, 19512, 19730, 19946, 20159, 20371, 20580, 20786, 20990, 21192, 21391, 21587, 21781, 21973, 22162, 22349, 22533, 22715, 22894, 23071, 23246, 23418, 23587, 23754, 23918, 24081, 24240, 24398, 24552, 24705, 24855, 25003, 25149, 25292, 25433, 25571, 25708, 25842, 25974, 26104, 26231, 26357, 26480, 26602, 26721, 26838, 26953, 27067, 27178, 27287, 27395},
    {5026, 5245, 5468, 5696, 5927, 6162, 6401, 6644, 6889, 7138, 7390, 7645, 7903, 8163, 8425, 8690, 8956, 9225, 9495, 9766, 10039, 10313, 10588, 10864, 11140, 11417, 11695, 11972, 12250, 12527, 12804, 13081, 13358, 13633, 13908, 14182, 14455, 14727, 14997, 15267, 15534, 15800, 16065, 16328, 16589, 16848, 17105, 17360, 17613, 17863, 18112, 18358, 18602, 18843, 19082, 19318, 19552, 19783, 20012, 20237, 20461, 20681, 20899, 21114, 21326, 21536, 21743, 21947, 22148, 22346, 22542, 22734, 22924, 23112, 23296, 23478, 23656, 23833, 24006, 24177, 24344, 24510, 24672, 24832, 24989, 25144, 25296, 25445, 25592, 25737, 25879, 26018, 26155, 26290, 26422, 26552, 26679, 26804, 26927, 27048, 27166},
};

#define CFX_STUDIO_REV_N 2048u
static union {
    int16_t master[CV_TOTAL];
    struct {
        int16_t reverb[CFX_STUDIO_REV_N];
        int16_t chorus[CHO_LEN];
    } studio[CF_FX_PARTS];
} cfx_reverb_memory __attribute__((section(".pool")));
_Static_assert(sizeof cfx_reverb_memory == sizeof cfx_reverb_memory.master, "Studio chorus and reverb fit the master reverb memory");
#define cv_buf cfx_reverb_memory.master
static struct {
    uint16_t base[V_N], n[V_N], w[V_N];          /* each line's place in cv_buf, its length, its next write */
    uint16_t len[V_N], tap[14];                  /* the lengths and taps at this size */
    int32_t pre, damp_k, dec;                    /* pre-delay; damping (Q15); decay (Q15) */
    int32_t hp[2], lp[2], pr, ls, dmp[2], tlc[2];   /* the input's filters, presence, low shelf; the tank's damping, its
                                                 * low cut */
    uint32_t ph, idle;                           /* the swing's phase; samples since anything went in */
    uint8_t ready;
} cv;

static void cv_init(void)                        /* the lines laid out in cv_buf (once) */
{
    uint32_t i, at = 0;
    for (i = 0; i < V_N; i++) {
        cv.base[i] = (uint16_t)at;
        cv.n[i] = (uint16_t)(CV_L[i] + 2u + (i == V_APL || i == V_APR ? CV_EXC + 2u : 0u));
        cv.w[i] = 0;
        at += cv.n[i];
    }
    cv.ready = 1;
}

/* the type and the DECAY value -> the lengths, the taps and the gains */
static void cv_coefs(uint32_t type, uint32_t decay)
{
    uint32_t i, sz = CV_TYPE[type].size;
    if (!cv.ready)
        cv_init();
    for (i = 0; i < V_N; i++) {
        uint32_t l = i == V_PRE ? CV_TYPE[type].pre : (CV_L[i] * sz) >> 15;
        cv.len[i] = (uint16_t)(l < 1u ? 1u : l);
    }
    for (i = 0; i < 14u; i++) {
        uint32_t t = ((uint32_t)CV_DS(CV_TAP[i].pos) * sz) >> 15;
        cv.tap[i] = (uint16_t)(t < 1u ? 1u : t >= cv.len[CV_TAP[i].line] ? cv.len[CV_TAP[i].line] - 1u : t);
    }
    cv.pre = CV_TYPE[type].pre;
    cv.damp_k = CV_TYPE[type].damp;
    cv.dec = CV_DEC[type][decay > 100u ? 100u : decay];
}

XF_AI int32_t cv_tap(uint32_t line, uint32_t k)  /* the value written k samples ago, k >= 1 */
{
    int32_t j = (int32_t)cv.w[line] - (int32_t)k;
    return cv_buf[cv.base[line] + (uint32_t)(j < 0 ? j + cv.n[line] : j)];
}
XF_AI void cv_push(uint32_t line, int32_t v)
{
    cv_buf[cv.base[line] + cv.w[line]] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    if (++cv.w[line] >= cv.n[line])
        cv.w[line] = 0;
}
XF_AI int32_t cv_ap(uint32_t line, int32_t x, int32_t g)   /* an all-pass of the line's length: out z + g v, v x - g z */
{
    int32_t z = cv_tap(line, cv.len[line]), v = x - ((g * z) >> 15);
    cv_push(line, v);
    return z + ((g * v) >> 15);
}
XF_AI int32_t cv_ap_mod(uint32_t line, int32_t x, int32_t g, int32_t off_q8)   /* .. its length swinging (Q8) */
{
    int32_t q = ((int32_t)cv.len[line] << 8) + off_q8, k = q >> 8, f = q & 255;
    int32_t z0 = cv_tap(line, (uint32_t)k), z = z0 + (((cv_tap(line, (uint32_t)k + 1u) - z0) * f) >> 8);
    int32_t v = x - ((g * z) >> 15);
    cv_push(line, v);
    return z + ((g * v) >> 15);
}

/* a half-rate block of the mono send -> the stereo wet (half rate). dec: the tank's decay for this block */
static void cv_run(const int32_t *in, int32_t *ol, int32_t *orr, uint32_t nh, int32_t dec)
{
    uint32_t j, t;
    int32_t sl, sc, any = 0;
    cv.ph += 2600000u * nh;                       /* the swing: ~0.4 Hz at 22.05 kHz, once a block */
    sl = (osc_sine(cv.ph) * CV_EXC) >> 7;         /* (Q8 samples) */
    sc = (osc_sine(cv.ph + 0x40000000u) * CV_EXC) >> 7;
    for (j = 0; j < nh; j++) {
        int32_t x = in[j] >> 2, a, b, fb_l, fb_r, o;
        any |= x;
        cv_push(V_PRE, x);
        x = cv.pre ? cv_tap(V_PRE, (uint32_t)cv.pre) : x;
        cv.hp[0] += ((x - cv.hp[0]) * 1815) >> 15;                /* the low cut: 200 Hz, two poles */
        x -= cv.hp[0];
        cv.hp[1] += ((x - cv.hp[1]) * 1815) >> 15;
        x -= cv.hp[1];
        cv.ls += ((x - cv.ls) * 3526) >> 15;                      /* the low mids under ~400 Hz, -4 dB: the mud */
        x -= (cv.ls * 12124) >> 15;
        cv.pr += ((x - cv.pr) * 8125) >> 15;                      /* presence: what is above ~1 kHz, +4 dB */
        x += ((x - cv.pr) * 19661) >> 15;
        cv.lp[0] += ((x - cv.lp[0]) * 29415) >> 15;               /* the high cut: 8 kHz, two poles */
        cv.lp[1] += ((cv.lp[0] - cv.lp[1]) * 29415) >> 15;
        x = cv_ap(V_IN1, cv.lp[1], 24576);                        /* the input diffusers: 0.75, 0.75, 0.625, 0.625 */
        x = cv_ap(V_IN2, x, 24576);
        x = cv_ap(V_IN3, x, 20480);
        x = cv_ap(V_IN4, x, 20480);
        fb_l = cv_tap(V_D2R, cv.len[V_D2R]);                      /* the tank: each half fed by the other's end */
        fb_r = cv_tap(V_D2L, cv.len[V_D2L]);
        a = cv_ap_mod(V_APL, x + ((fb_l * dec) >> 15), -22938, sl);
        cv_push(V_D1L, a);
        b = cv_tap(V_D1L, cv.len[V_D1L]);
        cv.dmp[0] += ((b - cv.dmp[0]) * cv.damp_k) >> 15;
        cv.tlc[0] += ((cv.dmp[0] - cv.tlc[0]) * 2685) >> 15;     /* the tank's low cut, 300 Hz: low mids die first */
        cv_push(V_D2L, cv_ap(V_AP2L, ((cv.dmp[0] - cv.tlc[0]) * dec) >> 15, 16384));
        a = cv_ap_mod(V_APR, x + ((fb_r * dec) >> 15), -22938, sc);
        cv_push(V_D1R, a);
        b = cv_tap(V_D1R, cv.len[V_D1R]);
        cv.dmp[1] += ((b - cv.dmp[1]) * cv.damp_k) >> 15;
        cv.tlc[1] += ((cv.dmp[1] - cv.tlc[1]) * 2685) >> 15;
        cv_push(V_D2R, cv_ap(V_AP2R, ((cv.dmp[1] - cv.tlc[1]) * dec) >> 15, 16384));
        for (t = 0, o = 0; t < 7u; t++)
            o += CV_TAP[t].sign * cv_tap(CV_TAP[t].line, cv.tap[t]);
        ol[j] = o * 5;                                            /* 0.6 x 7 taps, x 4 back from the quarter, x 2 */
        for (t = 7, o = 0; t < 14u; t++)
            o += CV_TAP[t].sign * cv_tap(CV_TAP[t].line, cv.tap[t]);
        orr[j] = o * 5;
    }
    cv.idle = any ? 0u : cv.idle + nh;
}

static void cfx_reverb(int32_t *l, int32_t *r, uint32_t n, int feed)
{
    int32_t *hr = xf.t[1], *ol = xf.t[2], *orr = xf.t[3], any;
    uint32_t i, nh = n >> 1;
    int fed = cfx_send(l, r, n, feed);
    if (!cv.ready)
        cv_coefs(1, 45);
    if (!fed && cv.idle > 15u * 22050u && !xf.up_live)
        return;                                   /* 15 s of nothing in: the tail is gone, nothing runs */
    XF_CALL(xf_down)(&xf.dn[1], cfx_m, hr, n);
    /* (switched off: the tail rings out, but in a second or two even at DECAY 100, where it would never end) */
    cv_run(hr, ol, orr, nh, feed || cv.dec < 16000 ? cv.dec : 16000);
    for (i = 0, any = 0; i < nh; i++)
        any |= ol[i] | orr[i];
    if (any)
        xf.up_live = 256u;
    else if (xf.up_live)
        xf.up_live = xf.up_live > nh ? xf.up_live - nh : 0u;
    XF_CALL(xf_up)(&xf.up[0], ol, cfx_wl, nh);
    XF_CALL(xf_up)(&xf.up[1], orr, cfx_wr, nh);
    cfx_mix(&cfx, l, r, CFX_REVERB, n, 256, 0);
}

static void cfx_chorus(int32_t *l, int32_t *r, uint32_t n)
{
    cfx_send(l, r, n, 1);
    xc_block(cfx_m, cfx_wl, cfx_wr, n);
    cfx_mix(&cfx, l, r, CFX_CHORUS, n, 256, 1);
}

static void cfx_phaser(cfx_dsp_t *dsp, int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t c, i, s;
    dsp->pph += dsp->pinc;
    for (c = 0; c < 2u; c++) {
        const int32_t *b = c ? r : l;
        int32_t q = 32 * 256 + CFX_MUL(osc_sine(dsp->pph + (c ? 0x40000000u : 0u)), dsp->pdep, 15), k, a;
        int32_t *w = c ? cfx_wr : cfx_wl, fb = dsp->pfb[c];
        q = q < 0 ? 0 : q > 63 * 256 - 1 ? 63 * 256 - 1 : q;
        k = q >> 8;
        a = CFX_PA[k] + (((CFX_PA[k + 1] - CFX_PA[k]) * (q & 255)) >> 8);
        for (i = 0; i < n; i++) {
            int32_t u = b[i] + CFX_MUL(fb, 19660, 15);    /* (feedback 0.6) */
            for (s = 0; s < 6u; s++) {
                int32_t y = dsp->px[c][s] + CFX_MUL(a, u - dsp->py[c][s], 15);
                dsp->px[c][s] = u;
                dsp->py[c][s] = y;
                u = y;
            }
            fb = u;
            w[i] = u;
        }
        dsp->pfb[c] = fb;
    }
    cfx_mix(dsp, l, r, CFX_PHASER, n, 256, 1);
}

/* ------------------------------------------------------ the power-on sound --- */
/* RAKT: a rising Dm7 electric-key strum with a snare in a live room.
 * Six rising notes, a brief second harmonic and a finite 2.2 s tail.
 * Independent of the chosen patches/FX, after the effects and before MASTER.
 * This audio-owned voice never emits MIDI, changes a part or writes a recording.
 */
static const struct { uint16_t ms; uint8_t note, vel; int8_t pan; } CF_BOOT[] = {
    {0,50,88,-10}, {100,53,76,-6}, {210,57,72,-2},
    {330,60,84,2}, {470,65,74,6}, {620,69,70,10} /* D3 F3 A3 C4 F4 A4 */
};
#define CF_BOOT_N ((uint32_t)(sizeof CF_BOOT / sizeof CF_BOOT[0]))
#define CF_BOOT_SAMPLES (FS * 2200u / 1000u)
#define CF_BOOT_FADE (FS * 500u / 1000u)
#define CF_BOOT_SNARE_AT (FS * 320u / 1000u)
#define CF_BOOT_ROOM_MASK 511u
#define CF_BOOT_ROOM_TAP(ms) (FS * (ms) / 4000u)
static const uint32_t CF_BOOT_INC[12] = {796254u,843601u,893765u,946911u,1003217u,1062871u,
    1126073u,1193033u,1263974u,1339134u,1418763u,1503127u}; /* MIDI 0..11, per sample */
static volatile uint8_t cf_boot_pending; /* delayed power-on request; notes cancel it */
static uint32_t cf_boot_ready_ms;
static volatile uint8_t cf_boot_go; /* UI mailbox: 1 start/audition, 2 cancel/fade */
static struct {
    uint8_t on,yield;
    int32_t gain; /* Q15; sample ramp gives way to a note or overload in <=256 samples */
    uint32_t t,pa[CF_BOOT_N];
    int32_t e[CF_BOOT_N],ei[CF_BOOT_N]; /* Q30 body and short hammer brightness */
} cfb;

static struct {
    int16_t delay[CF_BOOT_ROOM_MASK+1u];
    uint32_t cursor, noise, phase;
    int32_t wires, head, low, damped, l, r;
} cf_boot_snare __attribute__((section(".pool"), aligned(4)));

/* The room runs at a quarter sample rate. Uneven taps give the hit a wide
 * early reflection, and a damped feedback path lets it ring under the keys. */
static void cf_boot_snare_tick(unsigned age)
{
    uint32_t at=cf_boot_snare.cursor++;
    uint32_t noise=cf_boot_snare.noise*1664525u+1013904223u;
    cf_boot_snare.noise=noise;
    int32_t white=(int32_t)(noise>>16)-32768;
    cf_boot_snare.low+=(white-cf_boot_snare.low)>>2;
    int32_t dry=((white-cf_boot_snare.low)*(cf_boot_snare.wires>>16))>>15;
    uint32_t phase=cf_boot_snare.phase;
    int32_t body=(osc_sine(phase)*3+osc_sine(phase*2u))/4;
    dry+=(body*(cf_boot_snare.head>>16))>>15;
    cf_boot_snare.phase+=((1ull<<32)*185u*4u)/FS;
    cf_boot_snare.wires-=cf_boot_snare.wires>>8;
    cf_boot_snare.head-=cf_boot_snare.head>>7;
    if(age<16u)dry=dry*(int32_t)age/16;
    int32_t early_l=cf_boot_snare.delay[(at-CF_BOOT_ROOM_TAP(17))&CF_BOOT_ROOM_MASK];
    int32_t early_r=cf_boot_snare.delay[(at-CF_BOOT_ROOM_TAP(27))&CF_BOOT_ROOM_MASK];
    int32_t late_l=cf_boot_snare.delay[(at-CF_BOOT_ROOM_TAP(37))&CF_BOOT_ROOM_MASK];
    int32_t late_r=cf_boot_snare.delay[(at-CF_BOOT_ROOM_TAP(43))&CF_BOOT_ROOM_MASK];
    cf_boot_snare.damped+=((late_l+late_r)/2-cf_boot_snare.damped)>>2;
    cf_boot_snare.delay[at&CF_BOOT_ROOM_MASK]=(int16_t)(dry+cf_boot_snare.damped*29/32);
    cf_boot_snare.l=dry+(early_l+late_r)/2;
    cf_boot_snare.r=dry+(early_r+late_l)/2;
}

/* Preserve headroom for the snare transient while lifting the whole cue. */
static int32_t cf_boot_level(int32_t sample)
{
    sample+=sample/2;
    if(sample>20000)return 20000+(sample-20000)/4;
    if(sample<-20000)return -20000+(sample+20000)/4;
    return sample;
}

static void cf_boot_mix(int32_t *l,int32_t *r,uint32_t n)
{
    unsigned request=cf_boot_go;
    if(request) {
        cf_boot_go=0;
        if(request==1){
            memset(&cfb,0,sizeof cfb);cfb.on=1;cfb.gain=32768;
            memset(&cf_boot_snare,0,sizeof cf_boot_snare);
            cf_boot_snare.wires=7200<<16;cf_boot_snare.head=3200<<16;
        }
        else cfb.yield=1;
    }
    if(!cfb.on)return;
    /* Note setup belongs outside the sample loop. In particular, MIDI
     * octave/remainder and note-time divisions are costly on the device. */
    if(cfb.t>=CF_BOOT_SAMPLES){cfb.on=0;return;}
    unsigned count=n;
    if(count>CF_BOOT_SAMPLES-cfb.t)count=CF_BOOT_SAMPLES-cfb.t;
    if(cfb.yield && count>(unsigned)(cfb.gain-1)/128u)count=(unsigned)(cfb.gain-1)/128u;
    int32_t boot_l[CTL]={0},boot_r[CTL]={0};
    for(unsigned k=0;k<CF_BOOT_N;k++) {
        unsigned start=CF_BOOT[k].ms*FS/1000u;
        if(cfb.t+count<=start)continue;
        unsigned first=cfb.t<start?start-cfb.t:0;
        uint32_t inc=CF_BOOT_INC[CF_BOOT[k].note%12u]<<(CF_BOOT[k].note/12u);
        int32_t velocity=CF_BOOT[k].vel*80,pan=CF_BOOT[k].pan;
        for(unsigned i=first;i<count;i++) {
            unsigned at=cfb.t+i,age=at-start;
            if(!age)cfb.e[k]=cfb.ei[k]=1<<30;
            unsigned attack=age<FS*8u/1000u?age*32768u/(FS*8u/1000u):32768u;
            int32_t bright=(osc_sine(cfb.pa[k]*2u)*(cfb.ei[k]>>15))>>15;
            int32_t y=(osc_sine(cfb.pa[k])*7+bright)/8;
            y=(y*(cfb.e[k]>>15))>>15;y=(y*(int32_t)attack)>>15;
            y=y*velocity/32768;
            boot_l[i]+=y*(32-pan)/32;boot_r[i]+=y*(32+pan)/32;
            cfb.pa[k]+=inc;cfb.e[k]-=cfb.e[k]>>15;cfb.ei[k]-=cfb.ei[k]>>12;
        }
    }
    for(unsigned i=0;i<count;i++) {
        unsigned at=cfb.t+i;
        if(at>=CF_BOOT_SNARE_AT){
            unsigned age=at-CF_BOOT_SNARE_AT;
            if(!(age&3u))cf_boot_snare_tick(age);
            boot_l[i]+=cf_boot_snare.l;boot_r[i]+=cf_boot_snare.r;
        }
        unsigned remain=CF_BOOT_SAMPLES-at;
        int32_t fade=remain<CF_BOOT_FADE?remain*32768u/CF_BOOT_FADE:32768u;
        int32_t level=cfb.yield?cfb.gain-(int32_t)(128u*(i+1u)):cfb.gain;
        int32_t gain=(level*fade)>>15;
        l[i]+=(cf_boot_level(boot_l[i])*gain)>>15;
        r[i]+=(cf_boot_level(boot_r[i])*gain)>>15;
    }
    cfb.t+=count;
    if(cfb.yield)cfb.gain-=(int32_t)(128u*count);
    if(cfb.t>=CF_BOOT_SAMPLES || count<n || (cfb.yield && cfb.gain<=128))cfb.on=0;
}
/* The signature always yields before an instrument voice or its ringing tail. */
static int cf_boot_yield(void)
{
    if(cf_boot_go==1){cf_boot_go=0;return 1;}
    if(!cfb.on)return voice_tail_yield();
    cfb.yield=1;return 1;
}
#define SHED_FIRST() cf_boot_yield()
/* Also cancel a queued start: a key can arrive before the first chime block. */
static void cf_boot_note(void)
{
    cf_boot_pending=0;
    if(cf_boot_go==1)cf_boot_go=0;
    if(cfb.on)cfb.yield=1;
}

static void cfx_fade(int32_t *l, int32_t *r, uint32_t n, int32_t *blend, int32_t target)
{
    uint32_t i;
    int32_t gain = *blend;
    for (i = 0; i < n; i++) {
        if (gain < target) { gain += CFX_FADE_STEP; if (gain > target) gain = target; }
        else if (gain > target) { gain -= CFX_FADE_STEP; if (gain < target) gain = target; }
        if (!gain) { l[i] = cfx_dry[0][i]; r[i] = cfx_dry[1][i]; }
        else if (gain != 32768) {
            l[i] = cfx_dry[0][i] + CFX_MUL(l[i] - cfx_dry[0][i], gain, 15);
            r[i] = cfx_dry[1][i] + CFX_MUL(r[i] - cfx_dry[1][i], gain, 15);
        }
    }
    *blend = gain;
}

static void cfx_master(int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t i, h;
    if (!cfx_ready)
        return;
    if (cfx_pending && !cfx_blend) { cfx_set = cfx_target; cfx_pending = 0; }
    /* Save dry only during a transition/bypass. At steady unity the original
     * rendering path stays bit-identical and avoids copying the scratch pair. */
    int transitioning = cfx_pending || cfx_bypass || cfx_blend != 32768;
    if (transitioning) for (i = 0; i < n; i++) { cfx_dry[0][i] = l[i]; cfx_dry[1][i] = r[i]; }
    h = cfx_fnv(&cfx_set, sizeof cfx_set);
    if (h != cfx.hash || !cfx.fa1) {
        cfx.hash = h;
        cfx_coefs(&cfx, &cfx_set, 1);
        if (!cfx.fg)
            cfx.fg = cfx.fg_t;
        cfx_filter_coefs(&cfx);
    }
    cfx_xf_prepare();
    for (i = 0; i < CFX_N; i++) {
        uint32_t e = cfx_set.order[i];
        int on = e < CFX_N && ((cfx_set.on >> e) & 1u);
        switch (e) {
        case CFX_FILTER: if (on) cfx_filter(&cfx, cfx_set.v[CFX_FILTER][0], l, r, n); break;
        case CFX_DIST: if (on) cfx_dist(&cfx, l, r, n); break;
        case CFX_CHORUS: if (on) cfx_chorus(l, r, n); break;
        case CFX_PHASER: if (on) cfx_phaser(&cfx, l, r, n); break;
        case CFX_DELAY: cfx_delay(l, r, n, on); break;   /* (off: no new sound in, the tail rings out) */
        case CFX_REVERB: cfx_reverb(l, r, n, on); break;
        default: break;
        }
    }
    if (transitioning) cfx_fade(l, r, n, &cfx_blend, cfx_pending || cfx_bypass ? 0 : 32768);
}

#include "cf_fx_studio.c"

/* GLO's hold gesture publishes the attack step, or zero to release. The audio
 * thread owns the envelope and filter, so opening or closing GLO never cuts a sample. */
#define CFX_RISER_TOP (1u<<24)
static volatile uint16_t cfx_riser_step;
static uint32_t cfx_riser_phase, cfx_riser_noise;
static int32_t cfx_riser_low;

static __attribute__((noinline)) void cfx_riser_mix(int32_t *l,int32_t *r,uint32_t n)
{
    unsigned step=cfx_riser_step, phase=cfx_riser_phase;
    if(!step && !phase)return;
    for(unsigned i=0;i<n;i++){
        const unsigned release=CFX_RISER_TOP/(FS/20u);
        if(step)phase=phase+step<CFX_RISER_TOP?phase+step:CFX_RISER_TOP;
        else phase=phase>release?phase-release:0;

        int32_t level=phase>>16;
        int32_t gain=(level*level)>>8;
        cfx_riser_noise=cfx_riser_noise*1664525u+1013904223u;
        int32_t white=(int32_t)(cfx_riser_noise>>16)-32768;
        cfx_riser_low+=((white-cfx_riser_low)*(12+(gain*3>>2)))>>8;
        int32_t sample=(cfx_riser_low*gain)/2048;
        l[i]+=sample;
        r[i]+=sample;
    }
    cfx_riser_phase=phase;
}

/* Record a mono slice at 1/8 sample rate. The 8-bit encoding keeps more
 * resolution near zero. The live mix keeps running beneath the loop. */
#define CFX_REPEAT_DS 8u
#define CFX_REPEAT_CAP 3072u
#define CFX_REPEAT_FADE 128u
#define CFX_REPEAT_NEW (1u<<15)
static volatile uint16_t cfx_repeat_request;
static int8_t cfx_repeat_buf[CFX_REPEAT_CAP];
static unsigned cfx_repeat_count, cfx_repeat_pos, cfx_repeat_len, cfx_repeat_wet;
static unsigned cfx_repeat_generation;
static int32_t cfx_repeat_sum;

static int8_t cfx_slice_encode(int32_t sample)
{
    int32_t mag=sample<0?-sample:sample;
    int32_t code=mag<1024?mag>>5:mag<7168?24+(mag>>7):66+(mag>>9);
    code=clamp(code,0,127);
    return (int8_t)(sample<0?-code:code);
}

static int32_t cfx_slice_decode(int32_t code)
{
    int32_t mag=code<0?-code:code;
    int32_t sample=mag<32?mag<<5:mag<80?(mag-24)<<7:(mag-66)<<9;
    return code<0?-sample:sample;
}

static __attribute__((noinline)) void cfx_repeat_mix(int32_t *l,int32_t *r,uint32_t n)
{
    unsigned command=cfx_repeat_request;
    unsigned generation=command&CFX_REPEAT_NEW;
    unsigned request=(command&~CFX_REPEAT_NEW)*CFX_REPEAT_DS;
    if(cfx_repeat_wet && generation!=cfx_repeat_generation)request=0;
    if(!cfx_repeat_wet && (!request || generation!=cfx_repeat_generation)){
        cfx_repeat_count=cfx_repeat_pos=cfx_repeat_len=0;
        cfx_repeat_sum=0;
        if(!request)return;
    }
    if(request)cfx_repeat_generation=generation;
    for(unsigned i=0;i<n;i++){
        if(request && cfx_repeat_count<CFX_REPEAT_CAP*CFX_REPEAT_DS){
            unsigned capture=cfx_repeat_count++;
            cfx_repeat_sum+=(l[i]+r[i])>>1;
            if(!(cfx_repeat_count&(CFX_REPEAT_DS-1u))){
                cfx_repeat_buf[capture/CFX_REPEAT_DS]=cfx_slice_encode(cfx_repeat_sum>>3);
                cfx_repeat_sum=0;
            }
        }
        unsigned pos=cfx_repeat_pos, len=cfx_repeat_len;
        if(!pos && request && cfx_repeat_count>=request)len=cfx_repeat_len=request;
        if(!len)continue;
        unsigned frame=pos/CFX_REPEAT_DS;
        int32_t sample=cfx_slice_decode(cfx_repeat_buf[frame]);
        if(pos+CFX_REPEAT_DS<len)
            sample+=((cfx_slice_decode(cfx_repeat_buf[frame+1])-sample)*(int32_t)(pos&(CFX_REPEAT_DS-1u)))>>3;
        unsigned edge=pos<len-1u-pos?pos:len-1u-pos;
        if(edge<CFX_REPEAT_FADE)sample=(sample*(int32_t)edge)>>7;
        if(request && cfx_repeat_wet<CFX_REPEAT_FADE)cfx_repeat_wet++;
        if(!request && cfx_repeat_wet)cfx_repeat_wet--;
        int32_t wet=(int32_t)cfx_repeat_wet;
        l[i]+=((sample-l[i])*wet)>>7;
        r[i]+=((sample-r[i])*wet)>>7;
        cfx_repeat_pos=pos+1u==len?0:pos+1u;
        if(!request && !wet){
            cfx_repeat_count=cfx_repeat_pos=cfx_repeat_len=0;
            cfx_repeat_sum=0;
            break;
        }
    }
}

/* Echo Out uses a short, dark mono delay. In GLO the ring follows the mix.
 * Holding the key cuts the dry signal and replaces the input with decaying feedback. */
#define CFX_ECHO_DS 16u
#define CFX_ECHO_CAP 384u
#define CFX_ECHO_FADE 128
static volatile uint16_t cfx_echo_request;
static volatile uint8_t cfx_echo_listen;
static int8_t cfx_echo_buf[CFX_ECHO_CAP];
static unsigned cfx_echo_pos, cfx_echo_filled, cfx_echo_len, cfx_echo_sub;
static int32_t cfx_echo_sum, cfx_echo_from, cfx_echo_to, cfx_echo_wet;

static __attribute__((noinline)) void cfx_echo_mix(int32_t *l,int32_t *r,uint32_t n)
{
    unsigned request=cfx_echo_request;
    int32_t wet=cfx_echo_wet;
    if(!cfx_echo_listen && !wet){
        cfx_echo_filled=cfx_echo_sub=0;
        cfx_echo_sum=cfx_echo_from=cfx_echo_to=0;
        return;
    }
    if(request)cfx_echo_len=request;
    for(unsigned i=0;i<n;i++){
        unsigned pos=cfx_echo_pos, sub=cfx_echo_sub;
        if(!sub && (request || wet)){
            cfx_echo_from=cfx_echo_to;
            unsigned read=pos+CFX_ECHO_CAP-cfx_echo_len;
            if(read>=CFX_ECHO_CAP)read-=CFX_ECHO_CAP;
            cfx_echo_to=cfx_echo_len<=cfx_echo_filled?cfx_slice_decode(cfx_echo_buf[read]):0;
        }
        int32_t live=(l[i]+r[i])>>1;
        if(request)wet+=wet<CFX_ECHO_FADE;
        else wet-=wet>0;
        if(wet){
            int32_t echo=cfx_echo_from+((cfx_echo_to-cfx_echo_from)*(int32_t)sub>>4);
            int32_t tail=echo*3/4;
            live+=((tail*3/4-live)*wet)>>7;
            l[i]+=((tail-l[i])*wet)>>7;
            r[i]+=((tail-r[i])*wet)>>7;
        }
        cfx_echo_sum+=live;
        cfx_echo_sub=(sub+1u)&(CFX_ECHO_DS-1u);
        if(!cfx_echo_sub){
            cfx_echo_buf[pos]=cfx_slice_encode(cfx_echo_sum>>4);
            cfx_echo_sum=0;
            cfx_echo_pos=pos+1u==CFX_ECHO_CAP?0:pos+1u;
            cfx_echo_filled+=cfx_echo_filled<CFX_ECHO_CAP;
        }
    }
    cfx_echo_wet=wet;
}

static volatile uint16_t cfx_sweep_step;
static uint32_t cfx_sweep_phase;
static int32_t cfx_sweep_low[2];

/* Raise the high-pass cutoff over two beats, then ease its low end back in. */
static __attribute__((noinline)) void cfx_sweep_mix(int32_t *l,int32_t *r,uint32_t n)
{
    const unsigned release=CFX_RISER_TOP/(FS/30u);
    unsigned step=cfx_sweep_step,phase=cfx_sweep_phase;
    for(unsigned i=0;i<n;i++){
        if(step)phase=phase+step<CFX_RISER_TOP?phase+step:CFX_RISER_TOP;
        else phase=phase>release?phase-release:0;
        int32_t level=phase>>16;
        int32_t coeff=2+(level*level>>9);
        int32_t wet=level<32?level*8:256;
        cfx_sweep_low[0]+=(l[i]-cfx_sweep_low[0])*coeff>>8;
        cfx_sweep_low[1]+=(r[i]-cfx_sweep_low[1])*coeff>>8;
        l[i]-=cfx_sweep_low[0]*wet>>8;
        r[i]-=cfx_sweep_low[1]*wet>>8;
    }
    cfx_sweep_phase=phase;
    if(!phase)cfx_sweep_low[0]=cfx_sweep_low[1]=0;
}

static void cf_fx_chain(int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t i;
    int ready = cfx_route(cfx_studio);
    if (cfx_studio) cfx_studio_mix(l, r, n, ready);
    else if (ready) cfx_master(l, r, n);
    if(cfx_repeat_request || cfx_repeat_count)cfx_repeat_mix(l,r,n);
    if(cfx_sweep_step || cfx_sweep_phase)cfx_sweep_mix(l,r,n);
    if(cfx_echo_listen || cfx_echo_wet || cfx_echo_filled || cfx_echo_sub)cfx_echo_mix(l,r,n);
    if(cfx_riser_step || cfx_riser_phase)cfx_riser_mix(l,r,n);
#if CF_VLOAD
    {
        uint32_t vl0 = VL_NOW();
        cf_boot_mix(l, r, n);                    /* (the power-on sound: after the effects, before MASTER) */
        vl_aside_ticks += VL_NOW() - vl0;        /* (voice.c: it gives way, the parts' voices do not wait for it) */
    }
#else
    cf_boot_mix(l, r, n);                        /* (the power-on sound: after the effects, before MASTER) */
#endif
    {
        int32_t pk = 0;
        for (i = 0; i < n; i++) {
            int32_t a = l[i] < 0 ? -l[i] : l[i], b = r[i] < 0 ? -r[i] : r[i];
            pk |= a | b;
        }
        cfx_quiet = pk > 63 ? 0u : cfx_quiet + 1u;
    }
}
