/* SPDX-License-Identifier: GPL-3.0-only */
/* Real CoralFlower render, audio.c capture and UAC code on the emulator HAL, with USB registers/DMA mocked.
 * Does NOT establish physical USB enumeration, hardware ISR timing or macOS compatibility.
 * cc -std=gnu11 -O2 -w -Itools/emu/web/compat -Ibuild/gen -Ifirmware/src -o build/host/cf_usb_test tests/cf_usb_test.c -lm
 */
#include <stdio.h>
#include <math.h>
#include "usb_mock.h"
#define FELUCCA_UAC 1
#define FELUCCA_CDC 1
#include "../tools/emu/emu_fw.c"
static void ua_service_paced(uint32_t t0, int from_isr) { (void)t0; (void)from_isr; ua_service(); }

static unsigned checks, failures;
#define CHECK(c) do { checks++; if (!(c)) { failures++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint32_t le16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }
static uint32_t le24(const uint8_t *p) { return le16(p) | (uint32_t)p[2] << 16; }
static uint32_t syx32(const uint8_t *p)
{ return p[0] | (uint32_t)p[1] << 7 | (uint32_t)p[2] << 14 | (uint32_t)p[3] << 21 | (uint32_t)p[4] << 28; }

/* Walk the descriptors actually returned by GET_DESCRIPTOR, not a second handwritten configuration. */
static void descriptors(int cdc_on)
{
    const uint8_t *d;
    uint16_t len;
    uint32_t i = 0, interfaces = 0, eps = 0, cls = 0, sub = 0, format = 0, midi = 0, capture = 0;
    CHECK(get_desc(0x0200u, &d, &len));
    CHECK(le16(d + 2) == len && len == (cdc_on ? 175u : 211u));
    while (i < len) {
        uint32_t n = d[i], type = d[i + 1u];
        CHECK(n >= 2u && i + n <= len);
        if (n < 2u || i + n > len) break;
        if (type == 4u) {
            cls = d[i + 5u]; sub = d[i + 6u];
            if (!d[i + 3u]) interfaces++;
            midi += cls == 1u && sub == 3u;
            capture += cls == 1u && sub == 2u;
        } else if (type == 5u) {
            uint32_t ep = d[i + 2u], bit = 1u << ((ep & 15u) + ((ep & 0x80u) ? 16u : 0u));
            CHECK(!(eps & bit)); eps |= bit;
            if (ep == 0x82u) {
                CHECK((d[i + 3u] & 3u) == (cdc_on ? 3u : 1u));
                CHECK(le16(d + i + 4u) == (cdc_on ? 8u : UA_EP_MAXP));
                CHECK(d[i + 6u] == 1u || cdc_on);
            }
        } else if (type == 0x24u && cls == 1u && sub == 2u && d[i + 2u] == 2u) {
            format++;
            CHECK(n == 14u && d[i + 3u] == 1u && d[i + 4u] == 6u && d[i + 5u] == 2u && d[i + 6u] == 16u);
            CHECK(d[i + 7u] == 2u && le24(d + i + 8u) == 44100u && le24(d + i + 11u) == 48000u);
        }
        i += n;
    }
    CHECK(i == len && interfaces == d[4] && interfaces == 4u);
    CHECK(midi == 1u && capture == (cdc_on ? 0u : 2u) && format == (cdc_on ? 0u : 1u));
    CHECK((eps & (1u << 1)) && (eps & (1u << 17)) && (eps & (1u << 18)));
    CHECK(get_desc(0x0302u, &d, &len) && len == 20u);
    for (i = 0; i < 9u; i++) CHECK(d[2u + i * 2u] == (uint8_t)"RAKT FM-1"[i] && d[3u + i * 2u] == 0);
    CHECK(get_desc(0x0303u, &d, &len) && len == 16u);
    for (i = 0; i < 7u; i++) CHECK(d[2u + i * 2u] == (uint8_t)"RAKT In"[i]);
}

