/* SPDX-License-Identifier: GPL-3.0-only
 * CoralFlower: the UI's state and settings (the main loop's; the audio side is told through cf_out.c's queue).
 * Included after cf_out.c, cf_sound.c and flora; before cf_draw.c and cf_ui.c. */

/* ------------------------------------------------------------- screens --- */
enum {
    SCR_HOME = 0,  /* HOME: play mode */
    SCR_SEL,       /* SEL: the chord editor */
    SCR_ENV,       /* ENV: the envelope */
    SCR_BASS,      /* LFO: the bass editor */
    SCR_EDIT,      /* EDIT: the sound editor */
    SCR_GLO,       /* GLO: global settings */
    SCR_FX,        /* FX: effects */
    SCR_ARP,       /* ARP: the arpeggiator */
    SCR_SEQ,       /* SEQ: the metronome */
    SCR_SAVE,      /* SAVE */
    SCR_SQ,        /* SEQ again: the sequencer, a pattern's tracks or the song (cf_sequi.c) */
    SCR_SQED,      /* .. a track open: its steps */
    SCR_DRUM,      /* SEQ again, the METRONOME's carousel on it: the rhythm machine (0.6, cf_dmui.c) */
    SCR_ACC,       /* native shared-clock Groove / Loop / Mix / Click */
    SCR_PEDIT,     /* native protected semantic progression editor */
    SCR_LAB,       /* actual-note learning and offline next-chord suggestions */
    SCR_SONGS,     /* saved progressions and existing fixed-note recordings */
    SCR_N
};
static const char *const SCR_NAME[SCR_N] = {"HOME", "CHORD", "ENVELOPE", "BASS", "SOUND", "GLOBAL", "EFFECTS",
                                             "ARPEGGIO", "METRONOME", "SAVE", "RECORDINGS",
                                             "PATTERN", "RHYTHM MACHINE", "ACCOMPANIMENT", "PROGRESSION EDIT", "CHORD LAB", "SONGS"};

/* themes: the one interface colour (GLO > Display > COLOR), a palette ramp, after the M-VAVE synths' panel
 * colours and a few more */
typedef struct { const char *name; uint8_t ramp; uint32_t light; } cf_theme_t;   /* light: another lightest step
                                                                                     * (0xRRGGBB), 0 = the ramp's */
static const cf_theme_t CF_THEMES[] = {
    {"Coral", FL_R_ROSE, 0}, {"Amber", FL_R_FIRE, 0}, {"Peach", FL_R_PEACH, 0},
    {"Lime", FL_R_LIME, 0xB0E34Cu},            /* (the lime ramp's top turned greener: the UI's own) */
    {"Jade", FL_R_JADE, 0x5BB42Fu},            /* (the jade ramp starts yellow-green: the leaf green instead) */
    {"Sky", FL_R_SEA, 0}, {"Orchid", FL_R_ORCHID, 0}, {"Bone", FL_R_SAND, 0}, {"Mist", FL_R_SAGE, 0},
};
#define CF_NTHEMES ((int)(sizeof CF_THEMES / sizeof CF_THEMES[0]))

