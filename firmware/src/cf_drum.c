/* SPDX-License-Identifier: GPL-3.0-only
 * CoralFlower: the drum machine (0.6; the owner, 2026-10-28: "Да, подходят"). Ready rhythms of 1 or 2 bars, each for
 * one TIME SIG, played on the drums' part (the DRUM engine and the second one's kits, as a pattern's DRUMS) while the
 * keys play chords on HOME. The audio side: the rhythms and their player. Included by cf_out.c after cf_seqp.c: a
 * rhythm and the sequencer never play together (one thing plays: the player stops the other as it starts).
 *
 * A rhythm is its drums' hits as text, one string a drum, its bars one after another ('x' a hit, '.' none): the
 * drums' grid of the metronome's beats x 4 steps (x 2 in a bar of more than 4 beats, sq_dsteps), as a pattern's
 * DRUMS. Written for CoralFlower from the common grooves of each style, no other instrument's patterns. */

enum { DM_K, DM_S, DM_C, DM_H, DM_O, DM_LT, DM_MT, DM_HT, DM_R, DM_B, DM_X };   /* (the lanes: cf_seqd.c SQ_NLANE) */
typedef struct {
    const char *name;            /* 9 letters at most: "Breakcore" fits the page's title and the plaque */
    uint8_t id;                  /* its own number, never reused: what the user's KIT and SWING and the rhythm stood on
                                  * are kept by (cf_dmui.c); rc3's rhythms kept theirs (0 .. 28), new ones go on from 29 */
    uint8_t beats, bars;         /* its TIME SIG (cs.beats), its length */
    uint8_t kit, swing;          /* its own kit (SQ_KITS) and SWING (0..100 %, as a pattern's) */
    uint8_t bpm;                 /* the tempo its style lives at (shown, never set: the tempo is the user's) */
    const char *lane[SQ_NLANE];  /* each drum's hits, bar after bar; 0: the drum is not in it */
    uint8_t lev[SQ_NLANE];       /* .. its level, 0..100 % (a pattern's dlev) */
} dm_rhythm_t;

/* the kits by name (SQ_KITS' order) */
enum { DMK_STD, DMK_808, DMK_909, DMK_TIGHT, DMK_BOOM, DMK_LOFI, DMK_HAND, DMK_CRUSH, DMK_XCLUB, DMK_XDEEP, DMK_XLOFI,
       DMK_XLIVE, DMK_XTECH, DMK_XBREAK,
       DMK_DUST, DMK_TAPE, DMK_BRUSH, DMK_AIR, DMK_GLASS, DMK_DUB, DMK_WOOD, DMK_CHILL, DMK_MRETRO=22, DMK_ROOM=24 };

static uint32_t cf_perf_kit_style(uint32_t kit)
{
    return kit == DMK_808 || kit == DMK_CRUSH || kit == DMK_CHILL ? CFPERF_TIGHT : CFPERF_LOOSE;
}

/* Chase drum refresh: original two-bar patterns informed by the listening/production references in
 * docs/DRUM-LOOPS.md. Lo-fi, hip-hop, trap and gaze lead the browser; the other styles and metres follow.
 * Existing rhythm IDs retain their identity and saved KIT/SWING overrides. IDs 51..62 are new; retired
 * upstream IDs stay retired. The bank uses the existing fixed grid and lane levels, so EDIT/REC copies
 * the complete groove without changing the sequencer's storage format. */
