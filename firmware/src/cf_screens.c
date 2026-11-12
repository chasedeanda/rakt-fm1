/* SPDX-License-Identifier: GPL-3.0-only
 * CoralFlower: the screens' pictures and the popups, then the frame (cf_ui_draw). Included after cf_ui.c. */

#define fcos_ui(a) fl_fcos(a)                       /* (flora.c's integer sine) */
#define fsin_ui(a) fl_fsin(a)
static char cu_name[16], cu_name_sub[12];        /* the chord shown (it stays after the hands let go) */
static uint8_t cu_name_single;                   /* .. it is single notes (no chord button) */
static uint8_t cu_name_live;

static void name_set(uint32_t root, uint32_t mask, int bass_pc, int single, uint32_t n, const uint8_t *note)
{
    cu_name_live = 1;
    cu_name_single = (uint8_t)single;
    if (single) {
        char a[4];
        int k = 0;
        uint32_t i;
        cu_name[0] = 0;
        for (i = 0; i < n && i < 3u; i++) {
            cf_pc_name(note[i] % 12, &cu_spell, a);
            str_cpy(cu_name + k, a, sizeof cu_name - (uint32_t)k);
            k = (int)str_len(cu_name);
            if (i + 1u < n && i < 2u && k < 14)
                cu_name[k++] = ' ', cu_name[k] = 0;
        }
        cu_name_sub[0] = 0;
        return;
    }
    cf_chord_name((int)root, mask, bass_pc != (int)root ? bass_pc : -1, &cu_spell, cu_name);
    cu_name_sub[0] = 0;
    if (cs.chord.quantize != CF_Q_OFF) {
        if (ch.mode == CF_H_AUTO) cf_harmony_numeral(cs.chord.scale, cs.chord.tonic, (int)root, cu_name_sub);
        else cf_numeral(cs.chord.scale, cs.chord.tonic, (int)root, cu_name_sub);
    }
}
/* the chord shown: the keys' while they sound, else the sequencer's CHORDS track's while it plays (0.5, the owner:
 * "то, что играется в секвенсоре я бы хотел, чтобы на главной отображалось на цветочке"); it stays after both let go */
static void name_update(void)
{
    const cf_chord_out_t *o = &cf_sn.out;
    if (o->sounding && o->n)
        name_set(o->root_pc, o->mask, cs.bass_on ? o->bass_pc : -1, o->single, o->n, o->note);
    else if ((cf_sn.sq_on || cf_sn.ac_on) && cf_sn.sq_dn)
        name_set(cf_sn.sq_droot % 12u, cf_sn.sq_dmask, cf_sn.sq_bn ? cf_sn.sq_bass[cf_sn.sq_bn - 1u] % 12 : -1,
                 cf_sn.sq_dsingle, cf_sn.sq_dn, cf_sn.sq_dnote);
    else
        cu_name_live = 0;
}

/* a sound's number in its row, "007" */
static void snd_num(uint32_t idx, char *b)
{
    b[0] = (char)('0' + (idx + 1u) / 100u % 10u);
    b[1] = (char)('0' + (idx + 1u) / 10u % 10u);
    b[2] = (char)('0' + (idx + 1u) % 10u);
    b[3] = 0;
}
/* the top bar's sound: number, engine, name; the pattern's letter after it when the sound is that pattern's own
 * (0.5: PRESETS / SELECT then change the pattern's, not HOME's) */
static void draw_top_sound(uint32_t part, int xmax)   /* xmax: where the chips on the right begin */
{
    char num[4], l[2], nm[13];
    int x = 4, own, room;
    uint32_t cur = sq_cur_pat(), k;
    own = cur < SQ_NPAT && part < 3u && cfps.r[cur][part].own;
    l[0] = (char)('A' + cur % SQ_NPAT);
    l[1] = 0;
    snd_num(cf_snd_pos(part), num);
    x += fb_text(&CF_FONT_UI, x, Y_TOP + 1, num, FL_INK3, 1) + 5;
    /* the name as fits before the chips (ARP, SEQ, REC), the pattern's letter always after it; short of room the
     * engine's label goes first, then the name's end */
    str_cpy(nm, cf_snd[part].name, sizeof nm);
    room = xmax - x - (own ? 7 + fb_text_w(&CF_FONT_UI, l, 1) : 0);
    k = (uint32_t)fb_text_w(&CF_FONT_UI, cf_engine_name(cf_snd[part].engine), 1) + 5u;
    if (fb_text_w(&CF_FONT_UI, nm, 1) + (int)k <= room) {
        x += fb_text(&CF_FONT_UI, x, Y_TOP + 1, cf_engine_name(cf_snd[part].engine), C_SEC(1), 1) + 5;
        room -= (int)k;
    }
    for (k = str_len(nm); k && fb_text_w(&CF_FONT_UI, nm, 1) > room; )
        nm[--k] = 0;
    x += fb_text(&CF_FONT_UI, x, Y_TOP + 1, nm, FL_INK, 1) + 4;
    if (own) {
        fb_rect(x, Y_TOP + 5, 2, 2, FL_INK3);
        fb_text(&CF_FONT_UI, x + 5, Y_TOP + 1, l, C_ACC(0), 1);
    }
}

#include "cf_views.c"                           /* (the heads, the pager, the central pictures) */
#include "cf_motion.c"
#include "cf_stage.c"
#include "cf_acc_screen.c"
#include "cf_draft_screen.c"
#include "cf_lab_screen.c"
#include "cf_library_screen.c"
#include "cf_seqv.c"                            /* (the SEQUENCER screens' pictures) */
#include "cf_dmv.c"                             /* (the drum machine's, 0.6) */
static void draw_title(const char *t, const char *note);
/* ------------------------------------------------------------------- FX --- */
/* the selected effect's own little picture, x0..x0+w along, its foot on yb, at most h tall */
static void draw_fx_graph(uint32_t e, int x0, int yb, int w, int h)
{
    const uint8_t *v = fxp_active_chain()->v[e];
    uint8_t c = cfx_is_on(e) ? C_ACC(1) : FL_INK3, c2 = cfx_is_on(e) ? C_ACC(2) : FL_BG3;
    int x, px = -1, py = 0;
    fb_hline(x0, yb, w, FL_BG3);
    for (x = 0; x < w; x++) {
        int32_t y = 0;
        switch (e) {
        case CFX_FILTER: {                       /* the response: flat, the corner (a bump for the resonance), a slope */
            int cx = (int)v[1] * (w - 1) / 99, d = x - cx, bump = (int)v[2] * h / 160;
            int lp = d < 0 ? h * 2 / 3 : h * 2 / 3 - d * h / 28, a = d < 0 ? -d : d;
            if (a < 8)
                lp += bump * (8 - a) / 8;
            if (v[0] == 1)                       /* band pass: both sides fall */
                lp = h * 2 / 3 - a * h / 28 + (a < 8 ? bump * (8 - a) / 8 : 0);
            else if (v[0] == 2)                  /* high pass: the mirror */
                lp = d > 0 ? h * 2 / 3 : h * 2 / 3 + d * h / 28 + (a < 8 ? bump * (8 - a) / 8 : 0);
            y = lp < 0 ? 0 : lp > h ? h : lp;
            break;
        }
        default:
            break;
        }
        if (e == CFX_FILTER) {
            if (px >= 0)
                fb_line(x0 + px, yb - py, x0 + x, yb - (int)y, c);
            px = x;
            py = (int)y;
        }
    }
    if (e == CFX_REVERB || e == CFX_DELAY) {     /* bars: a tail decaying, or the echoes */
        int i, n = e == CFX_REVERB ? 56 : 12, step = e == CFX_REVERB ? 4 : 6 + (int)v[0] * 40 / 100;
        int32_t a = h * 256 * (50 + (int)v[2]) / 150, k = e == CFX_REVERB ? 205 + (int)v[1] * 45 / 100 : 80 + (int)v[1] * 17 / 10;
        for (i = 0; i < n && i * step < w; i++) {
            int bh = (int)(a >> 8);
            if (e == CFX_REVERB)                 /* (a grain of noise in the tail) */
                bh = bh * (12 + (int)((uint32_t)(i * 2654435761u) >> 29)) / 16;
            if (bh > 0)
                fb_rect(x0 + i * step, yb - bh, e == CFX_REVERB ? 2 : 3, bh, i ? c : C_ACC(0));
            a = a * k / 256;
        }
    } else if (e == CFX_DIST) {                  /* the transfer curve: tanh of the driven input */
        int32_t g = CFX_DRIVE[v[0] > 100u ? 100u : v[0]];
        for (x = 0; x < w; x++) {
            int32_t in = (x * 2 - w) * 32768 / w, y = softclip((int32_t)(((int64_t)in * g) >> 8));
            int yy = yb - h / 2 - (int)(y * (h / 2) / 32768);
            if (px >= 0)
                fb_line(x0 + px, py, x0 + x, yy, c);
            px = x;
            py = yy;
        }
    } else if (e == CFX_CHORUS || e == CFX_PHASER) {   /* the sweep: a sine moving with the rate */
        uint32_t ph = cf_art_ms() * (uint32_t)(20 + v[0] * 6) * 1000u;
        int amp = (h / 2 - 1) * (10 + (int)v[1]) / 110;
        for (x = 0; x < w; x++) {
            int yy = yb - h / 2 - (osc_sine(ph + (uint32_t)x * (e == CFX_PHASER ? 60000000u : 28000000u)) * amp) / 32768;
            if (px >= 0)
                fb_line(x0 + px, py, x0 + x, yy, x & 1 ? c : c2);
            px = x;
            py = yy;
        }
    }
}

