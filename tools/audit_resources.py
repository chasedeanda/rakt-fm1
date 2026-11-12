#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Audit the real pi32 device ELF; no emulator-memory or CPU-speed inference.

JIELI_TOOLCHAIN=... build/venv/bin/python tools/audit_resources.py --build \
    --output build/resource-audit/main

--build runs the normal builder, then captures compiler dependencies/macros using
its exact app_flags(). Without --build, --inputs DIR reuses a prior capture; the
ELF must match that capture's SHA256. --compare BASELINE/report.json emits deltas.
Only stdlib is required to inspect an existing, captured ELF.
"""
import argparse
import hashlib
import json
import os
import re
import sys
import importlib.metadata
import shlex
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True).strip()


def linker_regions(path):
    regions = {}
    for name, origin, length, suffix in re.findall(
            r'([A-Z]+)\s+\([rwx]+\)\s*:\s*ORIGIN\s*=\s*(0x[0-9a-fA-F]+|[0-9]+),\s*'
            r'LENGTH\s*=\s*(0x[0-9a-fA-F]+|[0-9]+)([Kk]?)', path.read_text()):
        regions[name] = {'origin': int(origin, 0), 'capacity': int(length, 0) * (1024 if suffix else 1)}
    if not {'RAM', 'POOL', 'XIP', 'RAMTEXT', 'NOINIT'} <= regions.keys():
        raise ValueError('Expected all five device linker MEMORY regions')
    return regions


def read_elf(path):
    data = path.read_bytes()
    if len(data) < 52 or data[:6] != b'\x7fELF\x01\x01':
        raise ValueError('Expected an ELF32 little-endian device image')
    h = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
    if h[11] != 40 or not h[12] or h[6] + h[11] * h[12] > len(data) or h[13] >= h[12]:
        raise ValueError('Invalid ELF section table')
    headers = [struct.unpack_from('<10I', data, h[6] + i * 40) for i in range(h[12])]

    def blob(section):
        if section[4] + section[5] > len(data):
            raise ValueError('Truncated ELF section')
        return data[section[4]:section[4] + section[5]]

    def string(table, offset):
        end = table.find(b'\0', offset)
        if offset >= len(table) or end < 0:
            raise ValueError('Invalid ELF string offset')
        return table[offset:end].decode('utf-8')

    names = blob(headers[h[13]])
    sections = [{'name': string(names, x[0]), 'address': x[3], 'bytes': x[5], 'flags': x[2]} for x in headers]
    symbols = []
    for header in headers:
        if header[1] != 2:  # SHT_SYMTAB
            continue
        if header[6] >= len(headers) or header[9] != 16 or header[5] % 16:
            raise ValueError('Invalid ELF symbol table')
        strings = blob(headers[header[6]])
        table = blob(header)
        for offset in range(0, len(table), 16):
            name, value, size, info, _, section = struct.unpack_from('<IIIBBH', table, offset)
            if name and size and section < len(sections) and info & 15 in (1, 2):
                symbols.append({'name': string(strings, name), 'bytes': size, 'address': value,
                                'section': sections[section]['name'], 'kind': 'object' if info & 15 == 1 else 'function'})
    if not symbols:
        raise ValueError('Unstripped ELF with sized symbols is required for allocation evidence')
    return sections, sorted(symbols, key=lambda x: (x['section'], -x['bytes'], x['name']))


def capture(output):
    # Import the existing builder; it owns flags, toolchain selection and Docker.
    import build
    flags = build.app_flags()
    unit = ROOT / 'firmware/src/rakt.c'
    dependencies = build.tc('cc', *flags, '-MM', unit)
    macros = build.tc('cc', *flags, '-dM', '-E', unit)
    (output / 'dependencies.d').write_text(dependencies)
    (output / 'macros.txt').write_text(macros)
    return flags


def make_report(elf, inputs):
    identity = json.loads((inputs / 'identity.json').read_text())
    if sha(elf) != identity['elf_sha256']:
        raise ValueError('ELF does not match the recorded compile-input capture')
    sections, symbols = read_elf(elf)
    sizes = {s['name']: s['bytes'] for s in sections}
    memory = identity['memory_regions']
    if not {'.text', '.data', '.bss', '.pool', '.ram_text', '.c1_text', '.noinit'} <= sizes.keys():
        raise ValueError('Device linker sections are missing')
    regions = {
        'ordinary_ram': {'bytes': sizes['.data'] + sizes['.bss'], 'capacity': memory['RAM']['capacity']},
        'pool': {'bytes': sizes['.pool'], 'capacity': memory['POOL']['capacity']},
        'xip': {'bytes': sum(sizes.get(s, 0) for s in ('.text', '.ram_text', '.c1_text', '.data')), 'capacity': memory['XIP']['capacity']},
        'ram_code': {'bytes': sizes['.ram_text'] + sizes['.c1_text'], 'capacity': memory['RAMTEXT']['capacity']},
        'noinit': {'bytes': sizes['.noinit'], 'capacity': memory['NOINIT']['capacity']},
    }
    for region in regions.values():
        region['headroom'] = region['capacity'] - region['bytes']
    deps = shlex.split((inputs / 'dependencies.d').read_text().replace('\\\n', ' ').split(':', 1)[1])
    deps = sorted(set(Path(x).as_posix() for x in deps))
    hashes = {x: sha(ROOT / x) for x in deps if (ROOT / x).is_file()}
    if hashes != identity['dependency_sha256']:
        raise ValueError('Dependency contents changed since capture; rebuild/capture before reporting')
    macros = {}
    for line in (inputs / 'macros.txt').read_text().splitlines():
        parts = line.split(maxsplit=2)
        if len(parts) == 3 and (parts[1].startswith(('FELUCCA_', 'CR_', 'CF_')) or
                               parts[1] in ('ENG_STATE_NPART', 'VA_NPART', 'MIX_NPART', 'FM6_POLY', 'NTRK', 'NSND', 'NVOICE', 'DRUM_NPART')):
            macros[parts[1]] = parts[2]
    groups = {
        'scene_allocations_gross_not_net_savings': ('fl_px', 'scene_body_px', 'flo'),
        'pattern_store_and_undo_not_reclaimable_without_replacement': ('sq_st', 'sqh'),
        'recording_cancel_and_flash_staging_pool': ('sq_cancel', 'sq_io'),
        'recording_order_and_preferences': ('sq_meta', 'sq_serial', 'sq_order', 'sq_pref'),
        'display_framebuffer_and_dma_shared_with_recovery': ('cv_px', 'fb_dma'),
        'mascot_art_const_not_runtime_image_buffers': ('CFM_ART', 'CFM_DEVIL_RGB', 'CFM_DEVIL_ROWS', 'CFM_DEVIL_DATA',
                                                      'CFM_CAT_RGB', 'CFM_CAT_ROWS', 'CFM_CAT_DATA'),
        'native_font_const_bitmaps_and_glyph_tables': tuple(
            f'CFN_{size}{suffix}' for size in (11, 12, 14, 16, 32, 64) for suffix in ('', '_G', '_BITS')),
        'engine_part_and_voice_state': ('trk', 'eng_state', 'va_vs', 'va_patch', 'va_mlast', 'va_rate_off', 'va_lfo', 'fm6_patch', 'fm6_eff'),
        'fx_buffers_preserve_audible_tails': ('xd_buf', 'cv_buf', 'cho_buf'),
        'legacy_loop_conversion_staging': ('sq_loop_buf',),
    }
    allocations = {}
    for group, wanted in groups.items():
        members = [x for x in symbols if x['kind'] == 'object' and x['name'] in wanted]
        allocations[group] = {'symbols': members, 'bytes_by_section': {
            section: sum(x['bytes'] for x in members if x['section'] == section)
            for section in sorted(set(x['section'] for x in members))}}
    return {'schema': 1, 'identity': identity, 'regions': regions,
            'sections': [x for x in sections if x['flags'] & 2], 'macros': macros,
            'compilation_dependencies': deps, 'linked_symbols': symbols, 'allocation_groups': allocations,
            'limits': ['Static allocations are not net savings of a replacement build.',
                       'Symbol sizes do not attribute inlined/shared code to a feature.',
                       'Included source is not proof that its functions survived linking.',
                       'Runtime, core activation and audio margins still require device diagnostics.']}


def report_json(report):
    # Keep the full symbol inventory reviewable: one symbol per line instead of
    # seven lines per record. Other metadata remains conventionally indented.
    metadata = {k: v for k, v in report.items() if k != 'linked_symbols'}
    prefix = json.dumps(metadata, indent=2, sort_keys=True)[:-2]
    symbols = ',\n'.join('    ' + json.dumps(x, sort_keys=True) for x in report['linked_symbols'])
    return prefix + ',\n  "linked_symbols": [\n' + symbols + '\n  ]\n}\n'


def render(report):
    lines = ['# Device resource audit', '', f"Source: `{report['identity']['revision']}`.",
             f"ELF SHA256: `{report['identity']['elf_sha256']}`.", '',
             '| Region | Linked bytes | Capacity | Headroom |', '| --- | ---: | ---: | ---: |']
    for name, r in report['regions'].items():
        lines.append(f"| {name} | {r['bytes']:,} | {r['capacity']:,} | {r['headroom']:,} |")
    lines += ['', 'Allocation groups are gross linked objects, not promised savings.', '']
    for group, value in report['allocation_groups'].items():
        lines += [f'## {group}', '', '| Symbol | Section | Bytes |', '| --- | --- | ---: |']
        lines += [f"| `{x['name']}` | `{x['section']}` | {x['bytes']:,} |" for x in value['symbols']]
        lines += ['']
    if 'comparison' in report:
        lines += ['## Paired change (current minus baseline)', '', '| Region | Delta bytes |', '| --- | ---: |']
        lines += [f'| {name} | {delta:+,} |' for name, delta in report['comparison']['delta_bytes'].items()]
    lines += ['', *['- ' + x for x in report['limits']], '']
    return '\n'.join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--build', action='store_true', help='Fresh normal firmware build and target dependency/macro capture')
    ap.add_argument('--release', help='Pass a version to the normal builder and capture the same versioned compiler flags')
    ap.add_argument('--elf', type=Path, default=ROOT / 'build/rakt.elf')
    ap.add_argument('--inputs', type=Path, help='Existing capture directory (required without --build)')
    ap.add_argument('--output', type=Path, default=ROOT / 'build/resource-audit/main')
    ap.add_argument('--compare', type=Path, help='Prior report.json; no speed claim is inferred from deltas')
    args = ap.parse_args()
    if not args.build and not args.inputs:
        ap.error('Use --build for an identified build, or --inputs for a verified prior capture')
    if args.release and not args.build:
        ap.error('--release requires --build')
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    if args.build and args.elf.resolve() != ROOT / 'build/rakt.elf':
        ap.error('--build produces build/rakt.elf; use --inputs to inspect another captured ELF')
    if args.build:
        import build
        revision = git('rev-parse', 'HEAD')
        # Track the exact firmware/build recipe, including local changes, across the build.
        paths = git('ls-files', '--cached', '--others', '--exclude-standard', '--', 'firmware', 'tools', 'assets').splitlines()
        before = {x: sha(ROOT / x) for x in paths}
        log = output / 'build.log'
        with log.open('w') as f:
            command=[str(ROOT / 'build.sh')]
            if args.release:command+=['--release',args.release]
            subprocess.run(command, cwd=ROOT,
                           env={**os.environ, 'PYTHON': os.sys.executable}, stdout=f, stderr=subprocess.STDOUT, check=True)
        if before != {x: sha(ROOT / x) for x in paths}:
            raise ValueError('Firmware/build source changed during build; capture discarded')
        if args.release:build.VERSION=build.release_brand(args.release)+' '+args.release.lower()
        flags = capture(output)
        deps = shlex.split((output / 'dependencies.d').read_text().replace('\\\n', ' ').split(':', 1)[1])
        identity = {'revision': revision, 'source_status': git('status', '--porcelain'), 'compiler_flags': flags,
                    'size_pass': os.environ.get('FELUCCA_SIZE') != '0', 'elf_sha256': sha(args.elf),
                    'build_log_sha256': sha(log), 'build_recipe_sha256': sha(ROOT / 'tools/build.py'),
                    'audit_tool_sha256': sha(Path(__file__)),
                    'compiler_version': build.tc('pi32v2/bin/clang', '--version').strip(),
                    'python_version': sys.version.split()[0],
                    'python_packages': {name: importlib.metadata.version(name) for name in ('Pillow', 'fonttools')},
                    'memory_regions': linker_regions(ROOT / 'firmware/app.ld'),
                    'startup_sha256': {x: sha(ROOT / x) for x in ('firmware/crt0.S', 'firmware/hal/fm1_vec.S',
                                                               'firmware/hal/fm1_isr.S', 'firmware/hal/fm1_cpu1.S')},
                    'linker_script_sha256': sha(ROOT / 'firmware/app.ld'),
                    'dependency_sha256': {x: sha(ROOT / x) for x in sorted(set(deps)) if (ROOT / x).is_file()}}
        (output / 'identity.json').write_text(json.dumps(identity, indent=2, sort_keys=True) + '\n')
        inputs = output
    else:
        inputs = args.inputs.resolve()
    report = make_report(args.elf, inputs)
    if args.compare:
        baseline = json.loads(args.compare.read_text())
        report['comparison'] = {'baseline_identity': baseline['identity'],
                                'delta_bytes': {name: r['bytes'] - baseline['regions'][name]['bytes']
                                                for name, r in report['regions'].items()}}
    (output / 'report.json').write_text(report_json(report))
    (output / 'report.md').write_text(render(report))
    print(render(report).split('Allocation groups')[0].strip())
    print(f'Reports: {output}')


if __name__ == '__main__':
    main()