static const dm_rhythm_t DM_RHY[] = {
    {"Lo-fi", 30, 4, 2, DMK_DUST, 38, 78,
     {[DM_K] = "x......x..x....." "x.....x..x.....x",
      [DM_S] = "....x.......x..." "....x.......x...",
      [DM_H] = "x.x.x.x.x.x.x..." "x.x.x.x.x.x.x.x.",
      [DM_R] = "...........x...." "..............x."},
     {[DM_K] = 88, [DM_S] = 72, [DM_H] = 31, [DM_R] = 24}},
    {"HipHop", 7, 4, 2, DMK_XLOFI, 28, 92,
     {[DM_K] = "x..x......x....." "x.....xx.x.....x",
      [DM_S] = "....x.......x..." "....x.......x...",
      [DM_H] = "x.x.x.x.x.x.x.x." "x.x.x.x.x.x.xx..",
      [DM_O] = "................" "..............x.",
      [DM_R] = ".......x........" "...........x...."},
     {[DM_K] = 96, [DM_S] = 86, [DM_H] = 38, [DM_O] = 32, [DM_R] = 25}},
    {"NeoSoul", 61, 4, 2, DMK_BRUSH, 32, 86,
     {[DM_K] = "x......x.x......" "x.....x.......x.",
      [DM_R] = "....x.......x..." "....x.......x...",
      [DM_H] = "x.x..xx.x.x...x." "x.x..x.xx.x..x..",
      [DM_O] = "................" "..............x."},
     {[DM_K] = 88, [DM_R] = 67, [DM_H] = 32, [DM_O] = 27}},
    {"TripHop", 58, 4, 2, DMK_TAPE, 18, 82,
     {[DM_K] = "x.........x....." "x..x..........x.",
      [DM_S] = "....x.......x..." "....x.......x...",
      [DM_H] = "x.x...x.x.x...x." "x.x...x.x.x..x..",
      [DM_LT] = "................" "...........x....",
      [DM_R] = ".......x........" "...............x"},
     {[DM_K] = 92, [DM_S] = 76, [DM_H] = 30, [DM_LT] = 38, [DM_R] = 25}},
    {"Trap", 51, 4, 2, DMK_808, 0, 142,
     {[DM_K] = "x.....x.....x..." "x......x..x...x.",
      [DM_S] = "........x......." "........x.......",
      [DM_C] = "........x......." "........x.......",
      [DM_H] = "x.x.x...x.xxx.x." "x.x.xxx.x.x..xxx",
      [DM_O] = "......x........." "............x..."},
     {[DM_K] = 98, [DM_S] = 76, [DM_C] = 34, [DM_H] = 35, [DM_O] = 33}},
    {"CloudTrap", 52, 4, 2, DMK_CHILL, 0, 136,
     {[DM_K] = "x.........x....." "......x.......x.",
      [DM_C] = "........x......." "........x.......",
      [DM_H] = "x.x...x.x.x....." "x.x...x.x.x..xxx",
      [DM_O] = "..............x." "................",
      [DM_R] = "................" "............x..."},
     {[DM_K] = 92, [DM_C] = 73, [DM_H] = 29, [DM_O] = 29, [DM_R] = 24}},
    {"Plugg", 53, 4, 2, DMK_808, 8, 144,
     {[DM_K] = "x..........x...." "......x.......x.",
      [DM_C] = "........x......." "........x.......",
      [DM_H] = "x...x.x.....x.x." "x.x.......x.x...",
      [DM_R] = "..............x." ".....x..........",
      [DM_O] = "................" "..............x."},
     {[DM_K] = 94, [DM_C] = 78, [DM_H] = 32, [DM_R] = 32, [DM_O] = 28}},
    {"Rage", 54, 4, 2, DMK_CRUSH, 0, 150,
     {[DM_K] = "x.....x.....x.x." "x.........xx..x.",
      [DM_C] = "........x......." "........x.......",
      [DM_H] = "x.x.x...xxxxx.x." "x.x.x.xxx.x..xxx",
      [DM_O] = "......x........." "............x...",
      [DM_X] = "x..............." "................"},
     {[DM_K] = 86, [DM_C] = 70, [DM_H] = 25, [DM_O] = 26, [DM_X] = 24}},
    {"NuGaze", 55, 4, 2, DMK_XLIVE, 0, 126,
     {[DM_K] = "x.....x.x.x....." "x......xx.....x.",
      [DM_S] = "....x.......x..." "....x.......x...",
      [DM_H] = "x.x.x.x.x.x.x.x." "x.x.x.x.x.x.....",
      [DM_O] = "................" "............x...",
      [DM_MT] = "................" ".............x..",
      [DM_LT] = "................" "...............x",
      [DM_X] = "x..............." "................"},
     {[DM_K] = 95, [DM_S] = 88, [DM_H] = 44, [DM_O] = 43, [DM_MT] = 58, [DM_LT] = 63, [DM_X] = 40}},
    {"DreamPop", 56, 4, 2, DMK_AIR, 0, 104,
     {[DM_K] = "x.......x.....x." "x.....x.x.......",
      [DM_S] = "....x.......x..." "....x.......x...",
      [DM_H] = "x.x.x.x.x.x.x.x." "x.x.x.x.x.x.x...",
      [DM_O] = "................" "..............x.",
      [DM_R] = "...........x...." "................"},
     {[DM_K] = 88, [DM_S] = 72, [DM_H] = 32, [DM_O] = 34, [DM_R] = 23}},
    {"Slowcore", 57, 4, 2, DMK_XLIVE, 0, 76,
     {[DM_K] = "x.......x......." "x.......x..x....",
      [DM_S] = "....x.......x..." "....x.......x...",
      [DM_H] = "x...x...x...x..." "x...x...x.......",
      [DM_O] = "................" "............x...",
      [DM_LT] = "................" "..............x."},
     {[DM_K] = 89, [DM_S] = 78, [DM_H] = 32, [DM_O] = 33, [DM_LT] = 49}},
    {"LoHouse", 59, 4, 2, DMK_DUST, 24, 118,
     {[DM_K] = "x...x...x...x..." "x...x...x..xx...",
      [DM_C] = "....x.......x..." "....x.......x...",
      [DM_H] = "..x...x...x...x." "..x...x...x...x.",
      [DM_LT] = ".......x......x." "...x......x.....",
      [DM_R] = "...............x" ".......x........"},
     {[DM_K] = 93, [DM_C] = 72, [DM_H] = 36, [DM_LT] = 42, [DM_R] = 24}},
    {"DeepHouse", 60, 4, 2, DMK_XDEEP, 14, 122,
     {[DM_K] = "x...x...x...x..." "x...x...x...x...",
      [DM_C] = "....x.......x..." "....x.......x...",
      [DM_H] = "...x.......x...." ".x.....x.......x",
      [DM_O] = "..x...x...x...x." "..x...x...x...x.",
      [DM_LT] = "......x........x" "...x......x....."},
     {[DM_K] = 95, [DM_C] = 73, [DM_H] = 29, [DM_O] = 44, [DM_LT] = 39}},
    {"House", 4, 4, 2, DMK_909, 8, 126,
     {[DM_K] = "x...x...x...x..." "x...x...x...x..x",
      [DM_C] = "....x.......x..." "....x.......x...",
      [DM_H] = "...x...x...x...x" ".x.x...x.x.x....",
      [DM_O] = "..x...x...x...x." "..x...x...x...x.",
      [DM_R] = "..........x....." "......x.......x."},
     {[DM_K] = 96, [DM_C] = 77, [DM_H] = 27, [DM_O] = 46, [DM_R] = 32}},
    {"Garage", 18, 4, 2, DMK_XCLUB, 58, 132,
     {[DM_K] = "x.........x....." "x.x.......x....x",
      [DM_S] = "....x.......x..." "....x.......x...",
      [DM_H] = "..xx..x..xx..x.." "..x..x....xx.xx.",
      [DM_O] = "..............x." "....x...........",
      [DM_R] = "...x.....x......" ".......x......x."},
     {[DM_K] = 96, [DM_S] = 83, [DM_H] = 33, [DM_O] = 37, [DM_R] = 29}},
    {"SpeedG", 62, 4, 2, DMK_XCLUB, 40, 138,
     {[DM_K] = "x...x...x...x..." "x...x...x...x..x",
      [DM_S] = "....x.......x..." "....x.......x...",
      [DM_H] = "...x..xx...x..x." ".x.x..x....x..xx",
      [DM_O] = "..x.......x....." "..x.......x.....",
      [DM_R] = ".......x......x." "...x.....x......"},
     {[DM_K] = 96, [DM_S] = 81, [DM_H] = 32, [DM_O] = 40, [DM_R] = 27}},
    {"Pop", 1, 4, 2, DMK_TAPE, 10, 108,
     {[DM_K] = "x.....x.x......." "x.......x.x....x",
      [DM_S] = "....x.......x..." "....x.......x...",
      [DM_C] = "............x..." "............x...",
      [DM_H] = "x.x.x.x.x.x.x.x." "x.x.x.x.x.x.x...",
      [DM_O] = "................" "..............x."},
     {[DM_K] = 92, [DM_S] = 75, [DM_C] = 38, [DM_H] = 34, [DM_O] = 35}},
    {"Funk", 2, 4, 2, DMK_TIGHT, 34, 106,
     {[DM_K] = "x..x..x...x....." "x.x......x..x..x",
      [DM_S] = "....x.......x..." "....x.......x...",
      [DM_H] = "xxxx.xxxxxxx.xx." "xxxx.xxxxxxx.x..",
      [DM_R] = ".......x.x......" "..x....x......x.",
      [DM_O] = "................" "..............x."},
     {[DM_K] = 94, [DM_S] = 84, [DM_H] = 32, [DM_R] = 29, [DM_O] = 36}},
    {"Techno", 5, 4, 2, DMK_XTECH, 0, 134,
     {[DM_K] = "x...x...x...x..." "x...x...x...x...",
      [DM_C] = "....x.......x..." "....x.......x...",
      [DM_H] = "...x.x.....x.x.." ".x.....x.x.x...x",
      [DM_O] = "..x...x...x...x." "..x...x...x...x.",
      [DM_LT] = "...x......x....." "......x........x",
      [DM_R] = "......x........x" "...x.......x...."},
     {[DM_K] = 96, [DM_C] = 60, [DM_H] = 28, [DM_O] = 42, [DM_LT] = 39, [DM_R] = 32}},
    {"Break", 13, 4, 2, DMK_XBREAK, 8, 132,
     {[DM_K] = "x.x.......x....." "x.....x...x....x",
      [DM_S] = "....x..x....x..." "....x....x..x..x",
      [DM_H] = "x.x.x.x.x.x.x.x." "x.x.x.x.x.x.x...",
      [DM_O] = "................" "..............x.",
      [DM_R] = ".........x......" ".......x........"},
     {[DM_K] = 97, [DM_S] = 86, [DM_H] = 36, [DM_O] = 39, [DM_R] = 25}},
    {"Jungle", 31, 4, 2, DMK_XBREAK, 0, 164,
     {[DM_K] = "x.........x....." "..x.......x....x",
      [DM_S] = "....x..x.x..x..." "....x.x....x..x.",
      [DM_H] = "x.xxx.x.x.xxx.x." "x.x.x.xxx.x.xx..",
      [DM_O] = "................" "..............x.",
      [DM_R] = "...x......x....x" ".x.......x......"},
     {[DM_K] = 96, [DM_S] = 86, [DM_H] = 30, [DM_O] = 33, [DM_R] = 27}},
    {"DnB", 14, 4, 2, DMK_XTECH, 0, 174,
     {[DM_K] = "x.........x....." "x.....x...x....x",
      [DM_S] = "....x.......x..." "....x.......x...",
      [DM_H] = "x.xxx.x.x.xxx.x." "x.x.x.xxx.x.xx..",
      [DM_O] = "................" "..............x.",
      [DM_R] = ".......x.x......" "..x........x...x"},
     {[DM_K] = 98, [DM_S] = 90, [DM_H] = 34, [DM_O] = 34, [DM_R] = 27}},
    {"Halftime", 38, 4, 2, DMK_DUB, 0, 140,
     {[DM_K] = "x.........x....." ".......xx.....x.",
      [DM_S] = "........x......." "........x.......",
      [DM_H] = "x.x...x.x.x...x." "x....x....x.x...",
      [DM_R] = ".......x......x." "...x...........x",
      [DM_LT] = "................" "......x........."},
     {[DM_K] = 94, [DM_S] = 84, [DM_H] = 29, [DM_R] = 25, [DM_LT] = 37}},
    {"Drill", 35, 4, 2, DMK_XTECH, 12, 142,
     {[DM_K] = "x......x....x..." "..x....x..x.....",
      [DM_S] = "........x......." "............x...",
      [DM_H] = "x..x..x.x..x...." "x..x..x.x.xx..x.",
      [DM_O] = "..............x." "....x...........",
      [DM_R] = "................" "...........x...."},
     {[DM_K] = 98, [DM_S] = 86, [DM_H] = 33, [DM_O] = 34, [DM_R] = 26}},
    {"Grime", 36, 4, 2, DMK_TIGHT, 0, 140,
     {[DM_K] = "x.....x......x.." "x..x......x.....",
      [DM_S] = "........x......." "........x.......",
      [DM_H] = "x...x...x...x..." "x...x...x.x...x.",
      [DM_C] = "................" "........x.......",
      [DM_R] = ".......x......x." "..x........x...."},
     {[DM_K] = 96, [DM_S] = 87, [DM_H] = 34, [DM_C] = 30, [DM_R] = 31}},
    {"Jersey", 32, 4, 2, DMK_XCLUB, 0, 140,
     {[DM_K] = "x...x...x..x..x." "x...x...x..x.xx.",
      [DM_C] = "....x.......x..." "....x.......x...",
      [DM_R] = "......x.......x." "......x...x...x.",
      [DM_O] = "..x...x...x....." "..x...x.........",
      [DM_H] = "...............x" "...............x"},
     {[DM_K] = 96, [DM_C] = 80, [DM_R] = 43, [DM_O] = 34, [DM_H] = 28}},
    {"Hyperpop", 34, 4, 2, DMK_CRUSH, 0, 152,
     {[DM_K] = "x.....x.x.....x." "x...x..x..x....x",
      [DM_C] = "....x.......x..." "....x.......x...",
      [DM_H] = "x.x.x.xxx.x.x.x." "x.x.x.x.xxxxxx..",
      [DM_O] = "................" "..............x.",
      [DM_X] = "x..............." "................"},
     {[DM_K] = 85, [DM_C] = 70, [DM_H] = 25, [DM_O] = 28, [DM_X] = 23}},
    {"Breakcore", 39, 4, 2, DMK_XBREAK, 0, 180,
     {[DM_K] = "x.x.......xx...." "x..x..x.x....x..",
      [DM_S] = "....x.xx.x..x.x." "..x.x..x.xx.x.xx",
      [DM_H] = "x.xx.xx.x.xx.xx." "xx.xx.x.xx.xx.x.",
      [DM_R] = "...x...........x" ".....x..........",
      [DM_X] = "x..............." "................"},
     {[DM_K] = 92, [DM_S] = 78, [DM_H] = 25, [DM_R] = 26, [DM_X] = 26}},
    {"Amapiano", 40, 4, 2, DMK_XDEEP, 16, 112,
     {[DM_K] = "x...x...x...x..." "x...x...x...x...",
      [DM_LT] = "...x..x...x..x.." "..x..x....xx...x",
      [DM_MT] = "...............x" ".......x........",
      [DM_R] = "....x.......x..." "....x.......x...",
      [DM_H] = "..x...xx..x...x." "..xx..x...x...xx"},
     {[DM_K] = 79, [DM_LT] = 84, [DM_MT] = 56, [DM_R] = 48, [DM_H] = 29}},
    {"Baile", 41, 4, 2, DMK_808, 0, 130,
     {[DM_K] = "x.....x...x....." "x.....x...x.x...",
      [DM_LT] = "..x.....x.....x." "..x.....x.....x.",
      [DM_MT] = "...x.....x...x.." "...x.....x...x.x",
      [DM_C] = "....x.......x..." "....x.......x...",
      [DM_H] = "......x.......x." "......x........."},
     {[DM_K] = 93, [DM_LT] = 73, [DM_MT] = 69, [DM_C] = 65, [DM_H] = 28}},
    {"Dembow", 42, 4, 2, DMK_XCLUB, 0, 96,
     {[DM_K] = "x...x...x...x..." "x...x...x...x...",
      [DM_S] = "...x..x....x..x." "...x..x....x..x.",
      [DM_H] = "x.x.x.x.x.x.x..." "x.x.x.x.x.x.x.x.",
      [DM_R] = "................" ".......x.......x"},
     {[DM_K] = 93, [DM_S] = 79, [DM_H] = 30, [DM_R] = 27}},
    {"PopRock", 29, 4, 2, DMK_XLIVE, 0, 122,
     {[DM_K] = "x.......x.x....." "x.....x.x.....x.",
      [DM_S] = "....x.......x..." "....x.......x...",
      [DM_H] = "x.x.x.x.x.x.x.x." "x.x.x.x.x.x.....",
      [DM_MT] = "................" "............x.x.",
      [DM_LT] = "................" "...............x",
      [DM_X] = "x..............." "................"},
     {[DM_K] = 94, [DM_S] = 87, [DM_H] = 43, [DM_MT] = 55, [DM_LT] = 61, [DM_X] = 38}},
    {"Ballad", 48, 6, 2, DMK_BRUSH, 0, 66,
     {[DM_K] = "x.......x..." "x.........x.",
      [DM_R] = "......x....." "......x.....",
      [DM_H] = "x.x.x.x.x.x." "x.x.x.x.x...",
      [DM_LT] = "............" "...........x"},
     {[DM_K] = 84, [DM_R] = 63, [DM_H] = 28, [DM_LT] = 35}},
    {"Electro", 49, 6, 2, DMK_DUB, 0, 92,
     {[DM_K] = "x.......x..." "x..x......x.",
      [DM_C] = "......x....." "......x.....",
      [DM_H] = "x.x.x.x.x.x." "x.x.x.x.x...",
      [DM_O] = "............" "..........x.",
      [DM_B] = "....x......." "...x.....x.."},
     {[DM_K] = 91, [DM_C] = 69, [DM_H] = 30, [DM_O] = 31, [DM_B] = 23}},
    {"Lo-Fi 6", 50, 6, 2, DMK_DUST, 0, 72,
     {[DM_K] = "x..........." "x........x..",
      [DM_S] = "......x....." "......x.....",
      [DM_H] = "x.x.x.x.x.x." "x.x.x.x.x...",
      [DM_R] = ".........x.." "............",
      [DM_O] = "............" "..........x."},
     {[DM_K] = 86, [DM_S] = 65, [DM_H] = 27, [DM_R] = 23, [DM_O] = 28}},
    {"Tamborzao", 47, 2, 2, DMK_808, 0, 130,
     {[DM_K] = "x..x..x." "x..x..x.",
      [DM_MT] = ".x...x.." ".x...x.x",
      [DM_LT] = "..x....x" "..x...x.",
      [DM_C] = "....x..." "....x...",
      [DM_H] = "......x." "........"},
     {[DM_K] = 93, [DM_MT] = 69, [DM_LT] = 74, [DM_C] = 65, [DM_H] = 27}},
    {"MathRock", 45, 5, 2, DMK_XLIVE, 0, 140,
     {[DM_K] = "x..x..x..." "x....xx...",
      [DM_S] = "..x....x.." "..x....x.x",
      [DM_H] = "xx.xx.x..." "xx.xx.x...",
      [DM_O] = "........x." "........x.",
      [DM_LT] = ".........." ".........x"},
     {[DM_K] = 91, [DM_S] = 82, [DM_H] = 36, [DM_O] = 34, [DM_LT] = 50}},
    {"MathRock", 46, 7, 2, DMK_XLIVE, 0, 130,
     {[DM_K] = "x...x.....x.x." "x..x....x...x.",
      [DM_S] = "..x...x...x..." "..x...x...x..x",
      [DM_H] = "xxxxxxxxxxxx.." "xxxxxxxxxxx...",
      [DM_O] = "............x." "...........x..",
      [DM_LT] = ".............." "............x."},
     {[DM_K] = 91, [DM_S] = 83, [DM_H] = 34, [DM_O] = 33, [DM_LT] = 49}},
    {"Waltz", 63, 3, 2, DMK_BRUSH, 0, 90,
     {[DM_K] = "x..........." "x.......x...",
      [DM_R] = "....x...x..." "....x...x...",
      [DM_H] = "x.x.x.x.x.x." "x.x.x.x.x.x."},
     {[DM_K] = 85, [DM_R] = 65, [DM_H] = 30}},
};
#define DM_N ((uint32_t)(sizeof DM_RHY / sizeof DM_RHY[0]))