/* ------------------------------------------------------------ settings --- */
/* everything the user sets and the unit keeps (cf_settings.c stores it) */
typedef struct {
    cf_chord_cfg_t chord;        /* algorithm, Quantize, Key, Scale, octave, the chord buttons' forms */
    uint16_t glide_ms;           /* HOME K4 */
    uint8_t glide_always;        /* GLO > Play: Always (from the last chord too) or Legato (held chords only) */
    uint8_t vel;                 /* the fixed velocity */
    uint8_t bass_on, octaver;
    int8_t bass_oct;
    uint8_t bass_level;          /* the bass part's P_LEVEL */
    uint8_t metro_level, beats;  /* SEQ: level 0..100, time signature (beats a bar) */
    uint16_t bpm;
    uint8_t accent;              /* the interface colour: a CF_THEMES index */
    uint8_t env_link;            /* ENV edits the chord and the bass part together */
    uint8_t midi_out, ch_chord, ch_bass;
    uint8_t lights;              /* key lights: 0 played only, 1 + the scale */
    uint8_t click;               /* the metronome's sound (cf_out.c CF_CLICKS) */
    uint8_t backdrop;            /* what is behind the plant (FL_BD_*, GLO > Display) */
    cf_fx_t fx;                  /* the effects chain: its order, what is on, the values (FX; cf_fx.c) */
    uint16_t snd[2];             /* the parts' sounds: indices in the one row (cf_sound.c) */
    /* (new fields go here, at the end: cf_settings.c reads an older, shorter record and keeps the defaults past it) */
    uint8_t retrig;              /* a chord changed under held keys (GLO > Play > RETRIG): CF_RT_WAIT a chord button
                                  * there waits for the next root key, another root moves the sounding voices with no
                                  * new attack; CF_RT_MOVE a button moves them too, only the tones a chord adds start
                                  * (the behaviour before WAIT); CF_RT_STRIKE the new notes strike; CF_RT_CHORD
                                  * the whole new chord strikes, the shared tones too, gliding from the last chord */
    cf_arp_t arp;                /* the arpeggiator's settings (ARP; on / off is not kept: off at power-on) */
    /* four bytes that held 0.3.1's looper settings (unused since 0.4, kept as they were: BARS 0..4, MODE 0..1, QUANT
     * 0..2, COUNT-IN 0..1). 0.5 marks its record in the third (CF_SET_MARK05): a record without it is an older build's,
     * whose first two bytes are a looper's, not NOTES IN and the ARP (cf_settings.c cfs_sanitize). 0.4.x reads the
     * mark as a QUANT out of range and resets the four in its memory only; if it saves, 0.5 later finds no mark and
     * takes NOTES IN and the ARP at their defaults. The fourth holds IN CH since 0.5.1 */
    uint8_t notes_in;            /* GLO > MIDI > NOTES IN (0.5): CF_NI_KEYS, CF_NI_ROOTS, CF_NI_DIRECT */
    uint8_t arp_keep;            /* the arpeggiator was on: on again after a power-on (0.5, the owner: "Арпеджиатор бы
                                  * ещё было прикольно сохранять") */
    uint8_t mark;                /* CF_SET_MARK05 */
    uint8_t in_ch;               /* GLO > MIDI > IN CH (0.5.1): the channel NOTES IN's FM-1 keys and Roots listen to,
                                  * CF_INCH_ALL every channel (as 0.5, whose byte here was always 1: the looper's
                                  * COUNT-IN kept at 1), else CF_INCH_ALL + 1 + the channel 0..15 (the owner, 2026-10-26:
                                  * "Новая настройка IN CH"; a Digitakt's other tracks played chords, GitHub #2) */
    uint8_t clock_in;            /* GLO > MIDI > CLOCK: 0 Off; 1 In, MIDI clock's pulses run the beat; 2 Out, the unit sends its beat as MIDI clock */
    uint8_t boot_sound;          /* GLO > Display > BOOT SOUND: the chime with the intro */
    uint8_t level_chord, level_layer, level_drums;   /* the parts' P_LEVEL (the sequencer's tracks, K1; the bass's:
                                                      * bass_level) */
    uint16_t snd_layer;          /* the LAYER's sound (part 2): its index in the chord sounds' row; 0xFFFF: the default */
    uint8_t ch_layer, ch_drums;  /* GLO > Channels: the sequencer's LAYER and DRUMS out on MIDI, 0..15 */
    uint8_t seq_hint;            /* bits 0..2: hints; 3: Bonfire, 4: Nothing, 5: Bass (all clear: Progressions);
                                  * bits 6..7: HOME main/Chord/Keyboard/Diagnostics. */
    uint8_t resume;              /* what PLAY on HOME plays after a power-on (0.4.2, the owner: "Как оставил";
                                  * cf_sequi.c sq_home_poll): CF_RESUME_ON | pattern 0..7, or CF_RESUME_ON |
                                  * CF_RESUME_SONG | the block the song's frame is on; 0 none. In what was the record's
                                  * padding byte: its size stays, 0.4.1 reads it */
} cf_set_t;
#define CF_RESUME_ON 0x80u
#define CF_RESUME_SONG 0x40u
#define CF_SET_MARK05 0xC5u    /* cs.mark: the record is 0.5's or later (out of 0.4.x's QUANT range, 0..2) */
#define CF_INCH_ALL 1u         /* cs.in_ch: every channel; 2..17 channels 1..16 */

