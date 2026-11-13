#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check listing metadata and exercise the loader's real flash-write gates."""
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
read = lambda p: (root / p).read_text()
meta = json.loads(read('fmhub.json'))
assert set(meta) <= {'what', 'author', 'kinds', 'play', 'editor', 'cheat', 'image', 'color', 'sounds'}
assert 20 <= len(meta['what']) <= 300 and not re.search(r'[<>{}`]|https?://|www\.', meta['what'])
assert 2 <= len(meta['author']) <= 60
assert meta['kinds'] and all(k in ['synth','groove','drums','chords','tracker','other'] for k in meta['kinds'])
for key in ['play','editor','cheat']:
    assert key not in meta or meta[key].startswith('https://')
assert re.fullmatch(r'#[0-9A-Fa-f]{6}', meta['color'])
assert max(int(meta['color'][i:i+2],16) for i in [1,3,5]) >= 128
image = (root / meta['image']).resolve()
assert image.is_relative_to(root) and image.stat().st_size <= 300_000
png = image.read_bytes()
assert png[:8] == b'\x89PNG\r\n\x1a\n'
w,h = struct.unpack('>II',png[16:24])
assert w == h and 240 <= w <= 1024
loader = read('firmware/loader/loader.c')
assert '#define FELUCCA_ID "ota-FM-1_57258001"' in loader
assert 'f\'-DFELUCCA_ID="ota-{PRODUCT}"\'' in read('tools/build.py')
assert 'PRODUCT = "FM-1_57258001"' in read('tools/build.py')
assert '#define FELUCCA_ID "FM-1_57258001"' in read('firmware/src/rakt.c')
assert 'ldr_flash_known = FL_JEDEC_OK(fl_jedec_ram());' in loader
for path in ['firmware/src/cf_shim.c','firmware/src/cr_shim.c','firmware/src/project.c']:
    assert 'flash_ok = FL_JEDEC_OK(FL_FAR(fl_jedec_ram)());' in read(path)
# Compile the actual erase/program hooks, without accessing hardware. Unknown,
# blank and wrong-capacity IDs must call neither underlying write function.
hooks = '\n'.join(re.search(r'static int '+name+r'\([^)]*\)\n\{.*?\n\}',loader,re.S)[0] for name in ['ldr_erase','ldr_prog'])
source = '''#include <assert.h>
#include "fm1_flash_ids.h"
static int erases, programs, ldr_flash_known;
static int fl_erase4k_ram(uint32_t off,uint32_t *t) { (void)off;(void)t;erases++;return 0; }
static int fl_prog_ram(uint32_t off,const void *p,uint32_t n,uint32_t *t) { (void)off;(void)p;(void)n;(void)t;programs++;return 0; }
'''+hooks+'''
int main(void) {
 const uint32_t ids[]={0x856014u,0xB36014u,0u,0xFFFFFFu,0x856015u,0xB36015u,0xEF4014u};
 for(unsigned i=0;i<sizeof(ids)/sizeof(ids[0]);i++) {
  erases=programs=0;ldr_flash_known=FL_JEDEC_OK(ids[i]);
  int e=ldr_erase(0x5000u),p=ldr_prog(0x5000u,0,0);
  assert(ldr_flash_known==(i<2));
  assert(erases==(i<2) && programs==(i<2));
  assert((e==0)==(i<2) && (p==0)==(i<2));
 }
 uint32_t once=0x856014u;assert(FL_JEDEC_OK(once++));assert(once==0x856015u);
}
'''
with tempfile.TemporaryDirectory(prefix='rakt-flash-') as directory:
    p=Path(directory);(p/'test.c').write_text(source)
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-I'+str(root/'firmware/hal'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print(f'PASS: Hub metadata, {w}x{h} PNG ({len(png)} B), app/loader identities, both flash chips and unknown-chip write refusal')
