/* SPDX-License-Identifier: GPL-3.0-only
 * Four independent Studio inserts. The time effects use quarter-rate lines so
 * the Studio and master paths can use the same pool allocation. */
#define CFX_STUDIO_DS CFX_STUDIO_DELAY_DS
#define CFX_STUDIO_HZ (FS / CFX_STUDIO_DS)
#define CFX_DELAY_MASK (CFX_STUDIO_DELAY_N - 1u)
#define CFX_REV_LINE (CFX_STUDIO_REV_N / 4u)

typedef struct {
    cf_fx_t set, target;
    uint8_t pending, own, bypass, target_bypass;
    int32_t blend;
    cfx_dsp_t dsp;
    uint32_t chorus_pos, chorus_phase, chorus_inc, chorus_live;
    int32_t chorus_read[2];
    uint16_t chorus_depth, delay_time, delay_fb, rev_len[4], rev_gain;
    uint32_t delay_pos, delay_live, rev_pos, rev_live;
    int32_t delay_last[2], delay_lp[2], rev_last[2], rev_lp[4];
} cfx_part_t;
static cfx_part_t cfx_parts[CF_FX_PARTS] __attribute__((section(".pool")));

static void cfx_part_publish(unsigned part, const cf_fx_t *fx, int recall, int bypass)
{
    cfx_part_t *p = &cfx_parts[part];
    fm1_irq_off();
    if (!p->own && recall) {
        p->set = cfx_pending ? cfx_target : cfx_set;
        p->bypass = cfx_bypass;
    }
    if (recall || p->pending) {
        p->target = *fx;
        p->target_bypass = (uint8_t)bypass;
        p->pending = 1;
    } else {
        p->set = *fx;
        p->bypass = (uint8_t)bypass;
    }
    p->own = 1;
    fm1_irq_on();
}

/* Both layouts start zeroed at boot. On a route change clear the overlapping
 * rings over several blocks while passing dry audio. Never clear the whole
 * delay allocation in one interrupt. */
static int cfx_route(int studio)
{
    static int8_t route = -1;
    static uint32_t cleared;
    const uint32_t delay_size = sizeof cfx_delay_memory, reverb_size = sizeof cfx_reverb_memory;
    const uint32_t end = delay_size + reverb_size;
    if (route != studio) {
        if (route < 0) cleared = end;
        else cleared = 0;
        route = (int8_t)studio;
        if (studio) {
            for (unsigned i = 0; i < CF_FX_PARTS; i++) {
                cfx_part_t *p = &cfx_parts[i];
                memset(&p->dsp, 0, sizeof *p - __builtin_offsetof(cfx_part_t, dsp));
                p->blend = cleared ? 32768 : 0;
            }
        } else if (!cleared) {
            memset(&cfx, 0, sizeof cfx);
            memset(&xf, 0, sizeof xf);
            memset(&cv, 0, sizeof cv);
            cfx_blend = 0;
        }
    }
    if (cleared < delay_size) {
        uint32_t n = delay_size - cleared;
        if (n > 2048u) n = 2048u;
        memset((uint8_t *)&cfx_delay_memory + cleared, 0, n);
        cleared += n;
        return 0;
    }
    if (cleared < delay_size + reverb_size) {
        uint32_t n = delay_size + reverb_size - cleared;
        if (n > 2048u) n = 2048u;
        memset((uint8_t *)&cfx_reverb_memory + cleared - delay_size, 0, n);
        cleared += n;
        return 0;
    }
    return 1;
}

static void cfx_part_coefs(cfx_part_t *p, const cf_fx_t *s)
{
    static const uint16_t lengths[] = {293, 349, 419, 503};
    static const uint8_t decay_min[] = {3, 8, 5}, decay_max[] = {25, 70, 50};
    const uint8_t *d = s->v[CFX_DELAY], *c = s->v[CFX_CHORUS], *r = s->v[CFX_REVERB];
    unsigned type = r[0];
    cfx_coefs(&p->dsp, s, 0);
    p->delay_time = (uint16_t)(xf_ms(d[0] * 127u / 100u) * CFX_STUDIO_HZ / 1000u);
    p->delay_fb = (uint16_t)(d[1] * 301u);
    p->chorus_inc = LFO_INC[8u + c[0]];
    p->chorus_depth = (uint16_t)(c[1] * 127u * 6u / 100u);
    unsigned rt60 = decay_min[type] + r[1] * (decay_max[type] - decay_min[type]) / 100u;
    unsigned total = 0;
    for (unsigned k = 0; k < 4; k++) {
        p->rev_len[k] = (uint16_t)((lengths[k] * CV_TYPE[type].size) >> 15);
        total += p->rev_len[k];
    }
    p->rev_gain = (uint16_t)xf_expn((4527090u / CFX_STUDIO_HZ) * (total / 4u) / rt60);
}