static cf_set_t cs;
static cf_browser_t cpb;
/* Appearance v5 ties the whole palette to the selected mascot. Old appearance
 * IDs remain named for record migration; they are not additional UI choices. */
enum { CF_SKIN_EMBER, CF_SKIN_PLUM, CF_SKIN_N };
enum { CF_SKIN_TEAL=0, CF_SKIN_STUDIO=1, CF_SKIN_EDITORIAL=2, CF_SKIN_LEGACY_N=3 };
static const char *const CF_SKIN_NAME[] = {
    CF_STUDIO && !CF_SCENES?"Amber":"Devil", CF_STUDIO && !CF_SCENES?"Plum":"Cat"
};
static uint8_t cf_skin, cf_motion = 1;
enum { CF_HB_CORE, CF_HB_SOUND, CF_HB_TEXTURE, CF_HB_GROOVE, CF_HB_PERFORM, CF_HB_LOOP, CF_HB_MIX, CF_HB_VARIATION, CF_HB_ENGINE, CF_HB_FLOW, CF_HB_LEARN, CF_HB_N };
static const char *const CF_HOME_BANK_NAME[] = {"CORE", "SOUND", "TEXTURE", "GROOVE", "PERFORM", "LOOP", "MIX", "VARIATIONS", "JAM", "FLOW", "KEYBOARD"};
/* Add future toolbars before Learn: Harmony must remain the final HOME page. */
_Static_assert(CF_HB_LEARN + 1 == CF_HB_N, "Harmony must be the last HOME page");
static uint8_t cf_home_bank;
static uint8_t cf_learn_focus = 255; /* last physical/FM-1-mapped key played on Keyboard; session only */
static uint32_t cf_home_bank_ms, cf_motion_epoch;
static int8_t cf_nav_dir = 1;
static uint8_t cf_click_level = 35;
enum { CF_ACC_GROOVE, CF_ACC_LOOP, CF_ACC_MIX, CF_ACC_CLICK, CF_ACC_N };
static const char *const CF_ACC_NAME[] = {"Groove", "Loop", "Mix", "Click"};
static uint8_t cf_acc_focus, cf_legacy_pick;
static uint32_t cf_acc_page_ms;
static uint32_t cf_art_ms(void);

static cf_harmony_t ch;                         /* separate versioned record; cf_set_t stays 188 bytes */
static struct {
    uint8_t idea, rhythm, mode, explored, repeat_off;
    uint8_t saved_tonic, saved_scale, saved_quantize, saved_beats;
    uint8_t tempo_generation;
    uint32_t revision, modifiers, click_generation;
} cpg;             /* Browser selection only; never persisted */

static void cs_defaults(cf_set_t *s)
{
    cf_chord_defaults(&s->chord);
    s->glide_ms = 0;
    s->glide_always = 0;              /* Legato: a glide between held chords only */
    s->vel = 100;
    s->bass_on = 1;
    s->octaver = 0;
    s->bass_oct = 0;
    s->bass_level = 96;
    s->metro_level = 0;
    s->beats = 4;
    s->bpm = 100;
    s->accent = 3;                   /* Lime */
    s->env_link = 1;
    s->midi_out = 1;
    s->ch_chord = 0;
    s->ch_bass = 1;
    s->lights = 1;
    s->click = 0;
    s->backdrop = FL_BD_SCENE;
    s->fx = CFX_DEFAULT;
    s->snd[0] = s->snd[1] = 0;
    s->retrig = CF_RT_WAIT;
    {
        static const cf_arp_t A = CF_ARP_DEFAULT;
        s->arp = A;
    }
    s->notes_in = CF_NI_KEYS;
    s->arp_keep = 0;
    s->mark = CF_SET_MARK05;
    s->in_ch = CF_INCH_ALL;                      /* (nothing goes quiet after an update: every channel, as 0.5) */
    s->boot_sound = 1;
    s->level_chord = s->level_layer = 104;       /* (params.c P_LEVEL's own) */
    s->level_drums = 104;
    s->snd_layer = 0xFFFFu;
    s->ch_layer = 2;                             /* (channel 3) */
    s->ch_drums = 9;                             /* (channel 10: General MIDI's drums) */
    s->seq_hint = 0;
    s->resume = 0;
}