/* Preset pages share FX's existing top bar and knob row. */
static void draw_fx_presets(void)
{
    char sub[32], text[32];
    fb_rect(0, Y_VIEW, FB_W, H_VIEW, FL_BG);
    if (fxpu.mode == FXP_BROWSE) {
        if (fxpu.pick < FXP_FACTORY_N) {
            char number[14];
            kfmt(sub, sizeof sub, FXP_GROUP[fxpu.pick / FXP_GROUP_SIZE]);
            str_cpy(sub + str_len(sub), "  ", 3);
            kint(number, fxpu.pick + 1, " / ");
            str_cpy(sub + str_len(sub), number, sizeof sub - str_len(sub));
            kint(number, FXP_FACTORY_N, "");
            str_cpy(sub + str_len(sub), number, sizeof sub - str_len(sub));
        }
        else kint(sub, fxpu.pick - FXP_FACTORY_N + 1, " / 16 user");
        draw_title("FX presets", sub);
        fb_text_c(&CF_FONT_UI, 120, Y_VIEW + 60, fxp_pick_name(), C_ACC(0), 1);
        fb_text_c(&CF_FONT_UI, 120, Y_VIEW + 84, !fxp_active_chain()->on ? "No FX" : fxp_active_bypass() ? "Chain bypassed" : "K2 clear  K3 undo", FL_INK2, 1);
        if (fxp_pending()) fb_text_c(&CF_FONT_UI, 120, Y_VIEW + 106, "Save pending: keep power on", C_ACC(0), 1);
        else if (fxp_protected) fb_text_c(&CF_FONT_UI, 120, Y_VIEW + 106, "User bank protected", C_ACC(0), 1);
        else fb_text_c(&CF_FONT_UI, 120, Y_VIEW + 106, "Turn SELECT to hear presets", FL_INK3, 1);
        fb_text_c(&CF_FONT_UI, 120, Y_VIEW + 126, "SAVE manage   K4 edit", FL_INK2, 1);
        static const char *const H[4] = {"SELECT", "audition", "SAVE", "manage"};
        draw_hints(H, 2);
    } else {
        static const char *const A[3] = {"Save new FX", "Replace user FX", "Delete user FX"};
        kint(sub, fxpu.slot + 1, " / 16");
        draw_title(A[fxpu.action], sub);
        memcpy(text, fxpu.name, FXP_NAME);
        for (unsigned i = 0; i < FXP_NAME - 1; i++) if (!text[i]) text[i] = ' ';
        text[FXP_NAME - 1] = 0;
        int x = (FB_W - fb_text_w(&CF_FONT_UI, text, 1)) / 2;
        fb_text(&CF_FONT_UI, x, Y_VIEW + 58, text, C_ACC(0), 1);
        text[fxpu.cursor] = 0;
        fb_rect(x + fb_text_w(&CF_FONT_UI, text, 1), Y_VIEW + 73, 7, 2, C_ACC(0));
        const char *target = fxp_bank.used & (1u << fxpu.slot) ? fxp_bank.slot[fxpu.slot].name : "Empty slot";
        fb_text_c(&CF_FONT_UI, 120, Y_VIEW + 87, target, FL_INK2, 1);
        fb_text_c(&CF_FONT_UI, 120, Y_VIEW + 107, "Hold SAVE 1 second", FL_INK3, 1);
        if (cu.save_hold) {
            unsigned t = cu.now - cu.save_hold; if (t > 1000) t = 1000;
            fb_rect(20, Y_VIEW + 126, (int)(t * 200 / 1000), 3, C_ACC(1));
        }
        static const char *const H[4] = {"SELECT", "slot", "FX", "back"};
        draw_hints(H, 2);
    }
}

/* FX: the chain as a row of tiles in its order (on: raised, off: dim), the sound running along it, the selected
 * effect named above and pictured below */
static void draw_fx(void)
{
    if (fxpu.mode) { draw_fx_presets(); return; }
    uint32_t sel = cu.fx_pos % CFX_N, e = fxp_active_chain()->order[sel] % CFX_N, i;
    int x0 = 8, y0 = Y_VIEW + 58, tw = 34, gap = 4, run = 6 * tw + 5 * gap;
    char sub[24];
    cf_scene_frame();
    draw_plant(Y_VIEW);
    fb_veil(0, Y_VIEW, FB_W, H_VIEW, FL_BG);   /* (the plant faint: the picture leads) */
    str_cpy(sub, e == CFX_FILTER ? CFX_FTYPE[fxp_active_chain()->v[e][0] % 3u] : e == CFX_REVERB ? CFX_RTYPE[fxp_active_chain()->v[e][0] % 3u] : "",
            sizeof sub);
    if (sub[0])
        str_cpy(sub + str_len(sub), "  ", 3);
    str_cpy(sub + str_len(sub), cfx_is_on(e) ? "on" : "off", 4);
    if (fxp_active_bypass()) str_cpy(sub, "Chain bypassed", sizeof sub);
    draw_title(CFX_NAME[e], sub);
    fb_hline(x0, y0 + 13, run, FL_BG3);          /* the signal's way, a dot running along it */
    {
        int d = (int)((cf_art_ms() / 10u) % (uint32_t)run);
        fb_rect(x0 + d, y0 + 12, 2, 3, C_ACC(1));
    }
    for (i = 0; i < CFX_N; i++) {
        uint32_t id = fxp_active_chain()->order[i] % CFX_N;
        int x = x0 + (int)i * (tw + gap), on = cfx_is_on(id);
        fb_rbox(x, y0, tw, 26, 2, on ? cf_panel(2) : cf_panel(1));
        if (i == sel)
            fb_rframe(x, y0, tw, 26, C_ACC(1));
        fb_text_c(&CF_FONT_UI, x + tw / 2, y0 + 3, CFX_SHORT[id], on ? FL_INK : FL_INK3, 1);
        fb_rect(x + tw / 2 - 3, y0 + 19, 6, 2, on ? C_ACC(1) : FL_BG3);
    }
    fb_rect(x0 + (int)sel * (tw + gap) + tw / 2 - 1, y0 + 29, 3, 2, C_ACC(1));   /* (under the selected one) */
    draw_fx_graph(e, x0, Y_VIEW + 122, run, 24);
    {
        static const char *const H[6] = {"SELECT", "pick", "ALGO", "move", "FX", "presets"};
        draw_hints(H, 3);
    }
}

