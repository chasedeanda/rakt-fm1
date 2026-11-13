#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Build the main-loop code for size (build.py): the functions defined in SIZE_FILES get LLVM's
minsize (about -Oz) while everything else (the sound, render and ISR code, the flash and OTA code,
the main loop) keeps -Os. The JieLi clang 4 has no '#pragma clang attribute' and its minsize
attribute is an error on variables, so build.py compiles the unit (choralroot.c) to LLVM IR without the
optimizer, this script adds 'minsize' to those definitions, and the IR is then compiled at -Os
(without the edit, that round trip is byte-identical to compiling the unit directly). A listed file that is
missing is skipped (Felucca's UI files stay listed: they are not in ChoralRoot's unit, so their names match nothing).
  size_fns.py IN.ll OUT.ll"""
import re
import sys
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[1]
SRC = _ROOT / "firmware" / "src" if (_ROOT / "firmware" / "src").is_dir() else _ROOT / "src"   # either layout
# the UI, the stores, the editor and the console: main loop only, never called by sound code
SIZE_FILES = ["ui.c", "favorites.c", "icons.c", "ui_graph.c", "ui_draw.c", "ui_menu.c", "ui_input.c",
              "storage.c", "upreset.c", "project.c", "settings_persist.c",
              "editor.c", "editor_preferences.c", "editor_backup.c", "console.c",
              # ChoralRoot's UI, screens and stores (not cr_engine.c, cr_out.c, cr_loop.c: they tick in the audio ISR)
              "cr_ui.c", "cr_edit.c", "cr_draw.c", "cr_gfx.c", "cr_pages.c", "cr_bank.c", "cr_name.c", "cr_settings.c",
              "cr_anim.c", "cr_shim.c",
              "cf_stage.c", "cf_screens.c", "cf_native_pages.c", "cf_lab_screen.c", "cf_draft_screen.c",
              "cf_rack_units.c", "cf_rack_style.c", "cf_boot_rakt.c", "cf_glow_view.c", "cf_style.c",
              "cf_arp_ui.c", "cf_keyboard_learn.c", "cf_library_screen.c", "cf_seqv.c", "cf_dmv.c",
              "cf_views.c", "cf_diag_view.c", "cf_variation_view.c", "cf_draw.c", "cf_mascot.c",
              "cf_sequi.c", "cf_dmui.c", "cf_library_ui.c", "cf_library_store.c", "cf_settings.c",
              "cf_acc_screen.c", "cf_workspace.c", "cf_workspace_view.c", "cf_studio_ui.c",
              "cf_accomp_ui.c", "cf_home_banks.c", "cf_ui.c", "cf_acc_ui.c",
              "cf_edit.c", "cf_fxui.c", "cf_fx_presets_ui.c", "cf_lab_ui.c",
              "cf_browser_ui.c", "cf_draft_ui.c"]
SIZE_EXCLUDE = {"cf_ui_pump", "cf_ui_input", "cf_ui_frame", "ui_note_key", "ui_note_key_aged",
                "cf_xnote_up", "cf_xq_drain", "cf_xnotes_up_all"}
DEF = re.compile(r"^(?:static|void|int|uint\w*|int\w*|const)\b[^;=(]*?\b([A-Za-z_]\w*)\s*\(", re.M)
IR_DEF = re.compile(r"^(define [^\n]*?@\"?([\w.]+)\"?\([^\n]*\)(?: unnamed_addr| local_unnamed_addr)?)( #\d+[^\n]*\{)$",
                    re.M)


def definitions(text):
    """names of the functions text defines (a signature at column 0 whose ')' is followed by '{')"""
    names = []
    text = re.sub(r"/\*.*?\*/|//[^\n]*", " ", text, flags=re.S)        # comments (none hold code)
    for m in DEF.finditer(text):
        i, depth = m.end() - 1, 0
        while i < len(text):
            depth += {"(": 1, ")": -1}.get(text[i], 0)
            i += 1
            if depth == 0:
                break
        rest = re.sub(r"^(__attribute__\s*\(\(.*?\)\)\s*)+", "", text[i:i + 200].lstrip())
        if rest.startswith("{"):
            names.append(m.group(1))
    return names


def size_names():
    names = set()
    for f in SIZE_FILES:
        if (SRC / f).is_file():
            names.update(definitions((SRC / f).read_text()))
    return names - SIZE_EXCLUDE


def main(src, dst):
    names, hit = size_names(), set()

    def mark(m):
        if m.group(2) not in names:
            return m.group(0)
        hit.add(m.group(2))
        return m.group(1) + " minsize" + m.group(3)
    Path(dst).write_text(IR_DEF.sub(mark, Path(src).read_text()))
    print(f"size: {len(hit)} main-loop functions built for size ({len(names) - len(hit)} not in this build)")
    return 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:3]))