/* --------------------------------------------------------- the UI's own --- */
typedef struct {
    uint8_t scr;                 /* the screen */
    uint8_t page[SCR_N];         /* each screen's page (SELECT / PRESETS) */
    uint8_t sel_btn;             /* SEL: the chord button being edited */
    uint8_t edit_sec;            /* EDIT: the section (OP1..OP6, PIT, GLO) */
    uint8_t edit_part;           /* EDIT: the sound edited, 0 chord 1 bass */
    uint8_t env_tgt;             /* ENV: 0 chord + bass, 1 chord, 2 bass, 3 the sequencer's LAYER */
    uint8_t fx_pos;              /* FX: the selected place in the chain */
    uint8_t arp_on;              /* the arpeggiator plays (K1 on the ARP page; since 0.5 as it was at the power-off:
                                  * cs.arp_keep) */
    uint8_t save_new;            /* SAVE: as a new sound (else over itself) */
    char save_name[13];          /* .. its name, being edited */
    uint8_t save_cur;            /* .. the letter K3 changes */
    uint32_t save_hold;          /* .. when SAVE went down on the SAVE screen (0: not held) */
    uint32_t rec_hold;           /* when REC went down (0: up): a tap or a hold (cf_rec_poll) */
    uint8_t rec_fired;           /* .. the hold acted: its release does nothing */
    uint8_t save_del;            /* .. ACTION Delete (a user sound only) */
    uint32_t now, last_ms;       /* ms */
    uint32_t frame_dt;           /* ms since the frame before */
    uint32_t keys;               /* the note keys down: the FM-1's own (pkeys) and an outside keyboard's (xkeys) */
    uint32_t pkeys, xkeys;       /* .. as last read; .. held from outside (NOTES IN FM-1 keys: cf_ui.c cf_xq_drain) */
    uint8_t xcnt[27];            /* .. outside notes down on each key (two octaves of one key fold onto it) */
    uint8_t xnote[128];          /* each outside note down: 0 up, key + 1, or 0xFF a root (NOTES IN Roots) */
    uint8_t xroots;              /* .. roots down from outside */
    uint32_t knob_ms[4];         /* when each knob last moved (its cell lights) */
    uint8_t knob_last;
    /* popups over the plant */
    uint8_t pop;                 /* POP_* */
    uint32_t pop_until;
    char msg[64];                /* (two lines at most, a '\n' between them) */
    uint8_t booted;
    uint32_t frames;
    uint32_t idle_ms;            /* since the last touch */
    uint8_t upd_left, upd_stub;  /* the update gesture's countdown (cf_shim.c cr_update_poll) */
    uint8_t edited[3];           /* a part's sound changed since it was loaded (SAVE keeps it): chord, bass, layer */
} cf_ui_t;
enum { POP_NONE, POP_SOUND_CHORD, POP_SOUND_BASS, POP_ALGO, POP_OCTAVE, POP_MSG, POP_SOUND_LAYER };
static cf_ui_t cu;

/* the plant */
#define FLW 120
#define FLH 70
#if CF_SCENES
static uint8_t fl_px[FLW * FLH] __attribute__((section(".pool")));
static flora_t flo __attribute__((section(".pool")));
static flc_t fl_cv = {fl_px, FLW, FLH};
#endif

/* ---------------------------------------------------------- colours --- */
/* palette entries 48..55: the interface colour's ramp, twice (C_ACC and C_SEC: the drawing still tells the two roles
 * apart, they share the one colour) */
