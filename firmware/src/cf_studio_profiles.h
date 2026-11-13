/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef CF_STUDIO_PROFILES_H
#define CF_STUDIO_PROFILES_H

enum { CGS_COMPOSE_POCKET, CGS_COMPOSE_DREAM, CGS_COMPOSE_PUNK };
enum { CGS_BASS_RIFF, CGS_BASS_OCTAVE_PULSE, CGS_BASS_LATE, CGS_BASS_PEDAL };
enum { CGS_LAYER_SEQUENCE, CGS_LAYER_FRAGMENT, CGS_LAYER_SWELL };
enum { CGS_BASS_REPLY_NONE, CGS_BASS_REPLY_LOCK, CGS_BASS_REPLY_LATE };
enum { CGS_LAYER_REPLY_NONE, CGS_LAYER_REPLY_GAPS, CGS_LAYER_REPLY_SNARE, CGS_LAYER_REPLY_HARMONY,
       CGS_LAYER_REPLY_DREAM };
enum { CGS_GUITAR, CGS_ELECTRONIC, CGS_EXPERIMENTAL, CGS_NFAMILY };
enum { CGS_CAB_WALNUT, CGS_CAB_ALUMINUM, CGS_CAB_TOLEX };
static const char *const CGS_FAMILIES[]={"Guitar","Electronic","Experimental"};
enum { CGS_FORM_BARS=8, CGS_FORM_LAYER_REST=1, CGS_FORM_LIGHT_HATS=2, CGS_FORM_FILL=4 };

typedef struct {
    uint16_t chords, kick, kick_answer; /* Optional second-bar variation for style 2. */
    uint8_t gate, snare, hat_gap, drift;
} cgs_pocket_t;

typedef struct {
    const char *id, *name;
    uint8_t bpm, primary, secondary, family, kit_count, cabinet;
    uint8_t bpm_min, bpm_max;
    uint8_t keys[6], scales[4], meter_weight[3]; /* Pitch classes, modes, and weights for 3/4, 4/4, 6/8. */
    uint8_t fx[CGS_NPART][4];
    const uint8_t *kit;
    uint8_t composer, feel_count;
    uint8_t bank_duration_scale, evolve_period_scale;
    uint8_t write_progressions, preserve_bank_kit, start_curated;
    uint8_t bass_style, layer_style, layer_start, layer_floor, hold_chords, sparse_drums;
    uint8_t bass_reply[2]; /* Response to planned kicks, per Bass style. */
    uint8_t layer_reply[2]; /* Response source per Layer style. */
    const uint8_t *form; /* CGS_FORM_BARS entries, counted from the start of the take. */
    cgs_pocket_t pocket;
    cgs_riff_t bass[2];
    const char *style[CGS_NPART][2];
#if CHASE_LIBRARY && FELUCCA_VA
    const char *tone[CGS_DRUMS][5]; /* The last tone in each row is the curated choice. */
    const char *const *const *extra_tones;
    uint8_t original_voice[CGS_DRUMS]; /* Zero means no additional original for the part. */
#endif
} cgs_profile_t;

#if CHASE_LIBRARY && FELUCCA_VA
#define CGS_TONES(...) .tone={__VA_ARGS__},
#define CGS_PALETTE(extra,...) .extra_tones=extra,.original_voice={__VA_ARGS__},

static const char *const *const CGS_EXTRA_DREAM[CGS_DRUMS]={
    (const char *const[]){"SOFT HORIZON","LONG BLOOM","DUSK STRINGS","QUIET STRING","SPARKLE PAD",
        "EMBER KEYS","NOIR TINES","ICE PAD","TAPE STRINGS","GENTLE SWELL","SOFT AAH","WARM FOG","LUSH PAD",0},
    (const char *const[]){"DEEP SINE","WARM DRIFT","ROUND FINGER","SOFT FLOOR","CLEAR SUB","MELLOW REESE","ROUND THUMB",0},
    (const char *const[]){"GLASS RAIN","MUTED BELL","CEDAR MALLET","SOFT KALIMBA","TAPE BLOOM",
        "SOFT PRISM","AQ34 FULLFLU","GLOW PLUCK","WATER CHIME","DREAM BELLS","HIGH BLOOM","SLOW CHIMES","SOFT LEAD",0},
};