static void cfx_part_chorus(cfx_part_t *p, unsigned part, uint32_t n, int fed)
{
    int16_t *line = cfx_reverb_memory.studio[part].chorus;
    uint32_t w = p->chorus_pos, phase = p->chorus_phase;
    p->chorus_phase += p->chorus_inc;
    int32_t pos[2], step[2];
    uint32_t live = p->chorus_live;
    for (unsigned side = 0; side < 2; side++, phase += 0x40000000u) {
        pos[side] = live ? p->chorus_read[side] :
            (400 << 8) + ((osc_sine(phase) + 32768) * p->chorus_depth >> 8);
        int32_t end = (400 << 8) + ((osc_sine(phase + p->chorus_inc) + 32768) * p->chorus_depth >> 8);
        step[side] = (end - pos[side]) / (int32_t)n;
        p->chorus_read[side] = pos[side] + step[side] * (int32_t)n;
    }
    int32_t *wet[2] = {cfx_wl, cfx_wr};
    for (unsigned i = 0; i < n; i++, w++) {
        line[w & (CHO_LEN - 1u)] = (int16_t)clamp(cfx_m[i] >> 1, -32767, 32767);
        for (unsigned side = 0; side < 2; side++) {
            pos[side] += step[side];
            uint32_t read = w - ((uint32_t)pos[side] >> 8);
            uint32_t age = (uint32_t)pos[side] >> 8;
            int32_t a = age <= p->chorus_pos + i ? line[read & (CHO_LEN - 1u)] : 0;
            int32_t b = age < p->chorus_pos + i ? line[(read - 1u) & (CHO_LEN - 1u)] : 0;
            wet[side][i] = 2 * (a + ((b - a) * (pos[side] & 255) >> 8));
        }
    }
    p->chorus_pos = w > CHO_LEN * 2u ? w - CHO_LEN : w;
    p->chorus_live = fed ? CHO_LEN : live > n ? live - n : 0;
}

static void cfx_part_upsample(unsigned i, int32_t *last, int32_t left, int32_t right)
{
    int32_t dl = left - last[0], dr = right - last[1];
    for (unsigned j = 0; j < CFX_STUDIO_DS; j++) {
        cfx_wl[i + j] = last[0] + ((dl * (int32_t)(j + 1u)) >> 2);
        cfx_wr[i + j] = last[1] + ((dr * (int32_t)(j + 1u)) >> 2);
    }
    last[0] = left;
    last[1] = right;
}

static int32_t cfx_part_downsample(unsigned i)
{
    return clamp((cfx_m[i] + cfx_m[i + 1] + cfx_m[i + 2] + cfx_m[i + 3]) >> 3, -32767, 32767);
}

static void cfx_part_delay(cfx_part_t *p, unsigned part, unsigned n, int fed, int enabled)
{
    int16_t *left = cfx_delay_memory.studio[part], *right = left + CFX_STUDIO_DELAY_N;
    unsigned w = p->delay_pos;
    int32_t fb = enabled ? p->delay_fb : p->delay_fb / 2;
    int32_t any = 0;
    for (unsigned i = 0; i < n; i += CFX_STUDIO_DS, w++) {
        unsigned at = (w - p->delay_time) & CFX_DELAY_MASK;
        if (w >= p->delay_time) {
            p->delay_lp[0] += (left[at] - p->delay_lp[0]) / 2;
            p->delay_lp[1] += (right[at] - p->delay_lp[1]) / 2;
        }
        left[w & CFX_DELAY_MASK] = (int16_t)clamp(cfx_part_downsample(i) + p->delay_lp[1] * fb / 32768, -32767, 32767);
        right[w & CFX_DELAY_MASK] = (int16_t)(p->delay_lp[0] * fb / 32768);
        any |= left[w & CFX_DELAY_MASK] | right[w & CFX_DELAY_MASK];
        cfx_part_upsample(i, p->delay_last, p->delay_lp[0] * 2, p->delay_lp[1] * 2);
    }
    p->delay_pos = w > CFX_STUDIO_DELAY_N * 2u ? w - CFX_STUDIO_DELAY_N : w;
    p->delay_live = fed || any ? CFX_STUDIO_DELAY_N * CFX_STUDIO_DS : p->delay_live > n ? p->delay_live - n : 0;
}