#define C_ACC(s) ((uint8_t)(48 + (s)))
#define C_SEC(s) ((uint8_t)(52 + (s)))
#include "cf_mascot_art.h"
enum { CGU_INK_BASE=57, CFN_HALO_0=64, CFN_HALO_1, CFN_HALO_2, CFN_HALO_3, CFM_PAL_BASE=68, CGU_GLASS=83, CGU_GLASS_AA=84,
    CFN_AA_BASE=128, CFN_AA_ROLES=5, CFN_AA_SURFACES=4,
    CGU_BAND_INK_BASE=CFN_AA_BASE+CFN_AA_ROLES*CFN_AA_SURFACES*2,
    CGU_RACK_PALETTE=CGU_BAND_INK_BASE+4,
    CGU_METAL=CGU_RACK_PALETTE+20,
    CGU_GRILLE=CGU_METAL+12, CGU_PROGRESSION_INK=CGU_GRILLE+6, CGU_MATERIAL_AA=CGU_GRILLE+8,
    CGU_FACE_AA=CGU_MATERIAL_AA+8, CFN_PAL_N=256 };
_Static_assert(CGU_FACE_AA+9*4 <= CFN_PAL_N, "Studio material palette fits");
static uint32_t cf_pal[CFN_PAL_N];
#include "cf_rack_style.c"
static uint32_t cfn_blend(uint32_t bg,uint32_t fg,unsigned alpha)
{
    unsigned r=((bg>>16&255)*(100-alpha)+(fg>>16&255)*alpha)/100;
    unsigned g=((bg>>8&255)*(100-alpha)+(fg>>8&255)*alpha)/100;
    unsigned b=((bg&255)*(100-alpha)+(fg&255)*alpha)/100;
    return r<<16|g<<8|b;
}

#ifndef CF_EMU
/* The FM-1's panel lifts the dark tones and washes out soft colours: the palette, made on a monitor, came out pale and
 * grey there (the owner's photos, 2026-10-15). On the device every colour is recalculated once, here: its saturation
 * raised away from its own luma, then a gamma of 1.2 (darks a little deeper, white and black kept). The interface's
 * neutrals (entries 0..7: background, panels, text) get x1.6. The colours (the plant's ramps, the accent, the sky) get
 * more the softer they are: x2.5 for a pastel down to x1.5 for a full colour, by the square of the colour's own
 * saturation, so the pale pinks rise to where the aqua already was. The emulator shows the palette as designed.
 * CF_PANEL_PLAIN builds the device without it. (rc6 flat x1.4: "much better"; rc8 flat x1.6: "super", but the blue
 * flower far stronger than the rest; the owner: raise the others.) */
static const uint8_t CF_PANEL_GAMMA[256] = {
      0,   0,   1,   1,   2,   2,   3,   3,   4,   5,   5,   6,   7,   7,   8,   9,
      9,  10,  11,  11,  12,  13,  13,  14,  15,  16,  16,  17,  18,  19,  20,  20,
     21,  22,  23,  24,  24,  25,  26,  27,  28,  28,  29,  30,  31,  32,  33,  34,
     34,  35,  36,  37,  38,  39,  40,  40,  41,  42,  43,  44,  45,  46,  47,  48,
     49,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  62,
     63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,
     79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,
     95,  96,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110,
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
    128, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 144, 145,
    146, 147, 148, 149, 150, 151, 152, 153, 155, 156, 157, 158, 159, 160, 161, 162,
    163, 165, 166, 167, 168, 169, 170, 171, 172, 173, 175, 176, 177, 178, 179, 180,
    181, 183, 184, 185, 186, 187, 188, 189, 191, 192, 193, 194, 195, 196, 197, 199,
    200, 201, 202, 203, 204, 205, 207, 208, 209, 210, 211, 212, 214, 215, 216, 217,
    218, 219, 221, 222, 223, 224, 225, 226, 228, 229, 230, 231, 232, 234, 235, 236,
    237, 238, 239, 241, 242, 243, 244, 245, 247, 248, 249, 250, 251, 253, 254, 255,
};
static uint32_t cf_panel_fix(uint32_t c, int colour)
{
    int32_t ch[3] = {(int32_t)((c >> 16) & 255u), (int32_t)((c >> 8) & 255u), (int32_t)(c & 255u)};
    int32_t y = (ch[0] * 77 + ch[1] * 150 + ch[2] * 29) >> 8, i, v, mx, mn, sat, k;
    uint32_t out = 0;
    mx = ch[0] > ch[1] ? ch[0] : ch[1];
    mx = mx > ch[2] ? mx : ch[2];
    mn = ch[0] < ch[1] ? ch[0] : ch[1];
    mn = mn < ch[2] ? mn : ch[2];
    sat = mx ? (mx - mn) * 256 / mx : 0;          /* the colour's own saturation, 0..256 */
    k = colour ? 637 - ((258 * ((sat * sat) >> 8)) >> 8) : 410;   /* in 1/256: x2.49 .. x1.48; the neutrals x1.6 */
    for (i = 0; i < 3; i++) {
        v = y + (((ch[i] - y) * k) >> 8);
        v = v < 0 ? 0 : v > 255 ? 255 : v;
        out = (out << 8) | CF_PANEL_GAMMA[v];
    }
    return out;
}
#endif