/* ------------------------------------------------------------------ HOME --- */
/* the chord the Chord and Keyboard views show (HOME, cf_ui.c home_view), kept as the name is: the keys' chord while it
 * sounds (as voiced, low to high, the bass's notes with it), else the sequencer's CHORDS track's while it plays; it
 * stays after both let go */
static uint8_t hv_n, hv_note[CF_MAX_NOTES], hv_nb, hv_bass[2], hv_root, hv_single;
static uint32_t hv_mask;
static void hv_keep(uint32_t n, const uint8_t *note, uint32_t nb, const uint8_t *bass, uint32_t root, uint32_t mask,
                    int single)
{
    uint32_t i;
    hv_n = (uint8_t)(n < CF_MAX_NOTES ? n : CF_MAX_NOTES);
    for (i = 0; i < hv_n; i++)
        hv_note[i] = note[i];
    hv_nb = (uint8_t)(nb < 2u ? nb : 2u);
    for (i = 0; i < hv_nb; i++)
        hv_bass[i] = bass[i];
    hv_root = (uint8_t)(root % 12u);
    hv_mask = mask;
    hv_single = (uint8_t)single;
}
static void hv_update(void)
{
    const cf_chord_out_t *o = &cf_sn.out;
    if (o->sounding && o->n)
        hv_keep(o->n, o->note, cs.bass_on ? cf_sn.nb : 0u, cf_sn.bass, o->root_pc, o->mask, o->single);
    else if ((cf_sn.sq_on || cf_sn.ac_on) && cf_sn.sq_dn)
        hv_keep(cf_sn.sq_dn, cf_sn.sq_dnote, cf_sn.sq_bn, cf_sn.sq_bass, cf_sn.sq_droot, cf_sn.sq_dmask,
                cf_sn.sq_dsingle);
}
/* a note with its octave, "F#4" (signs as the UI face draws them; raw: '#' and 'b' as typed, for the micro face,
 * which has no signs) */
static void hv_note_name_raw(uint32_t n, char *b, int raw)
{
    char a[4], t[8];
    uint32_t k;
    int o = (int)n / 12 - 1;
    cf_pc_name((int)(n % 12u), &cu_spell, a);
    k = str_len(a);
    a[k] = 0;
    str_cpy(t, a, sizeof t);
    k = str_len(t);
    if (o < 0)
        t[k++] = '-', o = -o;
    t[k++] = (char)('0' + o % 10);
    t[k] = 0;
    if (raw)
        str_cpy(b, t, 8);
    else
        fb_signs(t, b, 8);
}
#define hv_note_name(n, b) hv_note_name_raw((n), (b), 0)
/* the arpeggiator's note now (it hops along the notes, as ChoralRoot's): the key it plays (hv_arp_at), or, in the
 * Chord view's row, the chord tone it plays in any octave (hv_arp_pc: OCTAVE 2..4 plays the tones higher up). Not
 * for Strum, as ChoralRoot: its notes all sound after the strum, the last one is no more "now" than the others */
static int hv_arp_live(void)
{
    return cf_sn.arp_on && cf_sn.out.sounding && !CF_ARP_STRUMMING(cs.arp.mode);
}
static int hv_arp_at(uint32_t n)
{
    return hv_arp_live() && cf_sn.arp_note == n;
}
static int hv_arp_pc(uint32_t n)
{
    return hv_arp_live() && cf_sn.arp_note % 12u == n % 12u;
}

/* Chord: the plant faint, the name big in the middle (the root large, the rest small at its top, the scale degree
 * over it), the voiced notes in a row under it with each one's degree under it, the bass's under them */
static void draw_home_chord(void)
{
    char b[20], r[4], nm[8];
    int n, w, x, y = Y_VIEW + 44, i, cnt = hv_n, gap, sc;
    fb_veil(0, Y_VIEW, FB_W, H_VIEW, FL_BG);
    if (!cu_name[0]) {
        fb_text_c(&CF_FONT_UI, 120, Y_VIEW + 58, "play a chord", FL_INK3, 1);
        return;
    }
    /* the name as HOME writes it, twice its size: the root 4x, what follows it 2x on the root's top, white; the scale
     * degree after it in grey (draw_chord_name's proportions); single notes all at 3x */
    n = cu_name_single ? (int)str_len(cu_name) : 1 + (cu_name[0] && (cu_name[1] == '#' || cu_name[1] == 'b'));
    str_cpy(r, cu_name, (uint32_t)(n < 3 ? n : 3) + 1u);
    if (cu_name_single) {
        fb_signs(cu_name, b, sizeof b);
        sc = fb_text_w(&CF_FONT_UI, b, 3) <= 224 ? 3 : 2;
        fb_text_c(&CF_FONT_UI, 120, y - 3 * sc, b, FL_INK, sc);
    } else {
        char tail[20], deg[12];
        int wr, wt, wd, ts = 2;
        fb_signs(r, b, sizeof b);
        tail[0] = deg[0] = 0;
        if (cu_name[n])
            fb_signs(cu_name + n, tail, sizeof tail);
        if (cu_name_sub[0])
            fb_signs(cu_name_sub, deg, sizeof deg);
        sc = 4;
        wr = fb_text_w(&CF_FONT_UI, b, sc);
        for (;;) {                               /* (a long name: the tail and the degree at 1x) */
            wt = tail[0] ? 2 + fb_text_w(&CF_FONT_UI, tail, ts) : 0;
            wd = deg[0] ? 6 * ts + fb_text_w(&CF_FONT_UI, deg, ts) : 0;
            if (wr + wt + wd <= 228 || ts == 1)
                break;
            ts = 1;
        }
        x = 120 - (wr + wt + wd) / 2;
        x += fb_text_halo(&CF_FONT_UI, x, y - 3 * sc, b, FL_INK, FL_BG, sc);
        if (tail[0])
            x += 2 + fb_text_halo(&CF_FONT_UI, x + 2, y - 3 * ts, tail, FL_INK, FL_BG, ts);
        if (deg[0])
            fb_text_halo(&CF_FONT_UI, x + 6 * ts, y - 3 * ts, deg, FL_INK2, FL_BG, ts);
    }
    /* the notes, low to high: name over, degree under; the arpeggiator's now lit */
    if (cnt > 8)
        cnt = 8;
    for (i = 0, w = 0; i < cnt; i++) {
        hv_note_name(hv_note[i], nm);
        w += fb_text_w(&CF_FONT_UI, nm, 1);
    }
    gap = cnt > 1 ? (200 - w) / (cnt - 1) : 0;
    if (gap > 22)
        gap = 22;
    if (gap < 4)
        gap = 4;
    x = 120 - (w + gap * (cnt - 1)) / 2;
    for (i = 0; i < cnt; i++) {
        int nw, hot = hv_arp_pc(hv_note[i]);
        hv_note_name(hv_note[i], nm);
        nw = fb_text_w(&CF_FONT_UI, nm, 1);
        fb_text_halo(&CF_FONT_UI, x, Y_VIEW + 84, nm, hot ? C_ACC(0) : FL_INK, FL_BG, 1);
        if (hot)
            fb_rect(x, Y_VIEW + 98, nw, 2, C_ACC(0));
        if (!hv_single) {
            const char *dg = cv_degree((int)hv_note[i] - (int)hv_root + 120, hv_mask);
            fb_text_halo(&CF_FONT_UI, x + (nw - fb_text_w(&CF_FONT_UI, dg, 1)) / 2, Y_VIEW + 100, dg, FL_INK3, FL_BG, 1);
        }
        x += nw + gap;
    }
    if (hv_nb) {                                 /* the bass: what it plays now */
        char t[24];
        str_cpy(t, "bass ", sizeof t);
        for (i = 0; i < hv_nb; i++) {
            hv_note_name(hv_bass[i], nm);
            str_cpy(t + str_len(t), nm, (uint32_t)(sizeof t - str_len(t)));
            if (i + 1 < hv_nb)
                str_cpy(t + str_len(t), "  ", 3);
        }
        fb_text_halo(&CF_FONT_UI, 120 - fb_text_w(&CF_FONT_UI, t, 1) / 2, Y_VIEW + 120, t, C_SEC(1), FL_BG, 1);
    }
}