static uint32_t dmp_fill_style(uint32_t r)
{
    uint32_t id = DM_RHY[r % DM_N].id;
    return (id >= 51u && id <= 54u) || id == 35u || id == 36u ? CFPERF_TIGHT : CFPERF_LOOSE;
}

/* the rhythms of a TIME SIG: how many, the k-th (DM_N: none) */
static uint32_t dm_count(uint32_t beats)
{
    uint32_t i, n = 0;
    for (i = 0; i < DM_N; i++)
        n += DM_RHY[i].beats == beats;
    return n;
}
static uint32_t dm_nth(uint32_t beats, uint32_t k)
{
    uint32_t i;
    for (i = 0; i < DM_N; i++)
        if (DM_RHY[i].beats == beats && !k--)
            return i;
    return DM_N;
}
static uint32_t dm_place(uint32_t r)             /* .. its place in its TIME SIG's list */
{
    uint32_t i, k = 0;
    for (i = 0; i < r && i < DM_N; i++)
        k += DM_RHY[i].beats == DM_RHY[r % DM_N].beats;
    return k;
}
/* a bar's drum steps in a TIME SIG of so many beats (as cf_seqd.c sq_dsteps for the one now) */
static uint32_t dm_steps(uint32_t beats) { return beats * 4u <= 16u ? beats * 4u : beats * 2u; }
/* drum l's hits in bar b of rhythm r: a pattern's DRUMS bits (bit i, step i), in the rhythm's own TIME SIG */
static uint32_t dm_mask(uint32_t r, uint32_t l, uint32_t b)
{
    const dm_rhythm_t *y = &DM_RHY[r % DM_N];
    const char *s = y->lane[l % SQ_NLANE];
    uint32_t sd = dm_steps(y->beats), i, m = 0;
    if (!s || b >= y->bars)
        return 0;
    s += b * sd;
    for (i = 0; i < sd && s[i]; i++)
        if (s[i] != '.')
            m |= 1u << i;
    return m;
}

