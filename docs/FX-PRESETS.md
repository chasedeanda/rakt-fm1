# Effects: macros, individual editing and factory presets

Integration base: current main `70ed4e4` (2026-11-07). The approved thirteen-recipe
bank replaces the forty similar factory configurations. The current Instrument
Teal renderer, real Motion On/Off setting, smooth recall and sixteen saved user
slots are retained. Hardware verification remains pending.

## Controls

In the Studio rack, **FX** opens the highlighted musician’s preset page. **K1**
or **PRESETS** browses the library and **SEL** loads the preset for that musician.
Use **ALGORITHM** to switch musicians, **EDIT** to edit the loaded chain, and
**SAVE** to manage library slots or clear or undo that musician’s FX. Studio
saves the four assignments in backup object 76. Musicians without a saved
assignment start with the previous master FX settings. Studio uses separate
quarter-rate delay and reverb buffers for each musician.

Press FX to open Macros. SELECT browses the complete factory bank, followed by
sixteen user slots; it leaves the current sound alone. SEL explicitly loads the
highlighted recipe. The loaded name and browse target are distinct on screen.

| View | Control | Result |
| --- | --- | --- |
| Macros | K1 Space | Reverb decay and mix |
| Macros | K2 Echo | Delay feedback and mix; time stays with the recipe or Individual edit |
| Macros | K3 Motion | Chorus/phaser depth and mix |
| Macros | K4 Warmth | Filter cutoff, drive/tone and compensating drive level |
| Macros / Individual | ALGORITHM right / left | Individual / Macros; no audible change and no order editing |
| Individual | SELECT | Choose one of the six existing effects |
| Individual | K1–K4 | Numbered enable/type/parameter controls shown on screen |
| Both | SAVE | Existing user-slot management; hold SAVE one second to commit the visible action |
| Management | K4 Action | New / Replace / Delete / No FX / Previous |
| Management | FX | Return to Macros |
| Both | FX or HOME | Return home |

Each macro curve passes through its audible anchor. Individual edits re-anchor
from the actual chain so the next macro detent does not restore stale factory
values. Factory recipes use Filter → Drive → Chorus → Phaser → Delay → Reverb.
Legacy saved custom orders are retained until a factory recall; there is no
reorder control. The resulting chain persists in the unchanged 188-byte settings.
Exact factory/user matches recover their name; other edited restores show Custom.

## Approved factory bank

| Preset | Character |
| --- | --- |
| Clean | Unprocessed reference |
| Velvet | Dark, rounded and close |
| Bite | Audible grit and forward harmonics |
| Slap | A short, separate double behind each attack |
| Ping Pong | Clearly separated repeats across the stereo field |
| Wide Drift | Slow chorus movement with no ambient wash |
| Phase Sweep | Pronounced moving notches and a hollow sweep |
| Radio | Narrow midrange focus and a boxy edge |
| Silver Plate | Bright dense reflections; crisp attacks remain audible |
| Deep Space | Long diffuse hall with soft echoes and chorus |
| Orbit | Liquid sweeping ambience: deep phaser and slow chorus into a bright long plate, without echoes |
| Phone LP | Exaggerated muffled handset: resonant low-pass near 400 Hz, stronger drive and the darkest speaker tone; dry and close |
| Phone HP | Exaggerated thin raspy handset: resonant high-pass near 1.6 kHz, maximum drive and a brighter speaker tone; dry and close |

Orbit is retained. Broken Circuit is excluded. Phone LP and Phone HP use the
exaggerated versions approved in the listening study. No shimmer pitch shifter,
stutter buffer, bitcrusher, freeze or granular effect is implied by these recipes.

## One combined visual readout

Macros has one large shared response drawing. Reverb tail bars, echo marks and
chorus/phaser waves occupy that same field; there are no four mini-plots. Filter
and drive recipes show a large response curve. Individual shows one large drawing
for the chosen effect. The four numbered knob assignments remain underneath.

Motion uses main's existing post-MASTER peak, mute, freshness and smoothed energy
metadata. Tail bars breathe with that energy, echo accents advance at the selected
delay time, and modulation waves travel at the chosen rate while sound is active.
They settle after silence. Motion Off clears phase/activity and produces a stable
readout while values and parameter curves still respond. This is a response diagram,
not an oscilloscope. Hidden pages do not advance its phase. No DSP/flash work is
performed in the renderer.

Native screenshots and an Orbit playback GIF are under `previews/fx-screen`.
Run `sh tools/test_fx.sh`, then `python3 tools/render_fx_screen.py` with Pillow.

## Persistence and compatibility

The active 188-byte `cf_set_t`, manual forms, Harmony record, sound stores and sequencer layout retain their formats.
The new storage object is appended after `OBJ_CFHARM`, preserving every existing object's numeric type.
The FX bank uses the CoralFlower build's unused QUAD A/B pair at **0xB2000 / 0xB3000**. Harmony remains at
0xB4000 / 0xB5000; no existing CoralFlower object shares either FX sector. This allocation applies to this fork,
which disables CZ and QUAD; it is not an allocation for a QUAD-enabled firmware.

