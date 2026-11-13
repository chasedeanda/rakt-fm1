# ChaseFlower diagnostics

The scene-free development build retains these controls and exports. Record the
build profile (`CF_SCENES=0` or `1`) when comparing workloads. Only the optional
exploration build draws an animated scene beneath the dashboard.

HOME cycles Flower/Scene → Chord → Keyboard → Diagnostics. From the first view, three taps reach Diagnostics;
the next tap returns to the first view. Returning HOME from another screen keeps the selected view.
The existing 188-byte settings and backup formats are unchanged.

Open **GLO → Diagnostics** with SELECT to configure the dashboard:

| Control | Action |
| --- | --- |
| K1 | Overview, Timing, Memory or USB |
| K2 | Turn right to hold the displayed snapshot; left to resume |
| K3 | Turn either way to reset this diagnostic session and resume |
| K4 | Display sampling at 2, 4 or 8 Hz (default 4) |

These controls are session-local. Musical HOME controls keep their existing behavior.
Collection continues on the other views and while the display is held. Reset affects only diagnostic peaks and
counter baselines; it does not stop notes, change patches, clear lifetime counters or repaint stacks.
The underlying HOME animation still renders beneath the overlay, preserving that workload.

## Reading the numbers

- **Audio** is the existing smoothed audio-render utilization, excluding nested TIMER5 work. It is not whole-chip CPU usage.
- **Work peak** is the largest observed render time in this session. The nominal 128-frame budget at 44.1 kHz is 2,902 µs.
- **Wall** includes nested work; **lag** estimates how late a half started. **Lag+wall** is the maximum sum observed
  within a single half, rather than a sum of unrelated maxima. The small diagnostics accounting tail is outside this timer interval.
- **Late** counts halves where DMA advanced during rendering. **Stale~** estimates stale DAC samples; it is not a count of audible clicks.
- **Shed**, voice steals and hard kills reveal load-management behavior that a low average can hide. Active voices include outgoing tails.
- **UI Hz/gap** report UI service cadence in the last completed approximately one-second window, not display scanout rate.
- **RAM**, pool and XIP come from this device binary's linker symbols. These are fixed allocations and remaining region
  capacity, not live heap usage. Display values may be abbreviated; JSON contains exact bytes.
- **Core1** reports its actual state. This feature does not enable it. Its existing stack watermark is since boot and
  available only when running. CPU0 stacks are explicitly uninstrumented in this version.
- USB under/overruns and missed frames are session deltas. Resampler peak is the existing USB lifetime peak since boot,
  not cleared by diagnostic reset. USB state distinguishes closed, capture and hold/suspend paths.

The emulator marks itself **DIAG EMU**, leaves device RAM unavailable and does not present host timing as device CPU.
Colors reflect existing 70/85% engine pressure conventions; they are advisory, not calibrated pass/fail limits.

## Repeatable feature comparisons

The workload matrix is in [performance.json](../benchmarks/performance.json). First establish a baseline on the physical FM-1.
For every comparison use the same saved patch and exact edits, notes/velocities, releases, effects, tempo/meter,
USB recording rate, duration, scene, visible HOME view and polling frequency. Reset the diagnostic session immediately
before each run. Reports contain useful context, but do not capture every patch edit or automate musical playback.

Connect USB MIDI and use the same Python MIDI dependencies as `tools/fm1_install.py` (`python-rtmidi` on macOS/Linux).
The following tool only reads information; it does not play notes, reset sessions, flash or install firmware:

```sh
python3 tools/diagnostics.py capture before.json --label va-six-note-pad-long-release --seconds 60 --revision BASE_COMMIT
# Install the later build yourself, restore the identical workload, then reset its diagnostic session.
python3 tools/diagnostics.py capture after.json --label va-six-note-pad-long-release --seconds 60 --revision FEATURE_COMMIT
python3 tools/diagnostics.py compare before.json after.json
```

`--port` selects a MIDI port when needed. Capture polls roughly once per second and records version, an explicit
source revision label, UTC timestamp, workload label and raw samples. The revision label is supplied by the operator;
it is not a verified firmware Git identity. Hold affects only the screen; exports always read a fresh snapshot.
Capture after the audio engine starts and keep the full workload within the reset session. Run lengths should match.

Compare reports show memory cost, timing peaks and error deltas. Emulator reports and sessions reset during capture
are rejected. Different labels, final patch/FX/USB/tempo context or session lengths trigger comparison notes.
Inspect the raw reports too: edited parameter values and intermediate patch changes are not fully represented.
Stage profile values are valid only when `profile_enabled` is 1; they are the last completed window, not session peaks.
Do not interpret zero disabled stage readings as free processing.

Host CI checks measurement fixtures, protocol decoding, and the hold and reset controls reached through
**GLO → SAVE → Diagnostics** in RAKT. `sh tools/test_diagnostics.sh` runs these checks locally after generating headers.
Host checks establish correctness; hardware execution establishes timing. Numeric hardware regression thresholds remain
unset until repeatable physical baselines exist. The firmware link must also continue to fit each region.

## Read-only command 80

Request: `F0 7D 46 4C 50 F7` (0x50 is decimal 80). Reply uses the existing framing, followed by schema version 1,
field count 63, and five little-endian 7-bit bytes per unsigned 32-bit field. It totals 323 bytes including framing.
The ordered names in `tools/diagnostics.py:FIELDS` match the 51 `D_*` values in `cf_diag.c`, followed by six stage averages
and six stage maxima (tick, part0, part1, FX, master, wait), all times in µs unless the field says milliseconds.
Part0/part1 are the existing renderer stages, not independent whole-instrument per-part accounting.
The decoder accepts appended future fields but requires the current prefix and version. Reads leave peaks and session
baselines intact. Existing command 79 is unchanged. No flash writes or heap allocation are introduced.

## Initial validation (2026-11-06)

Local JieLi device build against main `83fe5a1` (same chase3 firmware):

| Region | Main bytes | Diagnostics bytes | Change | Remaining |
| --- | ---: | ---: | ---: | ---: |
| Main RAM (98,304 capacity) | 94,940 | 95,308 | +368 | 2,996 |
| Pool (344,064 capacity) | 329,756 | 329,756 | 0 | 14,308 |
| XIP (573,440 capacity) | 392,180 | 397,524 | +5,344 | 175,916 |

Session/snapshot state accounts for 268 bytes; the linked RAM delta also includes control tables and strings.
The device build passes its existing region, RAM-code and register-access checks. It is validation only; no new
installer release is published. Recheck combined memory after merging other features.
Host render comparisons cover all 56 USB DSP fixtures against main with bit-identical output, plus a complete
headless audio run that enters the fourth HOME view while playing. Actual device CPU overhead, musical stress
limits and CPU0 stack watermarks have not been measured by these host checks.