/* ------------------------------------------------------------ the player --- */
/* PLAY in the drum machine (or on HOME, the rhythm last chosen): its bars round, on the sequencer's clock (96 ticks a
 * beat from the tempo; MIDI clock in: its pulses), the metronome's beat 1 at its start. Another rhythm picked while it
 * plays comes in at the next bar line, from its first bar. REC in the SEQUENCER while it plays: it ends its bar (the
 * next too, with less than a beat of it left: the player gets ready), then the pattern on screen plays and the take
 * goes into it (rin) */
static struct {
    uint8_t on;                  /* a rhythm plays */
    uint8_t r, kit, swing;       /* .. which, its kit and SWING as played (its own or the user's) */
    uint8_t next, nkit, nswing;  /* .. the one coming in at the next bar line (0xFF none) */
    uint8_t bar;                 /* .. its bar, 0 .. its bars - 1 */
    uint32_t nbar;               /* .. bars since it began */
    uint32_t t, acc;             /* .. ticks into the bar; the clock's samples x bpm */
    uint8_t nh;                  /* .. the next step not struck yet */
    uint8_t rin, rtrk, rbars;    /* REC in the SEQUENCER: pattern + 1 (0 none), its track, bar lines to wait */
} dmp = {.next = 0xFF};

static uint32_t dmp_tick_of(uint32_t i)          /* where step i sounds: every second one late by SWING */
{
    uint32_t bt = sqp_bar_ticks(), sd = sq_dsteps(), t = i * bt / sd;
    if (i & 1u)
        t += dmp.swing * (bt / sd) / 200u;
    return t;
}
static void dmp_fire(uint32_t h)
{
    const dm_rhythm_t *y = &DM_RHY[dmp.r % DM_N];
    uint32_t l;
    for (l = 0; l < SQ_NLANE; l++)
        if (cf_perf_hit(l, h, sq_dsteps(), (dm_mask(dmp.r, l, dmp.bar) >> h) & 1u)) {
            sqp_drum_hit(l,dmp.kit,cf_perf_level(l,h,sq_dsteps(),y->lev[l]),100);
        }
}
static void dmp_now(void)
{
    while (dmp.nh < sq_dsteps() && dmp.t >= dmp_tick_of(dmp.nh))
        dmp_fire(dmp.nh++);
}
static void dmp_stop(void)
{
    if (!dmp.on)
        return;
    dmp.on = 0;
    cf_perf_reset();
    dmp.next = 0xFF;
    dmp.rin = 0;
    sqp_drum_offs();
    if (!sqp.on)
        cfm.running = 0;
}
static void dmp_start(uint32_t r, uint32_t kit, uint32_t swing)
{
    cpa_stop();
    sqp_stop();
    cf_perf_reset();
    dmp.r = (uint8_t)(r % DM_N);
    dmp.kit = (uint8_t)kit;
    dmp.swing = (uint8_t)swing;
    dmp.next = 0xFF;
    dmp.rin = 0;
    dmp.bar = 0;
    dmp.nbar = 0;
    dmp.t = 0;
    dmp.acc = 0;
    dmp.nh = 0;
    dmp.on = 1;
    cfm_restart();
    cfm.running = 1;
    dmp_now();
}
/* REC in the SEQUENCER while a rhythm plays: its bar ends first, then pattern pat plays from its top, the take into
 * track trk */
