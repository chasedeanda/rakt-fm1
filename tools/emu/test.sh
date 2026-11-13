#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/../.."
out=build/emu/test
mkdir -p "$out"
rm -f "$out/stopped.ppm" "$out/playing.ppm"
cat > "$out/studio.txt" <<'SCRIPT'
shot stopped
key C4 down
wait 350
dump
key C4 up
wait 300
dump
btn PLAY
wait 400
shot playing
dump
btn PLAY
wait 300
dump
SCRIPT

unset CF_FLASH CF_LOADMODEL CF_RENDER_MS
CF_BOOT_MS=1600 sh tools/emu/cf_headless.sh "$out/studio.txt" "$out" > "$out/studio.log"
test -s "$out/stopped.ppm"
test -s "$out/playing.ppm"
if cmp -s "$out/stopped.ppm" "$out/playing.ppm"; then
    echo 'Studio did not redraw after PLAY' >&2
    exit 1
fi
if ! awk '
    /^fm1_in:/ { step++ }
    /chord: .* sounding 1 / && step == 1 { key_down = 1 }
    /chord: .* sounding 0 / && step == 2 { key_up = 1 }
    /part 0 gated: [0-9]/ && step == 3 { band_playing = 1 }
    /part 0 gated:$/ && step == 4 { band_stopped = 1 }
    /midi out:.*10 [1-9][0-9]*\)/ && step == 3 { drum_midi = 1 }
    END { exit !(step == 4 && key_down && key_up && band_playing && band_stopped && drum_midi) }
' "$out/studio.log"; then
    echo "Studio input or transport check failed. See $out/studio.log" >&2
    exit 1
fi
echo 'PASS: RAKT headless boot, live chord release, Studio start/stop, and drum MIDI.'