static uint32_t cf_art_ms(void) { return cf_motion ? cu.now : 0; }
/* One bounded latest target per region. No callbacks, allocation or visual clock. */
static int cf_tween_offset(uint32_t start, unsigned ms, unsigned distance)
{
    if(!cf_motion || !start || !ms || (int32_t)(start-cf_motion_epoch)<=0 || cu.now-start>=ms)return 0;
    unsigned left=ms-(cu.now-start);
    return (int)(distance*left*left*left/(ms*ms*ms));
}
static const uint32_t CF_SKIN_RGB[CF_SKIN_N][7] = {
    {0x150F0C,0x241813,0x5D4537,0xFFF4E5,0xD0BFB1,0xFFA55B,0x28170D},
    {0x190F19,0x291929,0x5D3E58,0xFFF2FB,0xD0B8CD,0xF29BCD,0x30152A}
};
static void cf_palette_build(void)
{
    uint32_t i;
    for (i = 0; i < FL_PAL_SIZE; i++)
        cf_pal[i] = fl_pal_rgb[i];
    for (i = 0; i < 4u; i++) {
        const cf_theme_t *th = &CF_THEMES[cs.accent % CF_NTHEMES];
        cf_pal[48 + i] = !i && th->light ? th->light : fl_pal_rgb[FL_C(th->ramp, i)];
        cf_pal[52 + i] = cf_pal[48 + i];
    }
    for (i = 0; i < 8u; i++)
        cf_pal[56 + i] = fl_back_rgb[i];
#if !defined CF_EMU && !defined CF_PANEL_PLAIN
    for (i = 0; i < 64u; i++)
        cf_pal[i] = cf_panel_fix(cf_pal[i], i >= FL_RAMP0);
#endif
    /* Tokens are authored RGB; the panel conversion above belongs to legacy art. */
    const uint32_t *skin = CF_SKIN_RGB[cf_skin % CF_SKIN_N];
    cf_pal[FL_BG] = skin[0]; cf_pal[FL_BG1] = cf_pal[FL_BG2] = skin[1];
    cf_pal[FL_BG3] = skin[2]; cf_pal[FL_INK] = skin[3];
    cf_pal[FL_INK2] = skin[4]; cf_pal[FL_INK3] = skin[4];
    cf_pal[48] = cf_pal[49] = skin[5]; cf_pal[50] = skin[4]; cf_pal[51] = skin[1];
    cf_pal[52] = skin[5]; cf_pal[53] = skin[4]; cf_pal[54] = skin[2]; cf_pal[55] = skin[1];
    cf_pal[56] = skin[6];
    for(unsigned i=0;i<4;i++)cf_pal[CFN_HALO_0+i]=cfn_blend(skin[0],skin[5],12+12*i);
    const uint32_t *mascot_rgb=CF_STUDIO && !CF_SCENES?
        CFM_STICKERS[cf_skin%CF_SKIN_N].rgb:CFM_ART[cf_skin%CF_SKIN_N].rgb;
    for(unsigned i=0;i<CFM_ART_COLORS;i++)cf_pal[CFM_PAL_BASE+i]=mascot_rgb[i];
    cf_pal[CGU_INK_BASE]=0xffb464;   /* Chords */
    cf_pal[CGU_INK_BASE+1]=0x79d5ea; /* Bass */
    cf_pal[CGU_INK_BASE+2]=0xc6acff; /* Layer */
    cf_pal[CGU_INK_BASE+3]=0xfb878d; /* Drums */
    cf_pal[CGU_BAND_INK_BASE]=0xb8e59a;   /* Harmony */
    cf_pal[CGU_BAND_INK_BASE+1]=0x72deda; /* Shaping */
    cf_pal[CGU_BAND_INK_BASE+2]=0xeac889; /* Genre */
    cf_pal[CGU_BAND_INK_BASE+3]=0xb7afff; /* ARP */
    if(CF_STUDIO && !CF_SCENES){
        cf_rack_colors();
        cf_pal[48]=cf_pal[49]=cf_pal[52]=cf_skin==CF_SKIN_PLUM?skin[5]:cf_pal[RAKT_AMBER];
        cf_pal[50]=cf_pal[53]=cf_pal[FL_INK2];
        cf_pal[51]=cf_pal[55]=cf_pal[FL_BG1];
        cf_pal[54]=cf_pal[FL_BG3];
        static const uint32_t metal[]={0x111416,0x202427,0x2d3235,0x3d4346,0x50595d,0x667075,
            0x838d90,0xa5afb0,0xcbd0cb,0xe7e8dd,0x090b0c,0x181c1f};
        static const uint32_t grille[]={0x121516,0x1c1f20,0x242829,0x2c3030,0x353939,0x414545,0x4c5050,0x565b59};
        cf_pal[CGU_GLASS]=cf_pal[RAKT_GLASS];
        for(unsigned unit=0;unit<5;unit++){
            unsigned ink=unit<4?CGU_BAND_INK_BASE+unit:CGU_PROGRESSION_INK;
            uint32_t color=unit<4?cf_pal[ink]:0x9dcbed;
            for(unsigned level=0;level<2;level++)
                cf_pal[CGU_GLASS_AA+unit*2+level]=cfn_blend(cf_pal[CGU_GLASS],color,(level+1)*100/3);
        }
        memcpy(cf_pal+CGU_METAL,metal,sizeof metal);
        memcpy(cf_pal+CGU_GRILLE,grille,sizeof grille);
        cf_pal[CGU_PROGRESSION_INK]=0x9dcbed;
        for(unsigned surface=0;surface<2;surface++){
            unsigned bg=surface?CGU_GRILLE+2:CGU_METAL+2;
            for(unsigned ink=0;ink<2;ink++)for(unsigned level=0;level<2;level++)
                cf_pal[CGU_MATERIAL_AA+surface*4+ink*2+level]=
                    cfn_blend(cf_pal[bg],cf_pal[ink?FL_INK2:FL_INK],(level+1)*100/3);
        }
        for(unsigned face=RAKT_CHORDS;face<=RAKT_PROGRESSION;face++){
            for(unsigned ink=0;ink<2;ink++)for(unsigned level=0;level<2;level++)
                cf_pal[CGU_FACE_AA+(face-RAKT_CHORDS)*4+ink*2+level]=
                    cfn_blend(cf_pal[face],cf_pal[FL_INK+ink],(level+1)*100/3);
        }
    }
    static const uint8_t text_ink[]={FL_INK,FL_INK2,48,56,FL_C(FL_R_ROSE,0)};
    static const uint8_t text_bg[]={FL_BG,FL_BG1,48,FL_BG3};
    for(unsigned bg=0;bg<CFN_AA_SURFACES;bg++)for(unsigned ink=0;ink<CFN_AA_ROLES;ink++)
        for(unsigned level=0;level<2;level++)cf_pal[CFN_AA_BASE+(bg*CFN_AA_ROLES+ink)*2+level]=
            cfn_blend(cf_pal[text_bg[bg]],cf_pal[text_ink[ink]],(level+1)*100/3);
    fb_palette(cf_pal, CFN_PAL_N);
}