static void dmp_rec_in(uint32_t pat, uint32_t trk)
{
    cf_perf_reset();
    uint32_t bt = sqp_bar_ticks();
    if (!dmp.on) {
        sqp_rec_start(pat, trk);
        return;
    }
    dmp.rin = (uint8_t)(pat + 1u);
    dmp.rtrk = (uint8_t)trk;
    dmp.rbars = (uint8_t)(bt - dmp.t < SQ_PPQN ? 2u : 1u);
}
static void dmp_next_bar(void)
{
    const dm_rhythm_t *y;
    dmp.t = 0;
    dmp.nh = 0;
    if (dmp.rin && !--dmp.rbars) {               /* (REC in: the pattern takes over at this bar line) */
        uint32_t pat = dmp.rin - 1u, trk = dmp.rtrk;
        dmp_stop();
        sqp_begin(pat, 0, 0);
        sqp_rec_set(trk);
        return;
    }
    if (dmp.next != 0xFFu) {
        dmp.r = dmp.next;
        dmp.kit = dmp.nkit;
        dmp.swing = dmp.nswing;
        dmp.next = 0xFF;
        dmp.bar = 0;
        dmp.nbar = 0;
        if (DM_RHY[dmp.r % DM_N].beats != cfm.beats) {   /* (a rhythm of another TIME SIG: it comes with it, here at
                                                  * the bar line; the UI takes it into cs.beats, cf_dmui.c dm_poll) */
            cfm.beats = DM_RHY[dmp.r % DM_N].beats;
            sq_beats = cfm.beats;
        }
        return;
    }
    y = &DM_RHY[dmp.r % DM_N];
    dmp.nbar++;
    dmp.bar = (uint8_t)(dmp.nbar % (y->bars ? y->bars : 1u));
}
static void dmp_tick1(void)
{
    sqp_drum_offs();
    if (++dmp.t >= sqp_bar_ticks()) {
        dmp_next_bar();
        if (dmp.on) cf_perf_bar(dmp_fill_style(dmp.r),0);
    }
    if (dmp.on)
        dmp_now();
}
static void dmp_tick(uint32_t n, uint32_t pulses)   /* every audio block, as sqp_tick */
{
    uint32_t k;
    if (!dmp.on)
        return;
    if (cfx_in.active) {
        for (k = 0; k < pulses * (SQ_PPQN / 24u) * cf_meter_rate() && dmp.on; k++)
            dmp_tick1();
        return;
    }
    dmp.acc += n * cfm.bpm * SQ_PPQN * cf_meter_rate();
    while (dmp.acc >= FS * 60u && dmp.on) {
        dmp.acc -= FS * 60u;
        dmp_tick1();
    }
}
/* PLAY on a rhythm, or SELECT in the drum machine while one plays: the next comes in at the bar line */
static void dmp_queue(uint32_t r, uint32_t kit, uint32_t swing)
{
    if (!dmp.on) {
        dmp_start(r, kit, swing);
        return;
    }
    if (r == dmp.r) {                            /* (back to the one playing: it plays on) */
        dmp.next = 0xFF;
        dmp.kit = (uint8_t)kit;
        dmp.swing = (uint8_t)swing;
        return;
    }
    dmp.next = (uint8_t)r;
    dmp.nkit = (uint8_t)kit;
    dmp.nswing = (uint8_t)swing;
}
/* REC under a rhythm playing (on HOME, the drum machine): the new pattern, its DRUMS this rhythm from the bar playing,
 * takes over at once where the bar is, its take into CHORDS (8 bars at most, as from HOME) */
static void dmp_take_over(uint32_t pat)
{
    uint32_t t = dmp.on ? dmp.t : 0u, acc = dmp.acc, bt = sqp_bar_ticks();
    int run = dmp.on;
    dmp.on = 0;
    dmp.next = 0xFF;
    dmp.rin = 0;
    if (!run) {                                  /* (stopped meanwhile: a count-in, as from silence) */
        sqp_rec_start(pat, SQ_CHORDS);
        sqp.rlim = SQ_BARS;
        return;
    }
    sqp_setup(pat, 0);                           /* (the beat goes on where it is) */
    sqp_rec_set(SQ_CHORDS);
    sqp.rlim = SQ_BARS;
    sqp.t = t < bt ? t : 0u;
    sqp.acc = acc;
    while (sqp.nu < SQ_U && sqp.nu * bt / SQ_U < sqp.t)   /* (what lay before is not struck now) */
        sqp.nu++;
    while (sqp.nh < sq_dsteps() && sqp_drum_tick(&sq_st.pat[pat % SQ_NPAT], sqp.nh) < sqp.t)
        sqp.nh++;
    cfm.running = 1;
    sqp_now();
}