static void protocol(void)
{
    uint32_t rate, before;
    uint8_t req[8] = {0xA2, 0x81, 0, 1, 0x82, 0, 3, 0};
    usb_start(); descriptors(0);
    usb.config = 1;
    CHECK(!ua_set_interface(3, 2) && !ua_set_interface(9, 1));
    CHECK(ua_control_is(req) && ua_control_get(req, &rate) && rate == 44100u);
    req[1] = 0x82; CHECK(ua_control_get(req, &rate) && rate == 44100u);
    req[1] = 0x83; CHECK(ua_control_get(req, &rate) && rate == 48000u);
    req[4] = 0x81; CHECK(!ua_control_is(req)); req[4] = 0x82;
    CHECK(ua_set_interface(3, 1) && ua.cap_alt && ua.cap_hold);
    CHECK(um_ep[2].n == 528u && (um_enabled & 4u));
    before = um_ep[2].sends; ua_tx_fill(); CHECK(um_ep[2].sends == before);
    um_regs[2][S_TXCSR1] = 0; ua_tx_fill(); CHECK(!ua.cap_hold && um_ep[2].sends == before + 1u);
    req[0] = 0x22; req[1] = 1;
    CHECK(ua_control_setup(req));
    { const uint8_t r[3] = {0x80, 0xBB, 0}; CHECK(ua_control_data(r, 3) && ua.r48); }
    CHECK(ua_control_setup(req));
    { const uint8_t r[3] = {0x44, 0xAC, 0}; CHECK(ua_control_data(r, 3) && !ua.r48); }
    /* MIDI queues, endpoints, and the existing backup protocol remain available with capture open. */
    ep1_config();
    before = mi_w;
    midi_in_event(0x643C9009u); CHECK(mi_w == before + 1u);
    CHECK((midi_in_q[before & (MQ - 1u)] & 0xFFFFFFu) == 0x3C9009u);
    midi_out_event(0x643C9009u); ep1_tx();
    CHECK(um_ep[1].n == 4u && um_ep[1].bytes[0] == 9u && um_ep[1].bytes[1] == 0x90u);
    { const uint8_t info[4] = {0x7D, 0x46, 0x4C, 1}; CHECK(crb_handle(info, sizeof info)); CHECK(ua.cap_alt); }
    usb_bus_reset(); CHECK(!ua.cap_alt && !ua.r48 && !usb.config && !ua.cw && !ua.cr);
    cr_safe = 1; usb_start(); CHECK(USB_CDC_ON); descriptors(1);
    usb.config = 1; CHECK(ua_if_cap == UA_NO_IF || !ua_set_interface(ua_if_cap, 1) || !ua.cap_alt);
    cr_safe = 0; usb_start(); CHECK(!USB_CDC_ON);
    /* The inherited one-rate configuration and off/console option still allocate EP2 exclusively. */
    ua_off = ua_off_want = UA_OFF_48; usb_start(); CHECK(ua_cfg_len == 208u && ua_dev44);
    usb.config = 1; CHECK(ua_control_setup(req));
    { const uint8_t r[3] = {0x80, 0xBB, 0}; CHECK(ua_control_data(r, 3) && !ua.r48); }
    ua_off = ua_off_want = UA_OFF_IN; usb_start(); CHECK(USB_CDC_ON);
    ua_off = ua_off_want = 0; usb_start();
}