/* Keyboard: the name as on the flower, five octaves of keys (C1 .. B5; moved up or down when OCT or the bass's
 * OCTAVE take the notes past them, the chord's kept first) with the chord's notes lit in the accent, the bass's in the
 * second colour, the arpeggiator's now in white, each lit note named under its key */
static uint8_t hv_key_mark(int n)
{
    int i;
    for (i = 0; i < hv_nb; i++)
        if (n == hv_bass[i])
            return C_SEC(1);
    if (hv_arp_at((uint32_t)n))
        return FL_INK;
    for (i = 0; i < hv_n; i++)
        if (n == hv_note[i])
            return hv_arp_at((uint32_t)n) ? FL_INK : C_ACC(0);
    return 0;
}
static void draw_home_keys(void)
{
    const int octs = 5, wk = 6, x0 = 120 - octs * 7 * wk / 2, ky = Y_VIEW + 62, kh = 44;
    char nm[8];
    int i, o, lo = 24, mn = 127, mx = 0, xs[CF_MAX_NOTES + 2], rows[CF_MAX_NOTES + 2], nl = 0;
    for (i = 0; i < hv_nb + hv_n; i++) {         /* the window: C1 .. B5 if the notes are in it, else from the lowest
                                                  * note's C, or up to the highest's octave (the chord kept) */
        int n = i < hv_nb ? hv_bass[i] : hv_note[i - hv_nb];
        mn = n < mn ? n : mn;
        mx = n > mx ? n : mx;
    }
    if (mx && (mn < lo || mx >= lo + octs * 12)) {
        lo = mn / 12 * 12;
        if (mx >= lo + octs * 12)
            lo = (mx / 12 - (octs - 1)) * 12;
        lo = lo < 0 ? 0 : lo > 127 - octs * 12 + 1 ? (127 - octs * 12 + 1) / 12 * 12 : lo;
    }
    fb_veil(0, Y_VIEW, FB_W, H_VIEW, FL_BG);
    if (cu_name[0])
        draw_chord_name(6, Y_VIEW + 7, cu_name, cu_name_sub, cu_name_single);
    else
        fb_text(&CF_FONT_UI, 6, Y_VIEW + 4, "play a chord", FL_INK3, 1);
    fb_rect(x0 - 2, ky - 2, octs * 7 * wk + 3, kh + 4, FL_BG);
    draw_piano(x0, ky, lo, octs, wk, kh, hv_key_mark);
    for (o = 0; o < octs; o++) {                 /* the octaves, over the keys */
        int on = lo / 12 - 1 + o;
        char b[4] = {'C', on < 0 ? '-' : (char)('0' + on), on < 0 ? (char)('0' - on) : 0, 0};
        fb_text(&CF_FONT_MICRO, x0 + o * 7 * wk + 1, ky - 8, b, FL_INK3, 1);
    }
    for (i = 0; i < hv_nb + hv_n; i++) {         /* the lit notes named under their keys (stepped down when close) */
        int n = i < hv_nb ? hv_bass[i] : hv_note[i - hv_nb], cx;
        if (n < lo || n >= lo + octs * 12)
            continue;
        int row = 0, k, busy;
        for (k = 0; k < i && (k < hv_nb ? hv_bass[k] : hv_note[k - hv_nb]) != n; k++)
            ;
        if (k < i)
            continue;                            /* (the bass on a chord tone's key: named once) */
        cx = piano_cx(x0, lo, wk, n);
        hv_note_name_raw((uint32_t)n, nm, 1);
        do {                                     /* the first row with no label near this one */
            for (k = 0, busy = 0; k < nl; k++)
                busy |= rows[k] == row && cx - xs[k] < 16 && xs[k] - cx < 16;
            row += busy;
        } while (busy && row < 3);
        xs[nl] = cx;
        rows[nl++] = row;
        {
            int tw = fb_text_w(&CF_FONT_MICRO, nm, 1);
            fb_text_halo(&CF_FONT_MICRO, cx + 1 - tw / 2, ky + kh + 4 + row * 8, nm, i < hv_nb ? C_SEC(1) : FL_INK,
                         FL_BG, 1);
        }
    }
}

#include "cf_diag_view.c"

static void draw_home(void)
{
    char key[16], k2[8];
    uint32_t view = home_view();
    if (view == HV_FLOWER && cf_scene() == CF_SCENE_PROGRESSIONS) { view_progressions(12); return; }
    cf_scene_frame();
    draw_plant(Y_VIEW);
    if(view==HV_DIAG) {draw_home_diag();return;}
    name_update();
    hv_update();
    if (view == HV_CHORD)
        draw_home_chord();
    else if (view == HV_KEYS)
        draw_home_keys();
    else if (cu_name[0])
        draw_chord_name(6, Y_VIEW + 7, cu_name, cu_name_sub, cu_name_single);   /* (C MAJ's caps are at + 7) */
    /* the key in the corner */
    if (cs.chord.quantize != CF_Q_OFF) {
        kpc(k2, cs.chord.tonic);
        str_cpy(key, k2, sizeof key);
        str_cpy(key + str_len(key), " ", 2);
        str_cpy(key + str_len(key), CF_SCALES[cs.chord.scale].shrt, 8);
    } else {
        str_cpy(key, "CHROMATIC", sizeof key);
    }
    fb_text_halo(&CF_FONT_UI, 234 - fb_text_w(&CF_FONT_UI, key, 1), Y_VIEW + 4, key, FL_INK2, FL_BG, 1);
    if (cs.chord.octave) {
        char b[8];
        b[0] = 'O';
        b[1] = 'C';
        b[2] = 'T';
        b[3] = ' ';
        b[4] = cs.chord.octave > 0 ? '+' : '-';
        b[5] = (char)('0' + (cs.chord.octave > 0 ? cs.chord.octave : -cs.chord.octave));
        b[6] = 0;
        fb_text_halo(&CF_FONT_UI, 234 - fb_text_w(&CF_FONT_UI, b, 1), Y_VIEW + 18, b, C_ACC(1), FL_BG, 1);
    }
    if (cf_sn.running) {                       /* the beat */
        int i;
        for (i = 0; i < cs.beats; i++) {
            int on = i == (cf_sn.beat + cs.beats - 1) % cs.beats;
            fb_rect(234 - (cs.beats - i) * 6, Y_VIEW + H_VIEW - 8, 4, 4, on ? C_ACC(0) : FL_BG3);
        }
    }
}

/* ------------------------------------------------------- the other screens --- */
static void draw_title(const char *t, const char *note)   /* a head with no pages (cf_views.c) */
{
    draw_head(t);
    draw_head_note(note);
}
/* the picture of a paged screen, sliding in after a turn (cv_slide), clipped under the head */
static void draw_view(void (*view)(int))
{
    fb_clip(0, VIEW_Y - 8, FB_W, Y_VIEW + H_VIEW);
    view(12 + cv_slide());
    fb_clip(0, Y_VIEW, FB_W, Y_VIEW + H_VIEW);
}

/* SEL: the chord button's chord on the pitch-class clock, the bass degree marked */
static void draw_sel(void)
{
    const cf_form_t *f = sel_form();
    char lab[8], b[24];
    cf_scene_frame();
    draw_plant(Y_VIEW);
    fb_veil(0, Y_VIEW, FB_W, H_VIEW, FL_BG);   /* (the plant faint: the picture leads) */
    if (cf_is_off(&cs.chord, cu.sel_btn)) {
        draw_title("HOLD", "Holds notes after you let go");
    } else if (ch.mode == CF_H_AUTO) {
        draw_title(cf_modifier_name(ch.slot[cu.sel_btn]), "Auto modifier");
    } else {                                     /* the button as its key shows it; its chord in words */
        cf_form_label(f, lab);
        str_cpy(b, cf_type_name(f->type), sizeof b);
        if (f->add) {
            str_cpy(b + str_len(b), " + ", 4);
            str_cpy(b + str_len(b), cf_adds_name(cf_adds_index(f->add)), 10);
        }
        draw_title(lab, b);
    }
    draw_view(view_sel);
    draw_sel_pointer();
    {
        static const char *const H[2] = {"SELECT", "button"};
        draw_hints(H, 1);
    }
}

