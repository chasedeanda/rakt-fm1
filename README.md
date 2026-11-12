# RAKT · FM-1

**A rack-style chord instrument and groovebox for the M-VAVE FM-1.**

Play chords over a generated band of Chords, Bass, Drums and Layer. Choose a genre, give each musician its own sound and playing style, and write progressions for the whole band.

<p align="center">
  <img src="docs/images/rakt-boot/boot.gif" width="360" alt="RAKT power-on rack with a mixer lamp sweep and a READY output">
</p>

[Play in your browser](https://chasedeanda.github.io/rakt-fm1/play/) · [Install on your FM-1](#install-on-your-fm-1) · [Studio controls](docs/RAKT-STUDIO-CONTROLS.md) · [Build the firmware](#build-the-firmware) · [Credits](#credits)

## Make your first loop

1. **Start the band.** Bring up **MASTER** and press **PLAY** from the Studio overview. Play the lower keys for chord roots and the upper keys for modifiers and **HOLD**.
2. **Pick a genre.** Turn **PRESETS** on the Studio overview. During playback, the new take starts at the next bar.
3. **Change how the band plays.** Turn **ALGORITHM** to highlight a musician. Press **SEL** to open it and use **K1 Style** and **K2 Activity**. Press **EDIT** on the Studio overview to generate a new take or in a musician panel to regenerate that part.
4. **Shape the sound.** Press **HOME** to back out, then turn **SELECT** to **Sound**. Open **Mix** for the four channel levels, or open a musician to edit its tone and sends.

## Get around the rack

| Control | In Studio |
| --- | --- |
| **SELECT** | Switch between Studio, Sound, Record and Help from an overview. In a detail panel with multiple pages, turn SELECT to change pages. |
| **ALGORITHM** | Move between rack units. |
| **SEL** | Open the highlighted unit or perform the action shown on screen. |
| **K1–K4** | Adjust the controls shown along the bottom of the display. |
| **HOME** | Back out to the overview. From an overview, return to Studio. |
| **SEQ** | Jump to Harmony, Shaping, Genre, ARP and Progression. Press again to step through them. |
| **GLO** | Open the live controls for mutes, momentary solos, fills and tempo. Press **SAVE** there to open Settings. |

- **Write a progression.** Open **Studio → Progression** and press **EDIT**. Turn **ALGORITHM** to choose a chord slot and **SELECT** to turn editor pages. Press **PLAY** from the editor to use the draft with the band. [Progression controls](docs/RAKT-STUDIO-CONTROLS.md#custom-progression)
- **Shape the arrangement.** Use **Energy** for activity across the band and **Evolve** for changes over time. Use **Hold** on a musician's Evolve page to keep its phrase. [Harmony and Shaping](docs/RAKT-STUDIO-CONTROLS.md#harmony-and-shaping)
- **Arpeggiate keys or the band.** Press **ARP** to open the panel for Chords or Layer. Choose **Keys** for held notes or **Band** for generated chords. [Arpeggiator controls](docs/RAKT-STUDIO-CONTROLS.md#arpeggiator)
- **Choose and mix sounds.** Apply a genre palette from **Sound → Sound Library**, or browse presets and kits for each musician. Use **FX** for shared presets and **Sends** for individual parts. [Sound controls](docs/RAKT-STUDIO-CONTROLS.md#sound) · [Studio Originals listening pack](docs/RAKT-STUDIO-ORIGINALS.md)
- **Record a take.** Press **REC** to capture manual playing over drums for up to eight bars. The **Record** rack shows the band's phrases and gives access to the latest three saved takes. Use USB audio to capture the full generated performance. [Record controls](docs/RAKT-STUDIO-CONTROLS.md#record-and-help) · [Recording and backup guide](docs/RECORDINGS.md)

## Play without an FM-1

Open the [browser player](https://chasedeanda.github.io/rakt-fm1/play/) and press **Start** to enable sound. Click or tap the panel, drag or scroll the knobs, or use your computer keyboard.

| Computer control | FM-1 control |
| --- | --- |
| **Q W E R T Y U I O** | Chord roots, with **R = C4** |
| **1–0** and backtick | Chord modifiers and HOLD |
| **Space / Enter** | PLAY / REC |
| **[ / ]** | PRESETS |
| **; / '** | SELECT |
| **, / .** | ALGORITHM |

The page lists the full key map and offers MIDI on supported browsers. Browser saves stay in that browser.

## Install on your FM-1

1. Back up the FM-1's sounds, settings and recordings before switching firmware. Keep a known-working firmware package handy.
2. Open the [RAKT installer](https://chasedeanda.github.io/rakt-fm1/install/) in desktop Chrome or Edge and connect the FM-1 with a USB data cable.
3. The installer selects and checks the latest published RAKT package when one is available. To choose a different build, expand **Another version, or a build of your own** and select a `.fwsc` file from [RAKT releases](https://github.com/chasedeanda/rakt-fm1/releases) or a local build.
4. Verify the selected version, start the install and keep the cable connected and the page open until the FM-1 restarts.

The [Firmware Hub checklist](docs/RAKT-HUB-SUBMISSION.md) documents the package identity, supported flash chips and hardware checks.

## USB audio and MIDI

Select **RAKT In** in your DAW. Inputs **1/2** carry the full stereo mix, **3/4** the Chords stem and **5/6** the Bass stem. Capture supports **44.1 and 48 kHz**. [USB audio setup](docs/USB-AUDIO.md)

Select **RAKT FM-1** for USB MIDI. From Studio, press **GLO → SAVE** for Settings. **MIDI** sets clock and incoming notes, and **Channels** assigns the four parts. To follow an external clock, select **In** and send MIDI Start or Continue.

## Build the firmware

Clone `https://github.com/chasedeanda/rakt-fm1.git` and follow the [build guide](docs/RAKT-BUILD.md) to set up the pinned Python dependencies and JieLi toolchain. Run `./build.sh` from the checkout to produce `build/rakt.fwsc`.

For host checks, run `python3 tools/build.py --gen-only` before the scripts in `tools/test_*.sh`. For a local browser build, run `sh tools/emu/web/build_web.sh` and `python3 tools/make_play_site.py`, then serve `build/play-site` over HTTP. The [emulator guide](tools/emu/README.md) covers the toolchain and test commands.

## More guides

[Harmony](docs/AUTO-HARMONY.md) · [Chord Lab](docs/CHORD-LAB.md) · [Progression catalogue](docs/PROGRESSION-EXPLORER.md) · [Drum grooves](docs/DRUM-LOOPS.md) · [FX presets](docs/FX-PRESETS.md) · [Saved Songs](docs/SONGS-LIBRARY.md) · [Boot animation and chime](docs/RAKT-BOOT.md) · [Diagnostics](docs/DIAGNOSTICS.md) · [Report an issue](https://github.com/chasedeanda/rakt-fm1/issues)

## Credits

The Studio generator and live controls drew on these projects.

- Juan Jiant's generator informed the use of separate seeded parts, bass phrases that respond to the kick pattern, and genre-specific playing styles. His GLO layer informed the live keyboard map. [JIANT FM](https://github.com/juanjiant-bit/JIANT)
- Rastapopolix's work informed independent track behavior and the way phrases evolve over longer spans. [Tessera](https://github.com/Rastapopolix/tessera)
- deamonSLINKY's performance workflow informed how Studio keeps the controls available while the band plays. [PERF-1](https://github.com/deamonSLINKY/PERF-1)

imijes's panel zoom was a visual reference for Studio. The chord controls and strum also draw on Telepathic Instruments' Orchid through RAKT's firmware lineage. [Bass Something 2](https://github.com/imijes/bass-something-2) · [Orchid ORC-1](https://telepathicinstruments.com/products/orchid-orc-1)

RAKT is free software under **GPL-3.0-only**. The firmware inherits work from CORD, CoralFlower, ChoralRoot FM-1 and Felucca, with FM synthesis and drum recipe contributions from Melodee. [Source and asset acknowledgements](LICENSING.md) · [GPL license](LICENSE)
