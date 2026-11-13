# RAKT · FM-1

**Generate a band. Play it live.**

Pick a genre and press EDIT to generate a four-part band on the M-VAVE FM-1. Chords, bass, drums, and a melodic layer share the beat and harmony. The bass can lock to the kick, and the melody can answer gaps in the chords. Shape the groove in Studio, then jump into GLO Live to mix the band, cue fills and drops, and play effects from the keys.

[Play in your browser](https://chasedeanda.github.io/rakt-fm1/play/) · [Install on your FM-1](#install-on-your-fm-1) · [Download the quick-start PDF](docs/RAKT-FM1-Quick-Start.pdf) · [GLO Live cheat sheet](#glo-live-cheat-sheet) · [Studio controls](docs/RAKT-STUDIO-CONTROLS.md)

## RAKT on screen

<p align="center">
  <a href="docs/images/rakt-rack-ui.png"><img src="docs/images/rakt-rack-ui.png" width="760" alt="Twelve RAKT FM-1 screens showing Song, musician panels, Mix, Saved Takes, Help, Generator, ARP, FX Space and Sound Library"></a>
</p>

<p align="center">
  <a href="docs/RAKT-FM1-Quick-Start.pdf"><img src="docs/images/rakt-quick-start.png" width="760" alt="RAKT FM-1 quick start with first jam steps, rack navigation, sound and arp shortcuts, and the GLO keyboard map"></a>
</p>

The recording cheat sheet below shows the current REC workflow.

<p align="center">
  <a href="docs/RECORDINGS.md"><img src="docs/images/rakt-recording.png" width="760" alt="RAKT FM-1 recording cheat sheet for capturing all four parts, replaying and clearing saved takes, and recording the full mix over USB"></a>
</p>

## GLO Live cheat sheet

Press **GLO** while the band plays to mix the four parts and cue fills or drum drops. The seven hold effects sit together from **C5** to **G5**, including **D♯5** for Hype and **F♯5** for Phaser. [Download the one-page PDF](docs/RAKT-FM1-GLO-Live.pdf) · [Full GLO controls](docs/RAKT-STUDIO-CONTROLS.md#glo-live)

<p align="center">
  <a href="docs/RAKT-FM1-GLO-Live.pdf"><img src="docs/images/rakt-glo-live.png" width="760" alt="GLO Live cheat sheet with the FM-1 keyboard map, mixer, fills and drops, and seven hold effects"></a>
</p>

## How the band plays together

Some styles react to another musician's phrase. In **New Wave**, the Bass **Moving pulse** style accents notes that land with the kick and can pull a nearby note forward to meet it. Switch Drums from **Machine** to **Open hats** to hear the bass accents move. In **Indie**, try Chords **Chop** with Layer **Counterline**. The Layer looks for space between chord hits and shortens its reply when the next chord is close. [Musician styles](docs/RAKT-STUDIO-CONTROLS.md#musician-panel)

Indie also has an eight-bar form. The Layer and hats sit out in bar 5, return in bar 6, and the drums play a turnaround near the end of bar 8. The form runs even with **Evolve** set to zero.

## Shape an arrangement

- **Shuffle the song.** Press **EDIT** on the Studio overview for a new progression, parts, key, scale, meter, and tempo drawn from the selected genre. While playing, the new take starts at the next bar. Zoom into a musician to regenerate just that part. [Studio EDIT](docs/RAKT-STUDIO-CONTROLS.md#song-pages-and-generator)
- **Shape what the band plays.** In **Studio → Generator**, **Tempo** sets the pace and **Density** adjusts how busy the free parts are. At 50%, Density uses each part's own Activity setting. **Evolve** varies one eligible part over time. Set it to 0% to stop automatic changes, or turn on **Hold** in a musician's Evolve page to keep that phrase while the rest of the band changes. [Generator controls](docs/RAKT-STUDIO-CONTROLS.md#song-pages-and-generator)
- **Change the groove.** Open **Generator** with **SEL** and turn **SELECT** to **Groove**. Adjust **Swing** or choose the **Drums**, **Bass**, and **Chords** playing styles. [Groove controls](docs/RAKT-STUDIO-CONTROLS.md#song-pages-and-generator)
- **Change the harmony in time.** **Studio → Song → Harmony** queues key and scale changes for the next chord. Meter changes land at the next bar. Studio supports 3/4, 4/4 and 6/8. [Harmony controls](docs/RAKT-STUDIO-CONTROLS.md#song-pages-and-generator)
- **Give the band your own chords.** Open **Studio → Song → Progression**, then press **EDIT**. Use **ALGORITHM** to choose a slot and the chord page to set its degree, quality, color and length. On the **Phrase** page, choose **Insert** or **Duplicate** with K1 and press **SEL** to add a slot. Press **PLAY** in the editor to put the draft under all four musicians. The draft keeps its chords, durations and meter when you regenerate a part. [Progression editor](docs/RAKT-STUDIO-CONTROLS.md#custom-progression)

## Give each part its own sound

**Sound → Sound Library** lets you browse within the current genre or open **All sounds**. In Genre mode, **SEL Apply** loads the featured Chords, Bass, Layer and drum kit together. **EDIT** on the Sound overview shuffles tones, kits, and genre-matched FX. Zoom into a musician’s Tone panel to shuffle just that musician’s sound and FX. [Sound library](docs/RAKT-STUDIO-CONTROLS.md#sound) · [Studio Originals listening pack](docs/RAKT-STUDIO-ORIGINALS.md)

Open a part's **Envelope** page to adjust how a pitched sound swells or fades. On **Sends**, set Drive, Chorus, Delay and Reverb for that part. Open **Sound → FX Space** to scale chorus, delay and reverb across the band. [Sound and effects controls](docs/RAKT-STUDIO-CONTROLS.md#sound)

## Capture a performance

Press **REC** to capture what Chords, Bass, Layer and Drums are playing, including live keys, arpeggios and drum fills. Recording starts with the band from a stop or on the next bar while it plays. Press **REC** again to keep the take, or hold it to discard. The **Record** rack keeps the three latest takes, up to eight bars each. Select a take and press **PLAY** to hear it. Use USB audio for a stereo recording of the full mix and effects. [Record controls](docs/RAKT-STUDIO-CONTROLS.md#record-and-help) · [Recording and backup guide](docs/RECORDINGS.md)

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

Run `python3 tools/build.py --gen-only` and then `sh tools/test_rakt.sh` to run the same host regression suite as CI. The suite covers the Studio, Sound, Record and Help racks, GLO Live, startup, USB audio, recording and backup compatibility, and shared harmony and draft storage. Individual scripts for inherited firmware modes are available for targeted work.

For a local browser build, run `sh tools/emu/web/build_web.sh` and `python3 tools/make_play_site.py`, then serve `build/play-site` over HTTP. The [emulator guide](tools/emu/README.md) covers the toolchain and test commands.

## More guides

[Harmony](docs/AUTO-HARMONY.md) · [Chord Lab](docs/CHORD-LAB.md) · [Progression catalogue](docs/PROGRESSION-EXPLORER.md) · [Drum grooves](docs/DRUM-LOOPS.md) · [FX presets](docs/FX-PRESETS.md) · [Saved Songs](docs/SONGS-LIBRARY.md) · [Boot animation and chime](docs/RAKT-BOOT.md) · [Diagnostics](docs/DIAGNOSTICS.md) · [Report an issue](https://github.com/chasedeanda/rakt-fm1/issues)

## Credits

The Studio generator and live controls drew on these projects.

- Juan Jiant's generator informed the use of separate seeded parts, bass phrases that respond to the kick pattern, and genre-specific playing styles. His GLO layer informed the live keyboard map. [JIANT FM](https://github.com/juanjiant-bit/JIANT)
- Rastapopolix's work informed independent track behavior and the way phrases evolve over longer spans. [Tessera](https://github.com/Rastapopolix/tessera)
- deamonSLINKY's performance workflow informed how Studio keeps the controls available while the band plays. [PERF-1](https://github.com/deamonSLINKY/PERF-1)

imijes's panel zoom was a visual reference for Studio. The chord controls and strum also draw on Telepathic Instruments' Orchid through RAKT's firmware lineage. [Bass Something 2](https://github.com/imijes/bass-something-2) · [Orchid ORC-1](https://telepathicinstruments.com/products/orchid-orc-1)

RAKT is free software under **GPL-3.0-only**. The firmware inherits work from CORD, CoralFlower, ChoralRoot FM-1 and Felucca, with FM synthesis and drum recipe contributions from Melodee. [Source and asset acknowledgements](LICENSING.md) · [GPL license](LICENSE)
