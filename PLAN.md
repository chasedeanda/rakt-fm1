# FM-1 update plan

## Goals

1. Replace the flower and coral visuals with a bonfire and a swimming bass (fish), selectable independently of the playing mode.
2. Keep the instrument in Top Lock; repurpose the Algorithm control's mode-changing action for visual selection.
3. Plan recording improvements after defining the desired recording workflow.

## Starting point

- Confirmed project: ChaseFlower, Chase's customized CoralFlower firmware for the M-VAVE FM-1.
- User repository: https://github.com/chasedeanda/rakt-fm1 (main).
- Source is available locally on `codex/bonfire-bass-visuals`. Approved scenes and boot are implemented; see [the feature and validation guide](docs/VISUALS.md).
- Upstream source: https://github.com/FicusFicusov/CoralFlower
- Repository inspected on 2026-11-06. Current rendering selects flower versus coral from the chord algorithm. Algorithm cycles Top Lock, Top, Side Lock, Side.
- The fork's chase3 release also has HOME cycling through Flower, Chord, and Keyboard, with the view saved across restarts.
- Custom visuals are planned as firmware rendering changes; a user-uploadable image feature has not been established.

## Update 1: custom visuals

Requested subjects: a bonfire and a swimming bass.

Confirmed control requirements:

- Always use Top Lock, including startup and after loading older settings or restoring backups.
- Select Bonfire or Bass without changing the keyboard layout or chord-lock behavior.
- Replace Algorithm's chord-mode cycling with scene selection. The interaction below is implemented.

Approved interaction:

- On HOME, turn Algorithm to switch between Bonfire and Bass, and show the selected scene immediately.
- Show a brief `Bonfire` or `Bass` label when selecting it.
- Keep HOME's existing cycle as Scene → Chord → Keyboard; Scene displays the selected Bonfire or Bass.
- Remember the selected scene across power-off and backup/restore; use Bass when no scene has been saved yet.
- Preserve Algorithm's existing contextual jobs in FX (effect ordering) and Sequencer (pattern/song selection).
- Where Algorithm otherwise changes the chord mode, remove that mode-changing behavior. Top Lock remains fixed.
- Switching scenes must preserve the held/latched chord, active notes, sounds, effects, tempo, and sequencer playback.

Approved animation:

- Bonfire: flickering flames above logs, with occasional rising embers. Played notes briefly increase flame height and add sparks. The approved revision has one taller, triangular flame with curling golden ribbons.
- Bass: a recognizable fish silhouette with a moving tail and fins, gentle body-wave swimming, and bubbles. Four distant baitfish drift slowly behind it; notes accent the motion.
- Both scenes animate quietly at idle, with stronger responses while playing.
- Approved new boot: a bass swims through a night lake, jumps with small splashes, then fully exits while **Chase FM-1** appears. The intro is 2.4 seconds and can be skipped.

Implementation sequence (completed locally; merge request targets main):

1. Obtain the intended firmware source and establish a working baseline build.
2. Separate scene selection from `cs.chord.algo`; enforce Top Lock through defaults, settings load/restore, and any configuration path that can change the algorithm.
3. Add a persistent scene setting without overwriting the existing HOME view and sequencer hint bits or invalidating saved sounds/patterns.
4. Repurpose `ui_algo()` mode selection in `firmware/src/cf_ui.c`, update its popup, and replace the algorithm-dependent species selection in `cf_feed_plant()`.
5. Integrate the scenes with `cf_draw.c`, `cf_views.c`, and the existing animation/music inputs. Preserve Chord and Keyboard views.
6. Preview both designs at the actual screen resolution, then implement their animation and responses to music.
7. Verify scene switching during held chords and sequencer playback; Top Lock after restart/restore; scene persistence; HOME views; FX/Sequencer encoder actions; readable musical information; smooth animation; and uninterrupted audio/USB recording.
8. Build a local verification package and submit a merge request. The user will make the combined release build after all feature merges; no flashing or release publication here.

## Update 2: recording

Scope is not yet defined. The fork already documents sequencer recording and USB audio work; inspect that baseline when defining the next recording improvement. First determine whether the goal is recording notes into patterns, looping/overdubbing performances, capturing audio, or exporting recordings to a computer.

Then define the desired start/stop controls, count-in, recording length, playback behavior, and save/export workflow. Check the existing firmware and hardware capabilities before committing to an implementation.

## Next decisions

- Confirm the version currently installed on the device before implementation/build testing.
- Merge the visuals feature alongside the independently developed features, then make the combined build.
- Describe the desired recording change.