static void stream(void)
{
    int16_t samples[UA_RS_BLOCK * UA_CAP_CHANNELS];
    uint8_t packet[UA_PACKET];
    uint32_t i, ch, n, pos;
    ua_reset(); ua.cap_alt = 1; ua.cap_hold = 0;
    for (i = 0; i < UA_RS_BLOCK; i++) for (ch = 0; ch < 6u; ch++)
        samples[6u * i + ch] = (int16_t)((int)ch * 10000 - 30000 + (int)i);
    n = ua_transmit(packet); CHECK(n == 528u);
    for (i = 0; i < n; i++) CHECK(!packet[i]);
    for (i = 0; i < UA_CAP_TARGET / UA_RS_BLOCK; i++) ua_audio(samples, UA_RS_BLOCK);
    pos = ua.cr; n = ua_transmit(packet); CHECK(n == 528u || n == 540u);
    for (i = 0; i < n / 12u; i++) for (ch = 0; ch < 6u; ch++) {
        int16_t want = samples[((pos + i) % UA_RS_BLOCK) * 6u + ch];
        CHECK(packet[i * 12u + ch * 2u] == (uint8_t)want);
        CHECK(packet[i * 12u + ch * 2u + 1u] == (uint8_t)((uint16_t)want >> 8));
    }
    /* Extremes, channel order and saturation boundaries are transported without sign or endian changes. */
    { const int16_t extremes[6] = {-32768, 32767, -1, 0, 1, -256};
      ua.cr = ua.cw = 0; ua.cap_ready = 1;
      for (i = 0; i < 45u; i++) for (ch = 0; ch < 6u; ch++) ua.cap[i * 6u + ch] = extremes[ch];
      ua.cw = 45; n = ua_transmit(packet);
      for (ch = 0; ch < 6u; ch++) CHECK((int16_t)le16(packet + 2u * ch) == extremes[ch]); }
    ua.cap_hold = 1; pos = ua.cw; ua_audio(samples, 32); CHECK(ua.cw == pos);
    ua.cap_hold = 0; ua.cap_alt = 0; ua_audio(samples, 32); CHECK(ua.cw == pos);
    ua_reset(); ua.cap_alt = 1; ua.cap_ready = 1; ua_transmit(packet); CHECK(ua.cap_underruns == 1u);
    ua.cw = UA_CAP_RING; ua.cr = 0; ua_feed(samples, 32, 0); CHECK(ua.cap_overruns == 1u && ua.cw == 32u);
    ua_rate_set(48000); pos = ua.cw; ua_feed(samples, 32, 0); CHECK(ua.cw == pos);
    ua.rs_on = 1; pos = ua.cap_epoch; ua_cap_reset(); CHECK(!ua.rs_on && ua.cap_epoch == pos + 1u);
}

/* Simulated producer clocks, not a simulated hardware interrupt schedule. Every frame carries its sequence
 * number: packet changes must never duplicate, discard or reorder PCM to correct clock drift. */
static void drift(uint32_t hz, int ppm)
{
    uint64_t acc = 0;
    uint32_t ms, produced = 0, consumed = 0, packets = 0;
    uint8_t packet[UA_PACKET];
    int16_t block[32 * 6];
    memset(&ua, 0, sizeof ua); ua_reset(); ua.cap_alt = 1;
    ua_rate_set(hz);
    for (ms = 0; ms < 60000u; ms++) {
        uint32_t frames, k, j, ch, bytes, old;
        int valid = 1;
        acc += (uint64_t)hz * (uint32_t)(1000000 + ppm);
        frames = (uint32_t)(acc / 1000000000u); acc %= 1000000000u;
        while (frames) {
            k = frames > 32u ? 32u : frames;
            for (j = 0; j < k; j++) for (ch = 0; ch < 6u; ch++)
                block[j * 6u + ch] = (int16_t)((produced + j) % 10000u + 1000u * ch);
            ua_feed(block, k, hz == 48000u); produced += k; frames -= k;
        }
        old = ua.cr; ua_sof(); bytes = ua_transmit(packet); packets++;
        CHECK(bytes <= UA_PACKET && !(bytes % 12u));
        CHECK(hz == 44100u ? bytes == 528u || bytes == 540u : bytes >= 564u && bytes <= 588u);
        if (ua.cr != old) {
            for (j = 0; j < bytes / 12u; j++) for (ch = 0; ch < 6u; ch++)
                valid &= le16(packet + j * 12u + ch * 2u) == (consumed + j) % 10000u + 1000u * ch;
            consumed += bytes / 12u;
        }
        CHECK(valid);
    }
    CHECK(!ua.cap_underruns && !ua.cap_overruns && produced - consumed < UA_CAP_RING);
    CHECK(ua.cap_nudges_up || ua.cap_nudges_down || ppm == 0);
    printf("clock simulation: %u Hz %+d ppm, %u packets, %u/%u frames produced/consumed, no loss\n", hz, ppm, packets, produced, consumed);
}

static void resampler(void)
{
    int16_t block[32 * 6];
    uint32_t i, ch, k, frames = 0, n;
    memset(&ua, 0, sizeof ua); ua_reset();
    for (i = 0; i < 32u; i++) for (ch = 0; ch < 6u; ch++) block[i * 6u + ch] = (int16_t)((int)ch * 4000 - 10000);
    for (i = 0; i < 44100u;) {
        n = (i % 31u) + 1u; if (n > 44100u - i) n = 44100u - i;
        k = ua_resample(block, n); frames += k;
        CHECK(k <= UA_RS_OUT);
        if (i > 32u) for (ch = 0; ch < 6u; ch++)
            CHECK(abs((int)ua.rs_out[(k - 1u) * 6u + ch] - block[ch]) <= 3);
        i += n;
    }
    CHECK(frames == 48000u);
    ua.rs_on = 0; memset(block, 0, sizeof block); block[2] = 32767;
    k = ua_resample(block, 32);
    for (i = 0; i < k; i++) for (ch = 0; ch < 6u; ch++) if (ch != 2u) CHECK(!ua.rs_out[6u * i + ch]);
}

