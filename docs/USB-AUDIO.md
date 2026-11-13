# ChaseFlower 0.6-chase2 — USB recording candidate

> Current CORD builds identify as **CORD FM-1** for USB MIDI and **CORD In** for USB audio. The older ChaseFlower names below describe the historical chase2 release. Re-select the CORD device in your DAW after updating.

This build enables six-channel UAC1 recording alongside USB MIDI. The boot-only `0.6-chase1` was successfully
installed and booted by Chase. **The new audio build has passed host tests and the actual JieLi firmware build;
physical audio enumeration, sound quality, CPU timing and GarageBand recording are still unverified.**

## What changed and why

CoralFlower disabled `FELUCCA_UAC`, never enabled the inherited mixer's capture staging, and never populated its
master pair. Turning the flag on alone would advertise audio without recording the actual master mix.

`firmware/src/audio.c` now activates the stem taps only while the computer reads audio. After the complete
CoralFlower mix, effects, MASTER, metronome and output processing, it reduces the exact signed 24-bit words sent
to I2S to PCM16. At 44.1 kHz, tests compare each USB master sample against the physical output word shifted down
eight bits and saturated to 16 bits. The physical output words themselves are unchanged by capture in all 56
deterministic render comparisons.

`usb_audio_stream.c` resets the 48 kHz FIR history after a stream gap. A generation counter prevents a block rendered
across a USB stream reset from being fed into the new stream. The existing endpoint service, ring buffer, packet
format, clock-drift handling and six-channel resampler are reused.

`cf_backup.c` exposes read-only USB/CPU diagnostics on SysEx command 79; the first fourteen values retain the
inherited format. `tools/uac_check.py` understands the additional counters and selects the master pair explicitly,
avoiding an implicit downmix of the master plus stems. `fm1_install.py` decodes the extended diagnostics.

The boot stub, update loader, linker/flash layout, package identity `FM-1_930`, saved-settings format, presets and
backup data are unchanged. The INFO prefix remains `CoralFlower` for updater compatibility; the version is
`CoralFlower 0.6-chase2` and the startup title remains ChaseFlower.

## Audio channels and operating modes

| USB inputs | Signal |
|---|---|
| 1 / 2 | Master L / R: chord, bass, layer, drums, sequencer, effects, sound-change tails, and metronome when audible |
| 3 / 4 | Chord L / R: the chord part after its distortion, level and pan, before the shared effects and master volume |
| 5 / 6 | Bass L / R: the corresponding bass part |

The master follows the physical MASTER knob and normalized digital output level. The stems retain the inherited
half-level capture and Clean-output alignment delay; they are dry parts, not separate copies of the shared effects.
Recording only stems omits the layer and drums. Turn off the instrument's metronome if it should not be recorded.

Both 44.1 and 48 kHz, 16-bit PCM, are advertised. The instrument's native synthesis clock stays unchanged. At
48 kHz the inherited six-channel 160/147 FIR resampler runs only during capture. **Use 44.1 kHz with Spark 2.**
A read-only check on Chase's Mac on November 6, 2026 reported Spark 2 USB Audio at 44.1 kHz, with two inputs and
two outputs. This does not constitute a test of ChaseFlower audio.

Recording is on by default with no new menu or persistent setting. This deliberately avoids migrating the packed
188-byte settings record. The CDC serial console and audio both use EP2 IN, so the inherited driver presents
audio + MIDI in normal mode and console + MIDI in SAFE MODE, never both EP2 functions together. USB MIDI uses
EP1 IN/OUT and remains available for notes, backups and updates. Computer-to-FM-1 audio playback is not added.

## Build and artifacts

On the M2 Mac, use the same Python environment and downloaded toolchain as the first build. Docker Desktop must
be running; the repository runs the JieLi compiler inside Linux x86-64 containers.

```sh
PYTHON="$PWD/../work/venv/bin/python" \
JIELI_TOOLCHAIN="$PWD/../work/jieli/toolchain" \
./build.sh --release 0.6-chase2
```

The build generates `build/release-0.6-chase2/coralflower-0.6-chase2.fwsc`. Its identical, renamed installable copy is
checked into `releases/0.6-chase2/chaseflower-0.6-chase2.fwsc`. The SHA-256 is:

```text
ecc96068fd5c702fa40816b0b31085ee12b2521270b1e68a427623f943afcdb7
```

