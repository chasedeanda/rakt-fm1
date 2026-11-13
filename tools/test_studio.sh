#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/host
sh tests/hostcc.sh cc -std=gnu11 -O2 -w -Itools/emu/web/compat -Ibuild/gen -Ifirmware/src -Itests \
    tests/cf_studio_test.c -o build/host/cf_studio_test -lm
case "${1:-}" in
    "") build/host/cf_studio_test ;;
    --shots)
        mkdir -p build/studio/ui
        build/host/cf_studio_test --shots
        ;;
    --listening)
        build/host/cf_studio_test
        python3 tools/render_studio.py
        ;;
    *) echo "Usage: $0 [--shots|--listening]" >&2; exit 2 ;;
esac