static void diagnostics(void)
{
    const uint8_t req[5] = {0x7D, 0x46, 0x4C, 79, 0};
    uint32_t saved = cfs_saved;
    ua.r48 = 1; ua.cap_alt = 1; ua_dbg.opens = 7; fm1_ms = 12345;
    CHECK(crb_handle(req, 4));
    CHECK(CRB_OUT[0] == 0xF0 && CRB_OUT[4] == 79u && CRB_OUT[5] == 1u && CRB_OUT[6] == 39u);
    CHECK(crb_n == 8u + 39u * 5u && CRB_OUT[crb_n - 1u] == 0xF7);
    CHECK(syx32(CRB_OUT + 7) == 48000u && syx32(CRB_OUT + 7 + 14u * 5u) == 12345u);
    CHECK(syx32(CRB_OUT + 7 + 15u * 5u) == 1u && syx32(CRB_OUT + 7 + 16u * 5u) == 7u);
    CHECK(cfs_saved == saved);
    if (getenv("CF_USB_DIAG")) {
        FILE *f = fopen(getenv("CF_USB_DIAG"), "wb");
        CHECK(f != NULL); if (f) { fwrite(CRB_OUT, crb_n, 1, f); fclose(f); }
    }
    CHECK(!crb_handle(req, 5));
}