| Measurement | Result |
|---|---:|
| Firmware package | 610,313 bytes |
| Application image, including padded stub | 369,940 bytes |
| XIP body | 366,132 / 573,440 bytes |
| RAM data + BSS | 94,652 / 98,304 bytes |
| POOL | 329,756 / 344,064 bytes |
| NOINIT | 216 / 15,664 bytes |
| RAM increase over chase1 | 2,384 bytes |
| POOL increase over chase1 | 6,928 bytes |

The build passes its memory/headroom checks, including more than 8 KiB of spare POOL. Stack regions retain their
existing separate layout. Runtime timing and stack use under demanding patches still require the instrument.

## Verification performed

| Test | Result |
|---|---|
| Real JieLi build, image bounds, MMIO and boot-stub lock checks | PASS |
| Actual final `.fwsc` through the existing browser installer's file validator | PASS |
| Stub, decoded protected sector `0x4000..0x4FFF` and update loader vs chase1 | PASS: byte-identical |
| Complete firmware ARM syntax checks with UAC on and off | PASS |
| Returned descriptors, interface/endpoint allocation, CDC exclusivity, rate requests, reset, simultaneous MIDI queues/endpoints | PASS with mocked registers |
| PCM16 sign, endian order, channels, packet sizes, prime, overflow/underflow recovery and rate mismatch | PASS |
| Six 60-second producer-clock simulations: 44.1/48 kHz at 0, +400 and -1000 ppm | PASS: no frame loss/reordering or ring underruns/overruns |
| FIR ratio over 44,100 input frames, ragged blocks, DC gain, impulse channel isolation and history reset | PASS |
| Combined protocol/stream/resampler suite | PASS: 1,131,194 checks, 0 failures |
| Real firmware renders with capture on vs off | PASS: 56 byte-identical physical-output comparisons |
| Master output matching, live chord/bass/layer/drums, metronome, rhythm machine, four-track sequencer, dry/wet effects, Classic/Clean output, both rates | PASS in host renders |
| Stems on correct parts; silent parts cleared; pause, suspend, closed stream, console and SAFE MODE capture gates | PASS |
| C diagnostic wire reply decoded by Python tools; legacy reply compatibility | PASS: 4 Python tests |
| AddressSanitizer protocol suite and 48 kHz four-track/effects render | PASS: no memory errors reported |
| Existing backup / chord / theory / storage / VA tests | PASS: 132 / 94 / 99 / 14,352 / 228,000 checks |
| Existing Node installer suite | PASS: all checks passed |
| Physical enumeration, recording quality, timing/CPU headroom, reconnect/sleep, GarageBand | NOT RUN: manual hardware testing next |

Host USB tests simulate register/DMA access; they do not simulate a real host controller, USB cable, hubs or the
FM-1 interrupt schedule. The drift runs are simulated time, not six minutes of physical recording. At 48 kHz the
host tests establish resampler correctness and output preservation, not instrument performance under load.

Reproduce the new tests after generating headers:

```sh
../work/venv/bin/python tools/build.py --gen-only
sh tools/test_usb.sh --full
```

The existing `.github/workflows/check.yml` now includes these host tests. The earlier
[verification report](CHASEFLOWER-VERIFICATION.md) lists the existing individual test commands. Modern Node can
run `node --experimental-default-type=module web/test_installer.mjs`.

## Install and perform the first audio check

1. Keep the known-working `chaseflower-0.6-chase1.fwsc` and your November 6 JSON backup. Save another backup if
   sounds or patterns changed since then.
2. Open the upstream installer's [custom-build section](https://ficusficusov.github.io/CoralFlower/#own) in Chrome
   or Edge. Under **Another version, or a build of your own**, click **Choose a .fwsc file** and manually choose
   **`chaseflower-0.6-chase2.fwsc`**. Confirm that exact filename is selected before pressing Install. The site's
   default selection is upstream CoralFlower, and reloading the page restores that default.
3. Complete the update manually. Wait for Done and the restart with the cable connected. Confirm the ChaseFlower
   splash and `0.6-chase2` version. Nothing is flashed automatically by the agent.
4. Open Audio MIDI Setup and choose **Window > Show Audio Devices**. Look for **ChaseFlower In** (or an audio
   device identified as ChaseFlower FM-1), with six inputs. Set Format to **44,100 Hz, 6-channel, 16-bit**.