/* ENV: the envelope's shape, drawn over the plant */
static void draw_env(void)
{
    static const char *const TGT[4] = {"Chord + bass", "Chord", "Bass", "Layer"};
    int a = cf_env_get(env_part(), 0), d = cf_env_get(env_part(), 1), sl = cf_env_get(env_part(), 2);
    int r = cf_env_get(env_part(), 3), x0 = 16, yb = Y_VIEW + H_VIEW - 14, yt = Y_VIEW + 50;
    int xa = x0 + 8 + a * 44 / 99, xd = xa + 8 + d * 44 / 99, xs = xd + 40, xr = xs + 8 + r * 50 / 99;
    int ys = yb - (yb - yt) * sl / 99;
    cf_scene_frame();
    draw_plant(Y_VIEW);
    fb_veil(0, Y_VIEW, FB_W, H_VIEW, FL_BG);   /* (the plant faint: the picture leads) */
    draw_head(TGT[cu.env_tgt % 4u]);
    draw_pager((int)(cu.env_tgt % 4u), 4, 1, 0);
    fb_hline(x0, yb, 210, FL_BG3);
    fb_line(x0, yb, xa, yt, C_ACC(1));
    fb_line(xa, yt, xd, ys, C_ACC(1));
    fb_line(xd, ys, xs, ys, C_ACC(1));
    fb_line(xs, ys, xr, yb, C_ACC(1));
    fb_rect(xa - 1, yt - 1, 3, 3, C_ACC(0));
    fb_rect(xd - 1, ys - 1, 3, 3, C_ACC(0));
    fb_rect(xs - 1, ys - 1, 3, 3, C_ACC(0));
    {
        static const char *const H[2] = {"SELECT", "sound"};   /* (chord + bass, chord, bass, the LAYER) */
        draw_hints(H, 1);
    }
}

/* EDIT: the sound, the section and its page; FM: the six operators, the carriers below */
static void draw_edit(void)
{
    uint32_t part = cu.edit_part, np = cfe_npages(part, cu.edit_sec);
    char t[24];
    cf_scene_frame();
    draw_plant(Y_VIEW);
    fb_veil(0, Y_VIEW, FB_W, H_VIEW, FL_BG);   /* (the plant faint: the picture leads) */
    /* the title: the section (FM: "OP 1", its page's name by the pager), or the VA's page ("OSC 1", its second page
     * the same name). The sound edited: in the top bar, after SOUND (cf_ui_draw) */
    if (cfe_fm6(part)) {
        const char *k = cfe_key_name(part, cu.edit_sec);
        static const char *const FMSEC[2] = {"Pitch", "Voice"};
        if (cu.edit_sec < 6u) {
            str_cpy(t, "OP ", sizeof t);
            t[3] = k[2];
            t[4] = 0;
        } else {
            str_cpy(t, FMSEC[(cu.edit_sec - 6u) & 1u], sizeof t);
        }
        draw_head(t);
        {                                        /* the page's name, without the title's word ("Pitch Rate": Rate) */
            const char *pn = cfe_page_title(part, cu.edit_sec, cu.page[SCR_EDIT]);
            uint32_t n = str_len(t), i;
            for (i = 0; i < n && pn[i] == t[i]; i++)
                ;
            if (i == n && (!pn[n] || pn[n] == ' '))
                pn += pn[n] ? n + 1u : n;
            draw_pager(cu.page[SCR_EDIT], (int)np, 0, pn);
        }
    } else {
        uint32_t n;
        str_cpy(t, cfe_page_title(part, cu.edit_sec, cu.page[SCR_EDIT]), sizeof t);
        n = str_len(t);
        if (n && t[n - 1] == '+')
            t[n - 1] = 0;
        draw_head(t);
        draw_pager(cu.page[SCR_EDIT], (int)np, 0, 0);
#if FELUCCA_VA
        if (cfe_va(part))
            draw_view(view_va);
#endif
    }
    if (cfe_fm6(part))
        draw_view(view_fm);
    {
        static const char *const H[4] = {"SELECT", "page", "KEYS", "section"};
        draw_hints(H, 2);
    }
}

#include "cf_variation_view.c"