static const char *const *const CGS_EXTRA_INDIE[CGS_DRUMS]={
    (const char *const[]){"TINE EP","CLAVINET","DRAWBARS","CEDAR MALLET","LOUNGE ORGAN","DX TINE",
        "PIANO","EMBER KEYS","WORN TINES","ORGANISH","VELVET REED","POLY KEYS","WOODEN KEYS",0},
    (const char *const[]){"SOLID BASS","ROUND FINGER","ROUND THUMB","WOOD PLUCK","SOFT PUNCH","PLUCKED WOOD","WOOD BASS",0},
    (const char *const[]){"PLUCK","KALIMBA","DX MARIMBA","STEEL DRUM","MUTED MARIMB","HARP",
        "AQ33 DX SAX","NOIR TINES","DULL PLUCK","AIR FLUTE","WIRE LEAD","GLOW PLUCK","LATE BELL",0},
};

static const char *const *const CGS_EXTRA_WAVE[CGS_DRUMS]={
    (const char *const[]){"POLY KEYS","ROUND POLY","BRASS SECT","DARK STAB","OFFBEAT DUB","GLASS RAIN",
        "WAVE PAD","RAZOR PULSE","SYNTH BRASS","PWM PAD","SOFT POLY","NEON HAZE","WIDE STRINGS",0},
    (const char *const[]){"SOLID BASS","SAW BASS","SYNC BASS","CLEAN MONO","PUNCH BASS","RUBBER BASS","GLASS PULSE","FAT BASS",0},
    (const char *const[]){"GENTLE SYNC","CLAV","BELL","DX TINE","PLUCK","SYNC LEAD",
        "SPARKLE PAD","BRASS","GLOW PLUCK","SOFT LEAD","HOLLOW","SHIMMER","DIM CHIME",0},
};

static const char *const *const CGS_EXTRA_PUNK[CGS_DRUMS]={
    (const char *const[]){"DARK STAB","STRINGS","ROUND POLY","BRASS SECT","CLAVINET",
        "WAVE PAD","JANGLE ORGAN","PWM KEYS","SYNTH BRASS","RAZOR PULSE","NEON POLY","CABINET PAD","ORGANISH",0},
    (const char *const[]){"SOLID BASS","SAW BASS","PICKUP BASS","FELT PICK","PUNCH BASS","SYNC BASS","FAT BASS","MELLOW REESE",0},
    (const char *const[]){"PLUCK","SYNC LEAD","DX TINE","STAR KEYS","TINE EP","BRASS",
        "GLASS THREAD","GLOW PLUCK","NEON HAZE","SOFT LEAD","CRYSTAL STEP","HOLLOW","PWM KEYS",0},
};

static const char *const *const CGS_EXTRA_LOFI[CGS_DRUMS]={
    (const char *const[]){"DUSTY EP","FELT KEYS","TINE EP","DX TINE","NOIR TINES","WORN TINES","LOUNGE ORGAN",
        "MUTED SKANK","EMBER KEYS","NIGHT WINDOW","VELVET REED","SOFT POLY","WARM FOG",0},
    (const char *const[]){"ROUND FINGER","TAPE SUB","ROUNDED SUB","MUTED FINGER","WARM MONO","LOW MARIMBA","ROUND THUMB","DUB SUB",0},
    (const char *const[]){"MUTED BELL","AQ10 DX FLUT","SOFT KALIMBA","MARIMBA","CEDAR MALLET","AQ04 DX CRAC",
        "TAPE BLOOM","KALIMBA","GENTLE BOWL","DULL PLUCK","LATE BELL","GENTLE MALLE","WOODEN KEYS",0},
};