static void render(int part, int clean, int effects, int capture, const char *path, uint32_t hz)
{
    const int studio = part == 8;
    uint32_t b, i, master_nonzero = 0, chord_nonzero = 0, bass_nonzero = 0;
    int32_t out[CTL * 2];
    FILE *f = path ? fopen(path, "wb") : NULL;
    if (path && !f) { CHECK(f); return; }
    cf_fx_t fx = CFX_DEFAULT;
    fx.on = effects ? (1u << CFX_N) - 1u : 0u;
    cf_fx_apply(&fx); fx_output_set((uint32_t)clean, 1);
    for (b = 0; b < 64u; b++) audio_block(out, CTL); /* finish the boot's sound/engine changes */
    ua_reset(); ua.cap_alt = (uint8_t)capture; ua.cap_hold = 0; usb.config = usb.up = 1;
    ua_rate_set(hz);
    usb.suspended = usb_cdc_on = 0;
    if (part < 4) trk_note_on(&trk[part], part == 3 ? 36u : part == 1 ? 36u : 60u, 100);
    else if (part == 4) cf_post(CFE_METRO, 1, 100, 0);
    else if (part == 5) cf_post(CFE_DRUM, 1, 0, 0);
    else if (part == 6) {
        sq_pat_t *p = &sq_st.pat[0];
        memset(p, 0, sizeof *p); p->bars = 1; p->nev = 3;
        for (i = 0; i < SQ_NTRK; i++) { p->tb[i] = 1; p->steps[i][0] = 4; }
        for (i = 0; i < 3u; i++) {
            p->ev[i].trk = (uint8_t)i; p->ev[i].n = 1; p->ev[i].len = 48;
            p->ev[i].rootkey = 0xFFu; p->ev[i].note[0] = i == 1u ? 36u : i == 2u ? 72u : 60u;
        }
        p->ev[0].mask = 1; p->hit[0][0] = 1; p->dlev[0] = 100;
        cf_post(CFE_SEQ, 1, 0, 0);
    } else if (part == 7) {
        cf_harmony_defaults(&cfh_shadow);
        cfh_shadow.mode = CF_H_AUTO;
        cfh_shadow.slot[0] = CF_MOD_13;
        cfp_config();
        cfp_key(1, 1); cfp_key(1, 0); cfp_key(7, 1);
        CHECK(cfc.out.automatic && cfc.out.n == 6 && cfp.n == 6 && cfp.nb);
    } else if (studio) {
        cgs_init(107);
        cgs_prepare_start();
        cgu_home();
        cf_snapshot();
        CHECK(cgs_home());
        ui_button(B_PLAY);
        CHECK(cpa_commanded);
        CHECK(cpa_setup.flags && cpa_valid(&cpa_setup.plan));
    }
    for (b = 0; b < 1600u; b++) {
        uint32_t w = ua.cw;
        audio_block(out, CTL);
        if (f) CHECK(fwrite(out, sizeof out, 1, f) == 1);
        if (capture) {
            uint32_t expected = hz == 48000u ? ((b + 1u) * CTL * 160u + 146u) / 147u : (b + 1u) * CTL;
            uint32_t count = expected - w;
            CHECK(ua_stage_on && ua.cw == expected && count <= UA_RS_OUT);
            for (i = 0; i < count; i++) {
                const int16_t *frame = ua.cap + ((w + i) & (UA_CAP_RING - 1u)) * 6u;
                if (hz == 44100u)
                    CHECK(frame[0] == ua_sat(out[2u * i] >> 8) && frame[1] == ua_sat(out[2u * i + 1u] >> 8));
                master_nonzero += frame[0] != 0 || frame[1] != 0;
                chord_nonzero += frame[2] != 0 || frame[3] != 0;
                bass_nonzero += frame[4] != 0 || frame[5] != 0;
            }
            ua.cr = ua.cw; /* the test consumes each block without waiting for the USB packet prime */
        } else CHECK(!ua_stage_on && ua.cw == w);
    }
    if (studio) {
        CHECK(cgs_active());
        CHECK(cpa.q2 > 0);
    }
    if (f) fclose(f);
    if (capture) {
        CHECK(master_nonzero > 0u);
        CHECK((chord_nonzero > 0u) == (part == 0 || part == 6 || part == 7 || studio));
        CHECK((bass_nonzero > 0u) == (part == 1 || part == 6 || part == 7 || studio));
        CHECK((ua.rs_blocks > 0u) == (hz == 48000u));
        printf("render %u Hz part %d clean %d effects %d: master/chord/bass nonzero frames %u/%u/%u\n", hz, part, clean, effects,
               master_nonzero, chord_nonzero, bass_nonzero);
        /* A part that becomes silent must clear its stem instead of retaining the preceding block. */
        if (studio) cpa_stop();
        sqp.on = dmp.on = cfm.on = cfm.running = 0;
        for (b = 0; b < NSND; b++) {
            memset(trk[b].v, 0, sizeof trk[b].v); trk[b].tail = 0;
        }
        for (b = 0; b < 4u; b++) { memset(ua_stage, 0x5A, sizeof ua_stage); audio_block(out, CTL); ua.cr = ua.cw; }
        for (i = 0; i < CTL; i++)
            CHECK(!ua_stage[6u * i + 2u] && !ua_stage[6u * i + 3u] && !ua_stage[6u * i + 4u] && !ua_stage[6u * i + 5u]);
        /* Host pause, suspend, reset/closed, console and SAFE MODE must all stop feeding the ring. */
        for (b = 0; b < 6u; b++) {
            uint32_t w = ua.cw;
            usb.up = usb.config = 1; usb.suspended = usb_cdc_on = cr_safe = 0; ua.cap_alt = 1; ua.cap_hold = 0;
            if (b == 0) usb.up = 0;
            if (b == 1) usb.suspended = 1;
            if (b == 2) ua.cap_alt = 0;
            if (b == 3) usb_cdc_on = 1;
            if (b == 4) cr_safe = 1;
            if (b == 5) ua.cap_hold = 1;
            audio_block(out, CTL); CHECK(!ua_stage_on && ua.cw == w);
        }
    }
}

int main(int argc, char **argv)
{
    emu_fw_options(NULL, 1, 0, 1);
    emu_hal.ready = 2;
    emu_fw_init(0);
    if (argc >= 6 && !strcmp(argv[1], "--render")) {
        render(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), argc > 6 ? argv[6] : NULL,
               argc > 7 ? (uint32_t)atoi(argv[7]) : 44100u);
    } else {
        protocol(); stream(); resampler(); diagnostics();
        drift(44100, 0); drift(44100, 400); drift(44100, -1000);
        drift(48000, 0); drift(48000, 400); drift(48000, -1000);
    }
    printf("cf_usb: %u checks, %u failures\n", checks, failures);
    return failures != 0;
}
