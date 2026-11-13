#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Run the default RAKT host suite after tools/build.py --gen-only.
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/host

echo 'RAKT rack, generator and live controls'
sh tools/test_studio.sh
sh tools/emu/test.sh

echo 'Recordings, startup, voice budget and USB audio'
sh tools/test_recordings.sh
sh tools/test_rakt_boot.sh
sh tools/test_boot_sound.sh
sh tools/test_chord_drum_budget.sh
sh tools/test_usb.sh
sh tools/test_diagnostics.sh

echo 'Shared harmony, Chord Lab, progression drafts and FX preset format'
cc -std=c99 -Wall -Wextra -Ifirmware/src -o build/host/cf_chord_test \
    tests/cf_chord_test.c firmware/src/cf_chord.c firmware/src/cf_harmony.c firmware/src/cf_theory.c
build/host/cf_chord_test
cc -std=c99 -Wall -Wextra -Ifirmware/src -o build/host/cf_theory_test \
    tests/cf_theory_test.c firmware/src/cf_theory.c
build/host/cf_theory_test
cc -std=c99 -O2 -Wall -Wextra -Werror -Ifirmware/src -o build/host/cf_harmony_test \
    tests/cf_harmony_test.c firmware/src/cf_harmony.c firmware/src/cf_chord.c firmware/src/cf_theory.c
build/host/cf_harmony_test
cc -std=c11 -O2 -Wall -Wextra -Werror -Ifirmware/src -o build/host/cf_lab_test \
    tests/cf_lab_test.c firmware/src/cf_lab.c firmware/src/cf_progression.c \
    firmware/src/cf_chord.c firmware/src/cf_harmony.c firmware/src/cf_theory.c
build/host/cf_lab_test
sh tools/test_song.sh
cc -std=gnu11 -O2 -w -DST_DRAFT -Ifirmware/src -o build/host/st_draft_cut_test tests/st_log_test.c
build/host/st_draft_cut_test
cc -std=c11 -O2 -Wall -Wextra -Werror -Ifirmware/src -o build/host/cf_fx_preset_test \
    tests/cf_fx_preset_test.c firmware/src/cf_fx_preset.c
build/host/cf_fx_preset_test

echo 'Device backup and web installer'
cc -std=gnu11 -O1 -w -Itools/emu/web/compat -Ibuild/gen -Ifirmware/src -Itests \
    -o build/host/cf_backup_test tests/cf_backup_test.c -lm
build/host/cf_backup_test
node web/test_installer.mjs