The v1 little-endian `FXP1` bank is 752 bytes: a 12-byte header, sixteen 46-byte slots, and a 4-byte FNV-1a checksum.
The header stores magic, version, exact record size, a 16-bit occupied mask and zero reserved bytes. Each slot holds
a canonical zero-padded 21-byte printable name and the existing 25-byte `cf_fx_t`. Empty slots are zeroed.
Validation checks all ranges, a permutation of the six unique effects, supported enabled bits, terminated nonblank names,
empty-slot contents, version, size, reserved bytes and checksum. Validation occurs before mutation and restore.
A valid storage frame carrying an incompatible/corrupt FX payload becomes protected; it is not silently replaced.
Use a compatible backup or an explicit factory reset to recover such a bank. Old firmware with no bank starts with empty user slots.

The bank fits a 1024-byte logged entry (992 bytes payload); existing objects keep their original entry sizes.
It uses the existing verified A/B commit and delta log, with no flash operation in audio processing. Edits rest for
500 ms. Busy audio uses `ST_NOERASE`, with the existing 85% audio-load gate; a full log leaves the edit pending.
Erases/tidying require at least one second of silence including effect tails, and 1.5 seconds since the bank edit.
SAFE MODE and the restore lock prevent writes. Backup LIST tries to commit pending FX changes without erase and
returns a flash failure instead of exporting a stale bank if that cannot complete; let the unit become quiet and retry.

Backup object **69** carries the bank; 68 remains Harmony. Browser and Python backup tools know the new ID and name.
PUT validates the complete payload before writing; a successful restore locks automatic saves until restart.
A zero-length bank restore clears the bank. Older backups simply lack object 69. The existing reset includes both sectors.

## Audio transitions and memory

Preset recall fades the processed output to the dry input for about 10 ms, changes configuration at a block boundary
while fully dry, and returns over about 10 ms. Rapid recalls replace the pending target. One chain runs throughout;
there is no second reverb/delay allocation. Delay/reverb buffers remain alive. Legacy custom orders, time changes and reverb
size changes can still reshape tails; the fade briefly reduces their audibility. Bypass fades through the same blend
and keeps processing the chain, preserving its running state and CPU cost.
Individual parameter editing retains the existing filter/mix smoothing and behavior.

The principal added runtime storage is a 752-byte bank, a 256-byte dry scratch pair, a 25-byte pending chain and a
small UI/previous-chain state. The 13 factory records occupy 598 bytes of constants. Macro anchors and presentation
state use bounded foreground storage, without additional DSP buffers. These are source-layout measurements;
final linked device memory and real CPU headroom must be checked before release.

## Verification

Run `sh tools/test_fx_presets.sh` after `python3 tools/build.py --gen-only` with the repository's pinned dependencies.
CI now runs the same FX suite. It covers live SELECT audition, recall from bypass, previous-chain recovery,
clear-to-dry sample identity, clear persistence and repeated-clear undo, plus empty slots and browser limits without
loading synth patches, validation/corruption, all factory configurations, UI holds and cancellation,
new/replace/delete, full-bank capacity, reload, SAFE MODE/restore lock, incompatible payload protection,
nonoverlapping sectors, deferral without erase, transitions and held-note/sequencer playback with USB capture.

Observed locally:

- Format validation: 849 checks; catalog names, effective-chain uniqueness, workflow/storage and transition checks pass.
- Extended NOR log test: 1,500 steps, 2,189 power cuts (215 interrupted again during reboot), 18,432 checks, no failures.
- The 752-byte bank allowed 177 small fixture changes without erase; actual capacity varies with edit size.
- Constant-input transition fixture: maximum sample step 8,862 for an immediate distortion-level change, 23 with the recall fade.
  This measures the fade on a controlled signal, not perceived quality across all presets.
- Physical output is byte-identical with USB capture on/off at 44.1 and 48 kHz during preset/bypass changes.
  At 44.1 kHz the captured master also matches the physical output frame by frame.
- Each of the 13 chains is browsed with SELECT and recalled with SEL and exercised with bypass
  transitions during held-note/sequencer playback at both USB rates.
- Twenty-six independent level auditions: each factory on a six-note VA POLY KEYS or FM PAD chord plus bass at full MASTER,
  held for two seconds and released for one. No PCM samples clipped. Sustained RMS was -20.92..-14.41 dBFS on VA and
  -24.61..-14.34 dBFS on FM. The colored filters intentionally change energy; this is not perceptual loudness normalization.
  The fixtures reject unexpectedly quiet/loud output outside -27..-10 dBFS and saturation at the PCM boundary.

The focused macro/readout suite, native page/cache and Motion On/Off audio suites,
current Keyboard Learn checks, and the local linked firmware build pass. Physical
FM-1 timing, listening and USB checks remain pending.

Rendered screen previews and test logs are in `build/fx-tests/`. A local linked firmware package is built; hardware flashing and verification remain pending.
The suite also writes 26 three-second stereo WAVs and `levels.csv` to `build/fx-auditions/`. To rerender only these
auditions after building the test executable, run `python3 tools/audition_fx_presets.py`.

Before release on FM1:

- Save a named configuration, change it, recall it, wait for pending storage to clear, power-cycle and recall it again.
- Save/replace/delete during held or latched chords, ARP, sequencer and recording; ensure no note/transport changes or audio holes.
- Record all 13 factories and repeated transitions over six-channel USB at both rates; listen for clicks, level jumps and tail changes.
- Capture diagnostics before/after for CPU peak/history, voice shedding, audio deadlines, flash holes, USB drops and stack/memory margins.
- Audition each factory at varied input levels and dense FM/VA chords; adjust levels before approving the factory bank.
- Test backup/restore including older files without object 69, factory reset and SAFE MODE on the device.

Host emulation cannot close the physical acceptance items in #13–#15 or the release validation gate #31.
