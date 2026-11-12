# RAKT licensing

RAKT is a fork of **CORD** (<https://github.com/chasedeanda/cord-fm1>), which is based on **CoralFlower** (<https://github.com/FicusFicusov/CoralFlower>) and is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`; the full text is
in `LICENSE`). That covers the firmware, the tools, the web pages and the tests in this tree. A few bundled or ported
parts keep their own licences. They are listed below and in `docs/LICENSING-choralroot.md`, and their licence texts
are in `LICENSES/`.

The firmware built from this tree is GPL-3.0-only as a whole. If you distribute it, or firmware derived from it, you
must also give your recipients its complete corresponding source under the same licence.

## Where CoralFlower comes from

CoralFlower is a fork of **ChoralRoot FM-1** by Quixotic7 (GPL-3.0-only, <https://github.com/Quixotic7/ChoralRootFM1>),
which is itself a fork of **Felucca** by Leo Kuroshita (Hügelton Instruments, <https://github.com/hugelton/Felucca>). It keeps their platform: the hardware layer, the
update loader and the installer protocol, the sound engines (FM6, VA), the effects, the user sound stores. The
inherited parts, their authors and their licences are listed in `docs/LICENSING-choralroot.md`. That covers Melodee
by Kerem Kilic, MSFA and Dexed, SLOOP, the JieLi AC79 SDK files, the fonts and the rest. The copyright lines in the
source files are kept as they are.

## CoralFlower's own work

| What | Licence | Where |
| --- | --- | --- |
| The chord instrument: the chord model, the player, the arpeggiator, the looper, the effects chain and its screens, the UI and the screens | GPL-3.0-only | `firmware/src/cf_*.c`, `firmware/src/cf_*.h` |
| The organism (the plant and the coral) and its renderer | GPL-3.0-only | `firmware/src/flora/` |
| CoralFlower's VA presets and FM pad voices (F25..F30) | GPL-3.0-only | `firmware/src/eng_va.c`, `firmware/src/eng_fm6_rom.h` |
| The browser emulator page and the web installer page | GPL-3.0-only | `tools/emu/web/`, `web/` |

The plant's anatomy follows the model of PlantStudio by Paul D. Fernhout and Cynthia F. Kurtz (Kurtz-Fernhout
Software, <https://github.com/pdfernhout/PlantStudio>). That covers the phytomers, the phyllotaxis, apical dominance
and one orientation rule per plant. This is a design reference only: no PlantStudio code is copied.

## Third-party material added by CoralFlower

| What | Licence | Where |
| --- | --- | --- |
| Departure Mono font by Helena Zhang (<https://departuremono.com>): the screens' pixel font (rasterised at build time by `tools/gen_pixfont.py`) and the web pages' font | SIL OFL 1.1 | `assets/fonts/DepartureMono-Regular.otf`, `LICENSES/OFL-DepartureMono.txt` |

## ChaseFlower additions

ChaseFlower's changes and original synth/drum designs are GPL-3.0-only. The HOME Chord and Keyboard views were
contributed by **Michael Norris (@norrismw)** in [CoralFlower issue #8](https://github.com/FicusFicusov/CoralFlower/issues/8),
as a patch to the GPL firmware. The integration retains the existing file licenses and credits the contributor.

The initial chase3 library contains 32 original **Aquanode / aquanodemusic** DX7 voices, published under **CC0-1.0**.
Their packed voice bytes are unchanged; ChaseFlower adds browsing names, genre assignments and output trims.
The original banks, license, pinned source revision, hashes and per-slot audit are in
`assets/sounds/candidates/aquanode/`. The 128-sound release catalog maps each imported sound back to its original
bank and slot in `assets/sounds/chase3-catalog.json`. Two further unchanged voices, DX SAX and DX FULLFLU from
Aquanode Presets 4, extend that catalog to 130 records in current source. No Infinite Cycles remake banks are included.

The optional **M Retro** and **M FM** drum recipes adapt Kerem Kilic's original Melodee CR-78 and FM drum
recipes, published under **GPL-3.0-only** at revision `165d177a425944f3d30b0b669255a6ed0050c3a5`.
The source tables are `firmware/src/drum_cr78.c` and `firmware/src/drum_fm.c` in
[Melodee](https://github.com/keremimo/melodee/tree/165d177a425944f3d30b0b669255a6ed0050c3a5/firmware/src).
Their author credit is retained in `firmware/src/melodee_drum_rows.h`. These are adaptations for the existing
CoralFlower drum renderer, not ports of Melodee's engines. No Sequential factory programs or recorded sample
libraries are included. See `docs/MELODEE-DRUMS.md`.

## RAKT design acknowledgements

Studio's generative design draws on Juan Jiant's seeded parts, genre profiles and bass-to-kick phrasing, as
well as the GLO layer for the live keyboard map. Rastapopolix's work informed per-track behavior and
longer-term phrase development. deamonSLINKY's performance workflow informed keeping the band's controls
available while it plays. [JIANT FM](https://github.com/juanjiant-bit/JIANT) · [Tessera](https://github.com/Rastapopolix/tessera) · [PERF-1](https://github.com/deamonSLINKY/PERF-1)

imijes's panel zoom was a visual reference for the Studio rack. Telepathic Instruments' Orchid informed
the chord controls and strum in the upstream firmware. [Bass Something 2](https://github.com/imijes/bass-something-2) · [Orchid ORC-1](https://telepathicinstruments.com/products/orchid-orc-1)

## JieLi SDK files in the packages (Apache-2.0)

A `.fwsc` package made by `tools/build.py` holds three unmodified files from the JieLi AC79 SDK:
`uboot.boot`, `cfg_tool.bin` and `eq_cfg_hw.bin`. They are vendored in `tools/sdk/` with the SDK's licence. They are
licensed under the Apache License 2.0 (`LICENSES/Apache-2.0.txt`), not under the GPL, and their copyright stays with
JieLi Technology. Every release carries the licence texts next to the package.

## No vendor material

No M-VAVE material is part of CoralFlower. The firmware links no vendor code and holds no vendor data. The packages
hold only CoralFlower and the three SDK files above.

## Trademarks

"M-VAVE" and "FM-1" are trademarks of their respective owners. "Orchid" is a product of Telepathic Instruments.
CoralFlower is independent firmware that runs on FM-1 hardware. It is not affiliated with, endorsed by or supported
by any of them.

## RAKT contributions and upstream contact

RAKT issues and contributions: <https://github.com/chasedeanda/rakt-fm1/issues>.

The following contact information belongs to the upstream CoralFlower project.

Contributions are welcome under GPL-3.0-only. Contact: iambemoli@gmail.com, Telegram channel
<https://t.me/ficusficusov>.