static void draw_generic(void)
{
    static const char *const PG_GLO[CF_GLO_NPAGES] = {"Display", "MIDI", "Channels", "Play", "Reset", "Library", "Diagnostics", "Harmony", "Variation", "Accompany", "Browse", "Recordings", "Legacy tools"};
    cf_scene_frame();
    draw_plant(Y_VIEW);
    fb_veil(0, Y_VIEW, FB_W, H_VIEW, FL_BG);   /* (the plant faint: the picture leads) */
    switch (cu.scr) {
    case SCR_GLO: {
        static const char *const H[2] = {"SELECT", "page"};
        static void (*const V[7])(int) = {view_display, view_midi, view_midi, view_play, view_reset, view_library, view_diagnostics};   /* (MIDI and
                                                                                               * Channels: one picture) */
        uint32_t p = cu.page[SCR_GLO] % CF_GLO_NPAGES;
        draw_head(PG_GLO[p]);
        draw_pager((int)p, CF_GLO_NPAGES, 1, 0);
        if(p==CF_GLO_LEGACY) { fb_text_c(&CF_FONT_UI,120,Y_VIEW+68,"K1 selects / SEL opens",FL_INK,1); }
        else if (p == CF_GLO_RECORDINGS) {
            fb_text_c(&CF_FONT_UI,120,Y_VIEW+55,"KEEP THE LATEST THREE",C_ACC(0),1);
            fb_text_c(&CF_FONT_UI,120,Y_VIEW+77,"Newest recording is always 1",FL_INK,1);
            fb_text_c(&CF_FONT_MICRO,120,Y_VIEW+101,sq_pref.protect?"FULL: CLEAR A RECORDING FIRST":"A FINISHED TAKE REPLACES THE OLDEST",FL_INK2,1);
            fb_text_c(&CF_FONT_MICRO,120,Y_VIEW+121,"CANCEL KEEPS YOUR PREVIOUS TAKES",FL_INK2,1);
        }
        else if (p == CF_GLO_VARIATION) {
            cvr_draw();
        }
        else if (p == CF_GLO_BROWSE) {
            unsigned idea=cpb_target(); char b[64];
            kint(b,idea+1," / "); kint(b+str_len(b),cf_progression_count(),0);
            if(cf_browser_favorite(&cpb,idea))str_cpy(b+str_len(b),"  FAVORITE",sizeof b-str_len(b));
            fb_text_c(&CF_FONT_UI,120,Y_VIEW+42,b,C_ACC(0),1);
            fb_text_c(&CF_FONT_UI,120,Y_VIEW+61,cf_progression_get(idea)->name,FL_INK,1);
            fb_text_c(&CF_FONT_MICRO,120,Y_VIEW+78,cf_progression_get(idea)->context,FL_INK2,1);
            fb_text_c(&CF_FONT_UI,120,Y_VIEW+95,CPB_ORDER_NAME[cpb.mode],C_ACC(1),1);
            fb_text_c(&CF_FONT_MICRO,120,Y_VIEW+113,"K1: TENS / K2: ONES",FL_INK2,1);
            fb_text_c(&CF_FONT_MICRO,120,Y_VIEW+126,"SELECT: BROWSE / SAVE: FAVORITE",FL_INK2,1);
        }
        else if (p == CF_GLO_ACCOMP) {
            fb_text_c(&CF_FONT_UI, 120, Y_VIEW + 55, CPA_MODE_NAME[cpg.mode], C_ACC(0), 1);
            fb_text_c(&CF_FONT_UI, 120, Y_VIEW + 77, "PLAY: start / stop accompaniment", FL_INK2, 1);
            fb_text_c(&CF_FONT_MICRO, 120, Y_VIEW + 97, "CHANGES AT THE NEXT BAR", FL_INK3, 1);
        }
        else if (p == CF_GLO_HARMONY) {
            fb_text_c(&CF_FONT_UI, 120, Y_VIEW + 65, ch.mode ? "AUTO HARMONY" : "MANUAL HARMONY", C_ACC(0), 1);
            fb_text_c(&CF_FONT_UI, 120, Y_VIEW + 83,
                cf_harmony_supported(cs.chord.scale, cs.chord.quantize) ? "HOLD sustains; Lock latches" : "Auto: choose a 7-note scale", FL_INK2, 1);
        } else draw_view(V[p]);
        if(p==CF_GLO_BROWSE) { static const char *const BH[2]={"HOME","back"}; draw_hints(BH,1); }
        else draw_hints(H, 1);
        break;
    }
    case SCR_BASS: {
        char b[16], a[4];
        int i;
        b[0] = 0;
        for (i = 0; i < cf_sn.nb && i < 2; i++) {   /* the bass's notes now, "C2  C3" */
            cf_pc_name(cf_sn.bass[i] % 12u, &cu_spell, a);
            str_cpy(b + str_len(b), a, 4);
            kint(b + str_len(b), cf_sn.bass[i] / 12 - 1, 0);
            if (i + 1 < cf_sn.nb)
                str_cpy(b + str_len(b), "  ", 3);
        }
        draw_title(cf_snd[CF_PART_BASS].name, b);
        draw_view(view_bass);
        break;
    }
    case SCR_SEQ: {
        char b[12], s2[8];
        kint(b, cs.bpm, " BPM");
        kint(s2, cs.beats, 0);
        str_cpy(s2 + str_len(s2), cs.beats == 6 ? "/8" : "/4", 4);
        draw_title(b, s2);
        /* where SEQ goes from here (0.6: the SEQUENCER or the rhythm machine): two cards under the head (rc6) */
        dm_draw_carousel();
        {
            static const char *const H[2] = {"PLAY", "start"};   /* (the click sounds only while the beat runs) */
            static const char *const H2[2] = {"PLAY", "stop"};
            draw_hints(cf_sn.running ? H2 : H, 1);
        }
        break;
    }
    case SCR_ARP: {                              /* the mode; Pattern and Feel; the pattern pictured */
        static const char *const PG[2] = {"Pattern", "Feel"};
        static const char *const H[2] = {"SELECT", "page"};
        draw_head(CF_ARP_NAME[cs.arp.mode % CF_ARP_NMODES]);
        draw_pager(cu.page[SCR_ARP] & 1, 2, 1, PG[cu.page[SCR_ARP] & 1]);
        fb_text_r(&CF_FONT_UI, 234, HEAD_LINE, cu.arp_on ? (cf_grid_shown() ? "on the beat" : "on") : "off",
                  cu.arp_on ? C_ACC(1) : FL_INK3, 1);
        draw_view(view_arp);
        draw_hints(H, 1);
        break;
    }
    case SCR_SAVE: {                             /* the name being given, its letter under edit; Chord / Bass / Layer */
        static const char *const PG[3] = {"Chord", "Bass", "Layer"};
        static const char *const H[4] = {"SELECT", "part", "SAVE", "hold"};
        uint32_t part = cf_save_part();
        char pre[13];
        draw_head(cu.save_name[0] ? cu.save_name : " ");
        str_cpy(pre, cu.save_name, (uint32_t)cu.save_cur + 1u);
        fb_rect(6 + fb_text_w(&CF_FONT_UI, pre, 2), HEAD_Y + 25, 13, 2, C_ACC(0));   /* (under the letter K3 sets) */
        draw_pager((int)part, 3, 1, PG[part]);
        if (cu.edited[part])
            fb_text_r(&CF_FONT_UI, 234, HEAD_LINE, "edited", C_ACC(1), 1);
        draw_view(view_save);
        draw_hints(H, 2);
        break;
    }
    default: break;
    }
}

/* ---------------------------------------------------------------- popups --- */
static void draw_pop_sound(uint32_t part)
{
    int x = 16, y = Y_VIEW + 26, w = 208, h = 94, i;
    uint32_t n = cf_snd_count(part), pos = cf_snd_pos(part);
    char a[4], b[4], t[12];
    draw_panel(x, y, w, h);
    fb_text(&CF_FONT_UI, x + 8, y + 5, part == 2u ? "LAYER SOUND" : part ? "BASS SOUND" : "CHORD SOUND", C_ACC(0), 1);
    snd_num(pos, a);
    snd_num(n ? n - 1u : 0u, b);
    str_cpy(t, a, sizeof t);
    str_cpy(t + 3, " / ", 4);
    str_cpy(t + 6, b, 4);
    fb_text_r(&CF_FONT_UI, x + w - 8, y + 5, t, FL_INK2, 1);
    for (i = -2; i <= 2; i++) {
        uint32_t idx = n ? (pos + n * 2u + (uint32_t)(i + 2) - 2u) % n : 0u, e, pp;
        char nm[13], num[4];
        int yy = y + 22 + (i + 2) * 14;
        cf_snd_at(part, idx, &e, &pp);
        pool_name(e, pp, nm);
        snd_num(idx, num);
        if (!i)
            fb_rect(x + 4, yy - 1, w - 8, 14, FL_BG2);
        if (i && idx == 0u)                      /* the row starts again here */
            fb_hline(x + 8, yy - 2, w - 16, C_ACC(3));
        fb_text(&CF_FONT_UI, x + 8, yy, num, i ? FL_INK3 : FL_INK2, 1);
        fb_text(&CF_FONT_UI, x + 38, yy, cf_engine_name(e), i ? FL_INK3 : C_SEC(0), 1);
        fb_text(&CF_FONT_UI, x + 64, yy, nm, i ? FL_INK2 : FL_INK, 1);
    }
}

static void draw_pop_algo(void)
{
    int x = 14, y = Y_VIEW + 36, w = 212, h = 80, k;
    draw_panel(x, y, w, h);
    fb_text(&CF_FONT_UI, x + 8, y + 5, "CHORD ALGORITHM", C_ACC(0), 1);
    fb_text_r(&CF_FONT_UI, x + w - 8, y + 5, ALG_NAME[cs.chord.algo], FL_INK, 1);
    /* the keyboard in small: chord buttons in the accent, roots in grey */
    for (k = 0; k < CF_NKEYS; k++) {
        int kx, ky, kw, kh, role = cf_key_role(cs.chord.algo, k, 0);
        key_box(k, &kx, &ky, &kw, &kh);
        kx = x + 15 + kx * 3 / 4;
        ky = y + 30 + (ky - Y_KEYS) * 3 / 4;
        fb_rbox(kx, ky, kw * 3 / 4, kh * 3 / 4, 1, role == CF_ROLE_BUTTON ? C_ACC(1) : FL_BG3);
    }
    fb_text(&CF_FONT_UI, x + 8, y + h - 18, cs.chord.algo & 1 ? "buttons latch" : "hold the buttons", FL_INK2, 1);
}