static const char *const *const CGS_EXTRA_AMBIENT[CGS_DRUMS]={
    (const char *const[]){"TAPE BLOOM","LONG BLOOM","DUSK STRINGS","SOFT HORIZON","ICE PAD","WAVE PAD",
        "VOX PAD","AQ15 DX LONG","CLOUD PAD","SLEEP DRONE","HIGH BLOOM","GENTLE SWELL","RESONANT PAD",0},
    (const char *const[]){"DEEP SINE","SOFT FLOOR","LOW MARIMBA","BELL BASS","DUB SUB","WARM DRIFT","CLEAR SUB","DEEP SUB",0},
    (const char *const[]){"UNEVEN BELLS","GLASS RAIN","SOFT KALIMBA","SILVER ORGAN","AQ34 FULLFLU","HARP",
        "TUBULAR","CEDAR MALLET","AIR FLUTE","SLOW CHIMES","GENTLE BOWL","BREATH PAD","SHIMMER",0},
};
#else
#define CGS_TONES(...)
#define CGS_PALETTE(...)
#endif
#define CGS_KITS(...) .kit=(const uint8_t[]){__VA_ARGS__},.kit_count=sizeof((const uint8_t[]){__VA_ARGS__}),

static const uint8_t CGS_FORM_INDIE[CGS_FORM_BARS]={
    [4]=CGS_FORM_LAYER_REST|CGS_FORM_LIGHT_HATS,
    [7]=CGS_FORM_FILL,
};

