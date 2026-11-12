# Current Cord boot sound (0.6-chase13)

The warm signature now strums D3, F3, A3, C4, F4 and A4 over 620 ms and decays to silence
by 2.2 seconds. Its measured full-level output peak is -11.28 dBFS, about 6.6 dB louder than
the earlier four-note signature. It uses the existing MASTER and saved BOOT SND controls.

[Listen to the current firmware render](audio/rakt-boot-chime.wav).

Startup waits 180 ms after UI initialization before requesting the sound. Early note input
cancels that request. An active chime fades for note/overload priority in at most 256 samples.
SAFE stays quiet, Motion is independent, and the chime never emits MIDI or recording events.
The selected notes' timing, pitch and velocity setup happen outside the sample loop.

`tools/test_boot_sound.sh` checks finite duration, headroom, chunk-independent rendering,
clean muted output/stems and first-key physical/USB/MIDI isolation at both sample rates.
The old four-note first-pass WAV checksum is deliberately retired for this requested new sound.

Physical loudness and timing of the current revision remain device checks. Earlier release
behavior is recorded in docs/releases/0.6-chase10.md.