/* Four damped feedback lines, mixed by a normalized Hadamard matrix. */
static void cfx_part_reverb(cfx_part_t *p, unsigned part, unsigned n, unsigned type, int fed, int enabled)
{
    int16_t *ring = cfx_reverb_memory.studio[part].reverb;
    unsigned w = p->rev_pos;
    int32_t gain = enabled || p->rev_gain < 25000 ? p->rev_gain : 25000;
    int32_t damp = 32768 - CV_TYPE[type].damp / 8, any = 0;
    for (unsigned i = 0; i < n; i += CFX_STUDIO_DS, w++) {
        for (unsigned k = 0; k < 4; k++) {
            unsigned len = p->rev_len[k];
            int32_t tap = w >= len ? ring[k * CFX_REV_LINE + ((w - len) & (CFX_REV_LINE - 1u))] : 0;
            p->rev_lp[k] += (tap - p->rev_lp[k]) * damp / 32768;
        }
        int32_t a = p->rev_lp[0] + p->rev_lp[1], b = p->rev_lp[0] - p->rev_lp[1];
        int32_t c = p->rev_lp[2] + p->rev_lp[3], d = p->rev_lp[2] - p->rev_lp[3];
        int32_t in = cfx_part_downsample(i) / 4;
        int32_t sum[4] = {a + c, b + d, a - c, b - d};
        for (unsigned k = 0; k < 4; k++) {
            int32_t feedback = (sum[k] / 2) * gain / 32768;
            int32_t out = clamp(in + feedback, -32767, 32767);
            ring[k * CFX_REV_LINE + (w & (CFX_REV_LINE - 1u))] = (int16_t)out;
            any |= out;
        }
        cfx_part_upsample(i, p->rev_last, (a - d) * 2, (b + c) * 2);
    }
    p->rev_pos = w > CFX_REV_LINE * 2u ? w - CFX_REV_LINE : w;
    p->rev_live = fed || any ? CFX_REV_LINE * CFX_STUDIO_DS : p->rev_live > n ? p->rev_live - n : 0;
}

static void cfx_studio_mix(int32_t *l, int32_t *r, unsigned n, int ready)
{
    for (unsigned part = 0; part < CF_FX_PARTS; part++) {
        int32_t *pl = cfx_part_mix[part][0], *pr = cfx_part_mix[part][1];
        cfx_part_t *p = &cfx_parts[part];
        if (ready) {
            if (p->pending && !p->blend) {
                p->set = p->target;
                p->bypass = p->target_bypass;
                p->pending = 0;
                memset(&p->dsp, 0, sizeof *p - __builtin_offsetof(cfx_part_t, dsp));
            }
            const cf_fx_t *fx = p->own ? &p->set : cfx_pending ? &cfx_target : &cfx_set;
            int bypass = p->own ? p->bypass : cfx_bypass;
            static const uint8_t sends[CFX_N] = {[CFX_CHORUS]=P_CHOR, [CFX_DELAY]=P_DLY, [CFX_REVERB]=P_REV};
            int extra = trk[part].p[P_CHOR] | trk[part].p[P_DLY] | trk[part].p[P_REV];
            if ((!bypass && (fx->on || extra)) || p->pending || p->chorus_live || p->delay_live || p->rev_live) {
                uint32_t hash = cfx_fnv(fx, sizeof *fx);
                if (p->dsp.hash != hash) {
                    p->dsp.hash = hash;
                    cfx_part_coefs(p, fx);
                }
                int transitioning = p->pending || bypass || p->blend != 32768;
                if (transitioning) {
                    memcpy(cfx_dry[0], pl, n * sizeof *pl);
                    memcpy(cfx_dry[1], pr, n * sizeof *pr);
                }
                for (unsigned i = 0; i < CFX_N; i++) {
                    unsigned e = fx->order[i];
                    if (e >= CFX_N) continue;
                    int enabled = !bypass && ((fx->on >> e) & 1u);
                    int fed;
                    if (sends[e]) {
                        int32_t mix = enabled ? fx->v[e][2] * 32768 / 100 : 0;
                        int32_t send = bypass ? 0 : trk[part].p[sends[e]];
                        p->dsp.wt[e] = mix + (32768 - mix) * send / 127;
                        enabled = p->dsp.wt[e] != 0;
                    }
                    switch (e) {
                    case CFX_FILTER: if (enabled) cfx_filter(&p->dsp, fx->v[e][0], pl, pr, n); break;
                    case CFX_DIST: if (enabled) cfx_dist(&p->dsp, pl, pr, n); break;
                    case CFX_PHASER: if (enabled) cfx_phaser(&p->dsp, pl, pr, n); break;
                    case CFX_CHORUS:
                        if (!enabled && !p->chorus_live) break;
                        fed = cfx_send(pl, pr, n, enabled);
                        if (fed || p->chorus_live) {
                            cfx_part_chorus(p, part, n, fed);
                            cfx_mix(&p->dsp, pl, pr, e, n, 256, 1);
                        }
                        break;
                    case CFX_DELAY:
                        if (!enabled && !p->delay_live) break;
                        fed = cfx_send(pl, pr, n, enabled);
                        if (fed || p->delay_live) {
                            cfx_part_delay(p, part, n, fed, enabled);
                            cfx_mix(&p->dsp, pl, pr, e, n, 768, 0);
                        }
                        break;
                    case CFX_REVERB:
                        if (!enabled && !p->rev_live) break;
                        fed = cfx_send(pl, pr, n, enabled);
                        if (fed || p->rev_live) {
                            cfx_part_reverb(p, part, n, fx->v[e][0], fed, enabled);
                            cfx_mix(&p->dsp, pl, pr, e, n, 256, 0);
                        }
                        break;
                    }
                }
                if (transitioning) cfx_fade(pl, pr, n, &p->blend, p->pending || bypass ? 0 : 32768);
            } else p->blend = bypass ? 0 : 32768;
        }
        for (unsigned i = 0; i < n; i++) { l[i] += pl[i]; r[i] += pr[i]; }
    }
}