/* Profile IDs are stable even when the menu order changes. */
static const cgs_profile_t CGS_PROFILES[]={
    {
        .id="dream-pop", .name="Dream Pop", .bpm=104,
        .bpm_min=86, .bpm_max=118, .keys={0,2,4,5,7,9},
        .scales={CF_SC_MAJOR,CF_SC_MINOR,CF_SC_LYDIAN,CF_SC_DORIAN}, .meter_weight={1,7,4},
        .fx={{FXP_VELVET,FXP_SLAP,FXP_SILVER_PLATE,FXP_WIDE_DRIFT},
             {FXP_CLEAN,FXP_VELVET,FXP_SLAP,FXP_BITE},
             {FXP_DEEP_SPACE,FXP_PING_PONG,FXP_WIDE_DRIFT,FXP_SLAP},
             {FXP_CLEAN,FXP_VELVET,FXP_SILVER_PLATE,FXP_SLAP}},
        .primary=CF_P_DREAMY, .secondary=CF_P_EMOTIONAL, .family=CGS_GUITAR,
        CGS_KITS(DMK_AIR,DMK_ROOM,DMK_XLIVE,DMK_TAPE,DMK_GLASS,DMK_BRUSH)
        CGS_PALETTE(CGS_EXTRA_DREAM,STUDIO_VA_GLIDE_HAZE,STUDIO_VA_VELVET_PICK,STUDIO_VA_SLOW_SPARK)
        .composer=CGS_COMPOSE_DREAM, .feel_count=4,
        .bank_duration_scale=1, .evolve_period_scale=1, .layer_floor=60,
        .write_progressions=1, .preserve_bank_kit=1,
        .layer_reply={CGS_LAYER_REPLY_NONE,CGS_LAYER_REPLY_DREAM},
        .style={{"Sustain","Soft pulse"},{"Hook","Counterline"},{"Arp","Sparse motif"},{"Soft beat","Open hats"}},
        CGS_TONES({"SOFT LENS","GHOST CHOIR","CABINET PAD"}, {"SILK PICK","TAPE SUB","ROUNDWIRE"},
                  {"DREAM PLUCK","DISTANT BELL","GLASS THREAD"})
    },
    {
        .id="indie", .name="Indie", .bpm=116,
        .bpm_min=98, .bpm_max=138, .keys={0,2,4,5,7,9},
        .scales={CF_SC_MAJOR,CF_SC_MINOR,CF_SC_MIXOLYDIAN,CF_SC_DORIAN}, .meter_weight={2,8,2},
        .fx={{FXP_VELVET,FXP_BITE,FXP_SLAP,FXP_SILVER_PLATE},
             {FXP_CLEAN,FXP_VELVET,FXP_BITE,FXP_SLAP},
             {FXP_SLAP,FXP_PING_PONG,FXP_SILVER_PLATE,FXP_WIDE_DRIFT},
             {FXP_CLEAN,FXP_VELVET,FXP_BITE,FXP_SILVER_PLATE}},
        .primary=CF_P_FAMILIAR, .secondary=CF_P_UPLIFTING, .family=CGS_GUITAR,
        CGS_KITS(DMK_XLIVE,DMK_ROOM,DMK_STD,DMK_TIGHT,DMK_WOOD,DMK_TAPE)
        CGS_PALETTE(CGS_EXTRA_INDIE,STUDIO_VA_TWIN_CHOPS,STUDIO_VA_PALM_WIRE,STUDIO_VA_BRIDGE_CHIME)
        .bank_duration_scale=1, .evolve_period_scale=1, .layer_start=3, .layer_floor=60,
        .layer_reply={CGS_LAYER_REPLY_NONE,CGS_LAYER_REPLY_GAPS},
        .form=CGS_FORM_INDIE,
        .pocket={.chords=0x0449,.kick=0x0441,.gate=8,.snare=4,.hat_gap=4,.drift=1},
        .bass={
            {CGS_ANSWER,{{0,CGS_ANCHOR,10},{3,CGS_OCTAVE,4},{6,CGS_FIFTH,7},{11,CGS_THIRD,8},{15,CGS_STEP_IN,3}}},
            {CGS_FOLLOW,{{0,CGS_ANCHOR,7},{4,CGS_THIRD,4},{6,CGS_FIFTH,11},{12,CGS_UPPER,8},{15,CGS_PICKUP,3}}},
        },
        .style={{"Chop","Ring"},{"Offbeat","Answer"},{"Chime","Counterline"},{"Room beat","Open hats"}},
        CGS_TONES({"ROOM KEYS","FLOATING ORG","CABINET PAD","JANGLE ORGAN"},
                  {"FELT PICK","MUTED FINGER","MUTED WIRE","PICKUP BASS"},
                  {"GENTLE MALLE","DREAM PLUCK","GLASS THREAD"})
    },
    {
        .id="new-wave", .name="New Wave", .bpm=126,
        .bpm_min=108, .bpm_max=146, .keys={0,2,4,5,7,11},
        .scales={CF_SC_MINOR,CF_SC_DORIAN,CF_SC_MAJOR,CF_SC_MIXOLYDIAN}, .meter_weight={1,10,1},
        .fx={{FXP_WIDE_DRIFT,FXP_PHASE_SWEEP,FXP_BITE,FXP_SLAP},
             {FXP_CLEAN,FXP_BITE,FXP_VELVET,FXP_SLAP},
             {FXP_PING_PONG,FXP_ORBIT,FXP_PHASE_SWEEP,FXP_SLAP},
             {FXP_CLEAN,FXP_BITE,FXP_VELVET,FXP_SILVER_PLATE}},
        .primary=CF_P_DARK, .secondary=CF_P_ENERGETIC, .family=CGS_ELECTRONIC,
        .cabinet=CGS_CAB_ALUMINUM,
        CGS_KITS(DMK_909,DMK_XTECH,DMK_808,DMK_CRUSH,DMK_XCLUB,DMK_MRETRO)
        CGS_PALETTE(CGS_EXTRA_WAVE,STUDIO_VA_AFTERIMAGE,STUDIO_VA_MONO_SHIFT,STUDIO_VA_CHROME_ARP)
        .bank_duration_scale=1, .evolve_period_scale=1, .layer_start=2, .layer_floor=60,
        .bass_style=CGS_BASS_OCTAVE_PULSE, .bass_reply={CGS_BASS_REPLY_NONE,CGS_BASS_REPLY_LOCK},
        .pocket={.chords=0x1111,.kick=0x4141,.kick_answer=0x4441,.gate=7,.snare=4,.hat_gap=2},
        .bass={
            {CGS_FOLLOW,{{0,CGS_ANCHOR,5},{2,CGS_OCTAVE,5},{4,CGS_FIFTH,5},{8,CGS_ANCHOR,5},{14,CGS_OCTAVE,4}}},
            {CGS_FOLLOW,{{0,CGS_ANCHOR,6},{2,CGS_FIFTH,6},{6,CGS_OCTAVE,6},{10,CGS_THIRD,6},{15,CGS_PICKUP,3}}},
        },
        .style={{"Stab","Sustain"},{"Octaves","Moving pulse"},{"Steps","Sequenced"},{"Machine","Open hats"}},
        CGS_TONES({"NEON POLY","PWM KEYS","PULSE STACK"}, {"RELAY BASS","DARK PULSE","SYNC RELAY"},
                  {"FROSTWIRE","STAR KEYS","CRYSTAL STEP"})
    },
    {
        .id="pop-punk", .name="Pop Punk", .bpm=148,
        .bpm_min=132, .bpm_max=182, .keys={1,2,4,6,7,9},
        .scales={CF_SC_MAJOR,CF_SC_MINOR,CF_SC_MIXOLYDIAN,CF_SC_MAJOR}, .meter_weight={1,10,1},
        .fx={{FXP_BITE,FXP_VELVET,FXP_SLAP,FXP_RADIO},
             {FXP_BITE,FXP_VELVET,FXP_CLEAN,FXP_SLAP},
             {FXP_SLAP,FXP_WIDE_DRIFT,FXP_SILVER_PLATE,FXP_PING_PONG},
             {FXP_CLEAN,FXP_BITE,FXP_VELVET,FXP_SILVER_PLATE}},
        .primary=CF_P_DARK, .secondary=CF_P_EMOTIONAL, .family=CGS_GUITAR,
        CGS_KITS(DMK_XBREAK,DMK_ROOM,DMK_XLIVE,DMK_CRUSH,DMK_TIGHT,DMK_BOOM)
        CGS_PALETTE(CGS_EXTRA_PUNK,STUDIO_VA_STACK_BITE,STUDIO_VA_BREAK_PICK,STUDIO_VA_CLEAN_OCTAVE)
        .composer=CGS_COMPOSE_PUNK, .feel_count=4,
        .bank_duration_scale=1, .evolve_period_scale=1, .layer_floor=60, .start_curated=1,
        .style={{"Palm mute","Open wall"},{"Pick drive","Octave hook"},{"Picked hook","Octave answer"},{"Heavy pocket","Double kick"}},
        CGS_TONES({"AMP EDGE","POLY KEYS","AMP STACK","BARBED AMP"},
                  {"DOWN PICK","WOOD PLUCK","MUTED WIRE","POWER PICK","RUST PICK"},
                  {"BRIDGE CHIME","ROOM KEYS","GLASS THREAD","PICK CHIME"})
    },
    {
        .id="lo-fi", .name="Lo-Fi", .bpm=82,
        .bpm_min=66, .bpm_max=96, .keys={0,2,3,5,7,10},
        .scales={CF_SC_MINOR,CF_SC_DORIAN,CF_SC_MAJOR,CF_SC_MIXOLYDIAN}, .meter_weight={2,8,2},
        .fx={{FXP_VELVET,FXP_PHONE_LP,FXP_SILVER_PLATE,FXP_SLAP},
             {FXP_CLEAN,FXP_VELVET,FXP_BITE,FXP_SLAP},
             {FXP_SLAP,FXP_SILVER_PLATE,FXP_DEEP_SPACE,FXP_WIDE_DRIFT},
             {FXP_CLEAN,FXP_VELVET,FXP_PHONE_LP,FXP_SILVER_PLATE}},
        .primary=CF_P_JAZZY, .secondary=CF_P_DREAMY, .family=CGS_ELECTRONIC,
        .cabinet=CGS_CAB_TOLEX,
        CGS_KITS(DMK_DUST,DMK_TAPE,DMK_XLOFI,DMK_BRUSH,DMK_LOFI,DMK_CHILL)
        CGS_PALETTE(CGS_EXTRA_LOFI,STUDIO_VA_WORN_REED,STUDIO_VA_DUST_ROUND,STUDIO_VA_GRAIN_KEYS)
        .bank_duration_scale=1, .evolve_period_scale=1, .layer_floor=60,
        .bass_style=CGS_BASS_LATE, .layer_style=CGS_LAYER_FRAGMENT,
        .bass_reply={CGS_BASS_REPLY_LATE,CGS_BASS_REPLY_NONE},
        .layer_reply={CGS_LAYER_REPLY_SNARE,CGS_LAYER_REPLY_NONE},
        .pocket={.chords=0x0881,.kick=0x0881,.gate=11,.snare=8,.hat_gap=4,.drift=4},
        .bass={
            {CGS_FLOAT,{{0,CGS_ANCHOR,14},{5,CGS_OCTAVE,4},{7,CGS_THIRD,8},{11,CGS_FIFTH,10},{15,CGS_STEP_IN,3}}},
            {CGS_ANSWER,{{0,CGS_ANCHOR,17},{5,CGS_FIFTH,5},{8,CGS_UPPER,8},{13,CGS_THIRD,5},{15,CGS_PICKUP,3}}},
        },
        .style={{"Soft stabs","Held keys"},{"Late pocket","Walk"},{"Fragments","Soft bell"},{"Half-time","Loose hats"}},
        CGS_TONES({"DUSTED KEYS","VINYL KEYS","TAPE EP"}, {"TAPE FINGER","WARM TRIANGL","FELT SUB"},
                  {"DIM CHIME","UNEVEN BELLS","DUST TINES"})
    },
    {
        .id="ambient", .name="Ambient Soundscape", .bpm=64,
        .bpm_min=48, .bpm_max=78, .keys={0,2,4,5,7,9},
        .scales={CF_SC_LYDIAN,CF_SC_DORIAN,CF_SC_MAJOR,CF_SC_MINOR}, .meter_weight={4,3,5},
        .fx={{FXP_SILVER_PLATE,FXP_WIDE_DRIFT,FXP_VELVET,FXP_DEEP_SPACE},
             {FXP_CLEAN,FXP_VELVET,FXP_SLAP,FXP_BITE},
             {FXP_DEEP_SPACE,FXP_ORBIT,FXP_WIDE_DRIFT,FXP_SLAP},
             {FXP_CLEAN,FXP_VELVET,FXP_SILVER_PLATE,FXP_SLAP}},
        .primary=CF_P_DREAMY, .secondary=CF_P_UPLIFTING, .family=CGS_EXPERIMENTAL,
        .cabinet=CGS_CAB_ALUMINUM,
        CGS_KITS(DMK_CHILL,DMK_GLASS,DMK_AIR,DMK_DUB,DMK_WOOD,DMK_HAND)
        CGS_PALETTE(CGS_EXTRA_AMBIENT,STUDIO_VA_TIDAL_DRONE,STUDIO_VA_DEEP_ANCHOR,STUDIO_VA_AIR_LATTICE)
        .bank_duration_scale=2, .evolve_period_scale=2, .layer_floor=60, .start_curated=1,
        .bass_style=CGS_BASS_PEDAL, .layer_style=CGS_LAYER_SWELL, .hold_chords=1, .sparse_drums=1,
        .layer_reply={CGS_LAYER_REPLY_HARMONY,CGS_LAYER_REPLY_NONE},
        .bass={
            {CGS_FLOAT,{{0,CGS_ANCHOR,31},{6,CGS_FIFTH,31},{9,CGS_FIFTH,29},{12,CGS_ANCHOR,31},{15,CGS_STEP_IN,3}}},
            {CGS_FLOAT,{{0,CGS_ANCHOR,31},{5,CGS_THIRD,30},{8,CGS_OCTAVE,31},{12,CGS_FIFTH,31},{15,CGS_UPPER,6}}},
        },
        .style={{"Hold","Breathe"},{"Pedal","Slow contour"},{"Swells","Distant notes"},{"Sparse","Texture"}},
        CGS_TONES({"SLOW ORBIT","PRISM CLOUD","STILL AIR"}, {"FLOOR SWELL","SLOW BLOOM B","LOW TIDE"},
                  {"MOTION PAD","WATER CHIME","SOFT HALO"})
    },
};
#undef CGS_TONES
#undef CGS_PALETTE
#undef CGS_KITS

enum { CGS_DEFAULT_GENRE=0, CGS_NGENRE=NELEM(CGS_PROFILES) };
_Static_assert(CGS_NGENRE > 1 && CGS_NGENRE <= UINT8_MAX, "Studio genre index fits in a byte");

#endif
