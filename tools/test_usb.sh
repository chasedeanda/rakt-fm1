#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Host tests only: real firmware DSP/USB code, mocked USB register access. No device is opened or flashed.
set -eu
cd "$(dirname "$0")/.."
case "${1:-}" in
    ""|--full) ;;
    *) echo "Usage: $0 [--full]" >&2; exit 2 ;;
esac
mkdir -p build/host build/usb-tests
cc -std=gnu11 -O2 -w -Itools/emu/web/compat -Ibuild/gen -Ifirmware/src \
   -o build/host/cf_usb_test tests/cf_usb_test.c -lm
CF_USB_DIAG=build/usb-tests/diagnostics.syx build/host/cf_usb_test > build/usb-tests/protocol.log
tail -7 build/usb-tests/protocol.log
compare_capture() {
    part=$1 clean=$2 fx=$3
    name="$rate-$part-$clean-$fx"
    for capture in 0 1; do
        build/host/cf_usb_test --render "$part" "$clean" "$fx" "$capture" \
            "build/usb-tests/$name-$capture.raw" "$rate" > "build/usb-tests/$name-$capture.log" || {
            cat "build/usb-tests/$name-$capture.log"
            return 1
        }
    done
    cmp "build/usb-tests/$name-0.raw" "build/usb-tests/$name-1.raw"
    tail -2 "build/usb-tests/$name-1.log"
}

# Isolate each source for stem routing, then exercise the full Studio mix in both output modes.
for rate in 44100 48000; do
    for source in 0 1 2 3; do
        compare_capture "$source" 1 0
    done
    compare_capture 8 0 1
    compare_capture 8 1 1

    if [ "${1:-}" = "--full" ]; then
        for source in 0 1 2 3 4 5 6 7; do
            for mode in 0 1; do
                for effects in 0 1; do
                    if [ "$source" -ge 4 ] || [ "$mode" -ne 1 ] || [ "$effects" -ne 0 ]; then
                        compare_capture "$source" "$mode" "$effects"
                    fi
                done
            done
        done
    fi
done
python3 tests/test_usb_tools.py
echo 'PASS: USB protocol, resampling and bit-identical physical output with capture active or idle at both rates.'