5. Confirm ChaseFlower MIDI still appears in MIDI Studio. Start a stereo audio recording on inputs 1/2. Play a
   chord, bass, layer, rhythm and sequencer with effects, then play it back. Test the stems separately.
6. Only after the standalone recording works, add Spark 2 through the Aggregate Device below. Later, test 48 kHz
   by itself and repeat a long recording with busy UI and demanding patches. Check for clicks, missing sections,
   repeated stream starts, MIDI glitches and new diagnostic overruns/underruns or late audio halves.

If audio does not appear, confirm chase2 rather than chase1 is installed, reconnect the USB data cable directly,
and confirm the instrument did not start in SAFE MODE. SAFE MODE intentionally exposes MIDI/console only.

## GarageBand with Spark 2

Keep both devices connected. In Audio MIDI Setup, create an Aggregate Device with **+**, name it
**ChaseFlower + Spark**, and include ChaseFlower first and Spark 2 second. Set 44.1 kHz, choose Spark 2 as the
initial clock source, and enable Drift Correction for ChaseFlower. The devices have independent clocks; drift
correction keeps them aligned. Device order determines the input numbers. This follows
[Apple's Aggregate Device guidance](https://support.apple.com/en-us/102171).

With ChaseFlower first and the two-input Spark observed on Chase's Mac, expect:

| Aggregate inputs | Track source |
|---|---|
| 1 / 2 | ChaseFlower stereo master |
| 3 / 4 | Optional dry chord stereo stem |
| 5 / 6 | Optional dry bass stereo stem |
| 7 / 8 | Spark 2 stereo guitar recording |

Confirm these numbers in Audio MIDI Setup if device order changes. In GarageBand **Settings > Audio/MIDI**, set
Output Device to **Spark 2 USB Audio**, then Input Device to **ChaseFlower + Spark**. Create two audio tracks.
In Smart Controls, open Recording Settings and assign stereo inputs **1+2** and **7+8**. Show **Record Enable**
through Configure Track Header, arm both tracks, and press Record. Apple documents these
[GarageBand multi-track controls](https://support.apple.com/en-us/102171).

For the first USB test, unplug the FM-1-to-Spark AUX cable and monitor the ChaseFlower track through GarageBand
to Spark 2. If the AUX connection is used for direct monitoring instead, turn off software monitoring on the
ChaseFlower track to avoid hearing both paths. When hearing the guitar directly from Spark, leave software
monitoring off on its track too; record-enable still records it. Monitor through one path per instrument.
The FM-1 MASTER knob affects its recorded level, so lowering it to silence a physical output also lowers USB capture.

These are setup instructions for the candidate; a simultaneous GarageBand session has not been tested by the agent.

## Optional hardware diagnostics

With ffmpeg (avfoundation support) and `python-rtmidi` installed, the existing checker can capture the master pair
and report stream counters through MIDI. Set the audio device's Format first; this tool does not change it:

```sh
python3 tools/uac_check.py capture-44100.wav 30 --device 'ChaseFlower In' --rate 44100 --debug
python3 tools/uac_check.py capture-48000.wav 30 --device 'ChaseFlower In' --rate 48000 --debug
```

By default the tool sends audition notes. Add `--nonotes` to play the instrument manually. It does not install
firmware or change settings. `audio_cpu_q8 * 100 / 256` gives the firmware's averaged render-load percentage;
watch deltas of `audio_late`, `audio_stale`, stream underruns, overruns and missed USB frames. A successful host test
cannot substitute for these measurements. Keep a problematic WAV and before/after counters for investigation.

## Recovery and remaining work

Use the same custom-file installation flow with the preserved **chase1** package to return to the boot-tested
version. The unchanged update gesture is **OCT- + OCT+ + HOME held five seconds**. The existing version-3 stub's
rescue gesture is **OCT- alone at power-on**. See the [first-boot recovery notes](CHASEFLOWER.md#recovery) for the
staged-loader/ROM-boot distinction and upstream recovery tool. No bootloader or persistent-data change is required.

There is no recording On/Off or rate menu yet; recording is always available in normal mode and the host chooses
the rate. A separate USB level control and extra rhythm patterns, including the requested 6/8 rhythm, remain later
work. Physical validation is the remaining gate before calling this a proven recording build.
