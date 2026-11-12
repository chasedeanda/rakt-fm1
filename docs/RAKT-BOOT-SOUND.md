# RAKT boot sound

The electric-key signature strums D3, F3, A3, C4, F4 and A4 over 620 ms. A sharp
snare lands 320 ms in, just ahead of C4, and rings into a wide stereo room. The
cue fades to silence by 2.2 seconds and uses the existing MASTER and BOOT SND controls.
The complete cue gets a 50% gain boost before MASTER, with compression on the loudest transients.

[Listen to the current firmware render](audio/rakt-boot-chime.wav).

Startup waits 180 ms after UI initialization before requesting the sound. Early note input
cancels that request. An active chime fades for note/overload priority in at most 256 samples.
SAFE stays quiet, Motion is independent, and the chime never emits MIDI or recording events.
The selected notes' timing, pitch and velocity setup happen outside the sample loop.

`tools/test_boot_sound.sh` checks finite duration, headroom, chunk-independent rendering,
clean muted output/stems and first-key physical/USB/MIDI isolation at both sample rates.

Physical loudness and timing of the current revision remain device checks. Earlier release
behavior is recorded in docs/releases/0.6-chase10.md.