static void draw_popup(void)
{
    switch (cu.pop) {
    case POP_SOUND_CHORD: draw_pop_sound(CF_PART_CHORD); break;
    case POP_SOUND_BASS: draw_pop_sound(CF_PART_BASS); break;
    case POP_SOUND_LAYER: draw_pop_sound(CF_PART_LAYER); break;
    case POP_ALGO: draw_pop_algo(); break;
    case POP_OCTAVE: {
        char b[12];
        draw_panel(60, Y_VIEW + 52, 120, 44);
        str_cpy(b, "OCT ", 12);
        b[4] = cs.chord.octave > 0 ? '+' : cs.chord.octave < 0 ? '-' : ' ';
        b[5] = (char)('0' + (cs.chord.octave < 0 ? -cs.chord.octave : cs.chord.octave));
        b[6] = 0;
        fb_text_c(&CF_FONT_UI, 120, Y_VIEW + 61, b, FL_INK, 2);
        break;
    }
    case POP_MSG: {                              /* one line, or two (a '\n'); a button's name as a chip */
        char a[64];
        int i;
        str_cpy(a, cu.msg, sizeof a);
        for (i = 0; a[i] && a[i] != '\n'; i++)
            ;
        if (a[i]) {
            a[i] = 0;
            draw_panel(14, Y_VIEW + 50, 212, 48);
            draw_chip_text_c(Y_VIEW + 59, a, FL_INK);
            draw_chip_text_c(Y_VIEW + 75, a + i + 1, FL_INK);
        } else {
            draw_panel(20, Y_VIEW + 56, 200, 36);
            draw_chip_text_c(Y_VIEW + 67, a, FL_INK);
        }
        break;
    }
    default: break;
    }
}

/* -------------------------------------------------------------- the frame --- */
static int cf_intro_draw(uint32_t dt);
static void cf_legacy_frame(void)
{
    char top[24];
    fb_unclip();
    if (cf_intro_draw(cu.frame_dt)) { cf_stage_valid=cf_acc_valid=0; return; }
    if(!cu.upd_left && cu.scr==SCR_LAB){cfl_draw();return;}
    cfl_valid=0;
    if(!cu.upd_left && cu.scr==SCR_PEDIT){cfd_draw();return;}
    cfd_valid=0;
    if(!cu.upd_left && cu.scr==SCR_SONGS){csl_draw();return;}
    csl_screen_valid=0;
    if(!cu.upd_left && (cu.scr==SCR_ACC || (cu.scr==SCR_GLO && cu.page[SCR_GLO]==CF_GLO_LEGACY))) {cf_acc_draw();return;}
    cf_acc_valid=0;
    if (cu.scr == SCR_HOME && home_view() == HV_FLOWER && cf_scene() == CF_SCENE_PROGRESSIONS &&
        !cu.pop && !cu.upd_left) { cf_stage_draw(); return; }
    cf_stage_valid = 0; cf_stage_energy = 0; cf_stage_tick = cu.now;
    if (cu.scr == SCR_HOME) {
        /* the layout's name, and a padlock for the Lock ones */
        const char *an = ch.mode == CF_H_AUTO ?
            (cf_harmony_supported(cs.chord.scale, cs.chord.quantize) && !cf_sn.out.fallback ? "AUTO" : "AUTO?") :
            cs.chord.algo >= CF_ALG_SIDE ? "Side" : "Top";
        int lk = cs.chord.algo == CF_ALG_TOP_LOCK || cs.chord.algo == CF_ALG_SIDE_LOCK, xr = lk ? 195 : 206;
        draw_topbar("", 0);
        fb_text_r(&CF_FONT_UI, xr, Y_TOP + 1, an, FL_INK3, 1);
        if (lk)
            draw_lock(xr + 3, Y_TOP + 3, FL_INK3);
        {
            int x = xr - fb_text_w(&CF_FONT_UI, an, 1) - 6;
            if (cu.arp_on) {                     /* the arpeggiator plays: a chip before the algorithm */
                int w = fb_text_w(&CF_FONT_UI, "ARP", 1) + 6;
                x -= w;
                fb_rbox(x, Y_TOP + 2, w, 11, 2, C_ACC(1));
                fb_text(&CF_FONT_UI, x + 3, Y_TOP + 1, "ARP", FL_BG, 1);
                x -= 4;
            }
            {                                    /* the plaque: what plays (a pattern, the song, a rhythm, a take: its
                                                  * count-in, then its bar of 8); nothing playing, pale, what PLAY
                                                  * would start (0.6, the owner: "Показывать") */
                char b[24];
                uint32_t kind = dm_plaque(b, sizeof b);
                if (kind != DMP_NONE)
                    x = dm_plaque_chip(x, b, kind) - 4;
            }
            draw_top_sound(CF_PART_CHORD, x);    /* (after the chips: the name fits before them) */
        }
        (void)top;
    } else {
        draw_topbar(cu.scr == SCR_SQED ? (squ.trk == SQ_DRUMS ? "DRUMS" : squ.trk == SQ_BASS ? "BASS" :
                                          squ.trk == SQ_LAYER ? "LAYER" : "CHORDS") :
                    SCR_NAME[cu.scr], 0);
        if (cu.scr == SCR_EDIT)                  /* the sound edited, after SOUND */
            fb_text(&CF_FONT_UI, 4 + fb_text_w(&CF_FONT_UI, SCR_NAME[cu.scr], 1) + 7, Y_TOP + 1,
                    cf_snd[cu.edit_part].name, FL_INK3, 1);
    }
    cu_hints = 0;                                /* (the screen names its hints while it draws: draw_hints) */
    fb_clip(0, Y_VIEW, FB_W, Y_VIEW + H_VIEW);
    if (cu.scr == SCR_HOME)
        draw_home();
    else if (cu.scr == SCR_SEL)
        draw_sel();
    else if (cu.scr == SCR_ENV)
        draw_env();
    else if (cu.scr == SCR_EDIT)
        draw_edit();
    else if (cu.scr == SCR_FX)
        draw_fx();
    else if (sq_screen(cu.scr))
        draw_seq();
    else if (cu.scr == SCR_DRUM)
        draw_drum();
    else
        draw_generic();
    draw_popup();
    fb_unclip();
    if (cu.scr == SCR_SQED) {                    /* the open track and its sound (PRESETS turns it) */
        const char *tn = squ.trk == SQ_DRUMS ? "DRUMS" : squ.trk == SQ_BASS ? "BASS" : squ.trk == SQ_LAYER ? "LAYER" :
                         "CHORDS";
        const char *sn = squ.trk == SQ_DRUMS ? sq_kit_name(sq_cur()) : cf_snd[squ.trk == SQ_LAYER ? CF_PART_LAYER :
                         squ.trk == SQ_BASS ? CF_PART_BASS : CF_PART_CHORD].name;
        int x = 4 + fb_text_w(&CF_FONT_UI, tn, 1) + 7;
        fb_text(&CF_FONT_UI, x, Y_TOP + 1, sn, FL_INK3, 1);
        if (!dm_plaque_top())                    /* (a rhythm plays, a take: the plaque instead, cf_dmv.c) */
            draw_top_hints(x + fb_text_w(&CF_FONT_UI, sn, 1));
    } else if (cu.scr != SCR_HOME && !dm_plaque_top())   /* (the plaque in the PLAY hint's place: cf_dmv.c) */
        draw_top_hints(4 + fb_text_w(&CF_FONT_UI, SCR_NAME[cu.scr], 1) +
                       (cu.scr == SCR_EDIT ? 7 + fb_text_w(&CF_FONT_UI, cf_snd[cu.edit_part].name, 1) : 0));
    if (cu.upd_left) {                           /* the update gesture: a countdown over everything */
        char b[2];
        b[0] = (char)('0' + cu.upd_left % 10u);
        b[1] = 0;
        fb_fill(FL_BG);
        fb_text_c(&CF_FONT_UI, 120, 70, b, C_ACC(0), 4);
        fb_text_c(&CF_FONT_UI, 120, 140, cu.upd_stub ? "update mode" : "USB boot", FL_INK, 1);
        fb_text_c(&CF_FONT_UI, 120, 156, "let go to cancel", FL_INK2, 1);
        cf_present();
        return;
    }
    fb_rect(0, Y_VIEW + H_VIEW, FB_W, Y_KEYS - Y_VIEW - H_VIEW, FL_BG);   /* the gap over the keys: the intro's plant
                                                  * (cf_splash.c, 4 px lower) left its stem's foot there, and no
                                                  * screen draws it (the owner saw it over the m6 key, 2026-10-19) */
    draw_keys();
    draw_knobs();
    cf_present();
}

#include "cf_native_pages.c"
#include "cf_arp_ui.c"
typedef struct { char text[3][64];char name[3][24];uint16_t pos,n;uint8_t kind,skin,ack,focus,progress,warn; } cfn_popup_t;
static cfn_popup_t cfn_popup_last;
static uint32_t cfn_saved_ms,cfn_popup_focus_ms;
static uint16_t cfn_popup_target;
static void cfn_message_lines(char dst[3][64],const char *src)
{
    for(unsigned row=0;row<3 && *src;row++) {
        unsigned n=0,space=0;
        while(src[n] && src[n]!='\n' && n<62){dst[row][n]=src[n];dst[row][n+1]=0;if(cfn_width(&CFN_12,dst[row])>208){dst[row][n]=0;break;}if(src[n]==' ')space=n;n++;}
        if(src[n] && src[n]!='\n' && space)n=space;
        dst[row][n]=0;src+=n;while(*src==' ' || *src=='\n')src++;
    }
}
static void cf_ui_draw(void)
{
    fb_unclip();cfn_motion_frame();
    if(!cu.upd_left && cgu_glow.active){
        cf_stage_valid=cf_acc_valid=cfd_valid=cfn_page_valid=cfarp_valid=cfl_valid=csl_screen_valid=0;
        cgu_glow_draw();
        return;
    }
    cgu_glow_valid=0;
    if(!cu.upd_left && cu.scr==SCR_LAB){cf_stage_valid=cf_acc_valid=cfd_valid=cfn_page_valid=0;cfl_draw();return;}
    cfl_valid=0;
    if(cf_intro_draw(cu.frame_dt)){cf_stage_valid=cf_acc_valid=cfd_valid=cfn_page_valid=0;return;}
    if(!cu.upd_left && cu.scr==SCR_SONGS) {
        cf_stage_valid=cf_acc_valid=cfd_valid=cfn_page_valid=0;
        memset(&cfn_popup_last,0,sizeof cfn_popup_last);csl_draw();return;
    }
    csl_screen_valid=0;
    cfn_popup_t pop;memset(&pop,0,sizeof pop);pop.kind=cu.pop;pop.skin=cf_skin;
    if(cu.pop==POP_SOUND_CHORD || cu.pop==POP_SOUND_BASS || cu.pop==POP_SOUND_LAYER) {
        unsigned part=cu.pop==POP_SOUND_LAYER?2:cu.pop==POP_SOUND_BASS?1:0;
        pop.pos=cf_snd_pos(part);pop.n=cf_snd_count(part);
        str_cpy(pop.text[0],part==2?"Layer sound":part?"Bass sound":"Chord sound",sizeof pop.text[0]);
        for(unsigned i=0;i<3;i++) {unsigned idx=pop.n?(pop.pos+pop.n+i-1)%pop.n:0,e,pp;cf_snd_at(part,idx,&e,&pp);pool_name(e,pp,pop.name[i]);}
        unsigned target=(pop.kind<<9)|pop.pos;
        if(cfn_popup_target!=target){cfn_popup_target=target;cfn_popup_focus_ms=cf_motion?(cu.now?cu.now:1):0;}
        unsigned age=cu.now-cfn_popup_focus_ms;
        if(cf_motion && cfn_popup_focus_ms && (int32_t)(cfn_popup_focus_ms-cf_motion_epoch)>0 && age<100)pop.focus=2*(100-age)*(100-age)/10000;
    } else if(cu.pop==POP_ALGO) {str_cpy(pop.text[0],"Chord layout",sizeof pop.text[0]);str_cpy(pop.text[1],ALG_NAME[cs.chord.algo],sizeof pop.text[1]);str_cpy(pop.text[2],cs.chord.algo&1?"Buttons latch":"Hold chord buttons",sizeof pop.text[2]);}
    else if(cu.pop==POP_OCTAVE){str_cpy(pop.text[0],"Octave",sizeof pop.text[0]);kint(pop.text[1],cs.chord.octave,0);}
    else if(cu.pop==POP_MSG) {
        cfn_message_lines(pop.text,cu.msg);
    }
    if(cu.scr==SCR_SQ && sq_clear_down) {
        unsigned elapsed=cu.now-sq_clear_down;pop.kind=POP_MSG;pop.warn=1;
        kint(pop.text[0],sq_number(sq_clear_slot)," / Clear recording");
        str_cpy(pop.text[1],"Hold SAVE: ",sizeof pop.text[1]);kint(pop.text[1]+str_len(pop.text[1]),elapsed>=3000?0:(3000-elapsed+999)/1000," s left");
        str_cpy(pop.text[2],"Release to cancel",sizeof pop.text[2]);pop.progress=elapsed>=3000?100:elapsed/30;
    }
    if(cu.upd_left)memset(&pop,0,sizeof pop);
    if(pop.kind==POP_MSG && !memcmp(pop.text[0],"Saved ",6)) {
        if(cfn_popup_last.kind!=POP_MSG || memcmp(pop.text,cfn_popup_last.text,sizeof pop.text))cfn_saved_ms=cf_motion?(cu.now?cu.now:1):0;
        pop.ack=cf_tween_offset(cfn_saved_ms,240,20);
    } else cfn_saved_ms=0;
    int changed=memcmp(&pop,&cfn_popup_last,sizeof pop)!=0;
    if(changed){cf_stage_valid=cf_acc_valid=cfd_valid=cfn_page_valid=cfarp_valid=0;cfn_popup_last=pop;}
    if(cu.scr!=SCR_ARP || cu.upd_left)cfarp_valid=0;
    unsigned before=cf_stage_draws+cf_acc_draws+cfd_draws+cfn_page_draws+cfarp_draws;
    if(!cu.upd_left && cu.scr==SCR_PEDIT){cf_stage_valid=cf_acc_valid=cfn_page_valid=0;cfd_draw();}
    else if(!cu.upd_left && (cu.scr==SCR_ACC || (cu.scr==SCR_GLO && cu.page[SCR_GLO]==CF_GLO_LEGACY))) {cf_stage_valid=cfd_valid=cfn_page_valid=0;cf_acc_draw();}
    else if(!cu.upd_left && cu.scr==SCR_HOME && home_view()==HV_FLOWER && cf_scene()==CF_SCENE_PROGRESSIONS){cf_acc_valid=cfd_valid=cfn_page_valid=0;cf_stage_draw();}
    else if(!cu.upd_left && cu.scr==SCR_ARP){cf_stage_valid=cf_acc_valid=cfd_valid=cfn_page_valid=0;cfarp_draw();}
    else {cf_stage_valid=cf_acc_valid=cfd_valid=0;cfn_page_draw();}
    if(!pop.kind || (!changed && before==cf_stage_draws+cf_acc_draws+cfd_draws+cfn_page_draws+cfarp_draws))return;
    fb_unclip();cfn_dialog(8,55,224,131,pop.warn?FL_C(FL_R_ROSE,0):FL_BG3);
    if(pop.n) {
        cfn_text(&CFN_12,16,74,pop.text[0],FL_INK2);
        for(unsigned i=0;i<3;i++){if(i==1){fb_rect(12,109,216,29,FL_BG1);fb_rect(12,109,2+pop.focus,29,C_ACC(0));}cfn_fit(&CFN_16,20,100+i*30,pop.name[i],200,i==1?C_ACC(0):FL_INK2);}
        char b[20];kint(b,pop.pos+1," / ");kint(b+str_len(b),pop.n,0);cfn_text(&CFN_12,16,181,b,FL_INK2);
    } else {
        for(unsigned i=0;i<3;i++)cfn_fit(i==1 && pop.kind!=POP_MSG?&CFN_32:pop.kind==POP_MSG?&CFN_12:&CFN_16,16,89+i*32,pop.text[i],208,i?FL_INK:FL_INK2);
        if(pop.ack)fb_hline(16,168,20-pop.ack+8,C_ACC(0));
        if(pop.progress)fb_rect(16,174,208*pop.progress/100,3,FL_C(FL_R_ROSE,0));
    }
    cf_present();
}
