#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Package the existing FM-1 WASM player without the inherited installer/release list."""
from pathlib import Path
import hashlib, json, re, shutil, subprocess
root=Path(__file__).resolve().parents[1]
out=root/'build/play-site'; out.mkdir(parents=True,exist_ok=True)
play=out/'play';play.mkdir(exist_ok=True)
for name in ('index.html','emu.js','keymap.js','worklet.js','rakt.wasm'):
    shutil.copyfile(root/'build/emu-web'/name,play/name)
shutil.copyfile(root/'web/site.css',out/'site.css')
guides=out/'guides'; guides.mkdir(exist_ok=True)
for name in ('rakt-quick-start.png', 'rakt-recording.png', 'rakt-glo-live.png'):
    shutil.copyfile(root/'docs/images'/name,guides/name)
for name in ('RAKT-FM1-Quick-Start.pdf', 'RAKT-FM1-GLO-Live.pdf'):
    shutil.copyfile(root/'docs'/name,guides/name)
shutil.copytree(root/'web/fonts',out/'fonts',dirs_exist_ok=True)
for name in ('favicon.svg','favicon-32.png','apple-touch-icon.png'):
    shutil.copyfile(root/'tools/emu/web'/name,out/name)
for name in ('LICENSE','LICENSING.md'):
    shutil.copyfile(root/name,out/name)
shutil.copytree(root/'LICENSES',out/'LICENSES',dirs_exist_ok=True)
sha=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
source=f'https://github.com/chasedeanda/rakt-fm1/tree/{sha}'
text=(play/'index.html').read_text()
head=text[:text.index('<body>')]
head=head.replace('<title>Play RAKT in your browser</title>', '<title>RAKT · Generate a band. Play it live.</title>')
head=re.sub(r'(<meta (?:name="description"|property="og:description") content=")[^"]*(">)', r'\1Generate a band on the M-VAVE FM-1. Four parts stay in sync and play off each other. Perform with the band in GLO Live.\2', head)
head=head.replace('content="Play RAKT in your browser"', 'content="RAKT · Generate a band. Play it live."')
main=text[text.index('<main id="main">'):text.index('  <section class="section scope-accent final"')]
main=main.replace('<h1 class="display-2" id="play-title">Play it in your browser</h1>', '<p class="rakt-faceplate__label">RAKT / Browser rack</p><h1 class="display-2" id="play-title">Generate a band. Play it live.</h1>')
main=main.replace('<p class="lead">This is the real RAKT firmware, running in your browser with sound. Tap or click the keys, or use your computer keyboard.</p>', '<p class="lead">Pick a genre and bring in a band. Chords, bass, drums and melody share the groove and play off each other. Press Start to turn on audio, then use the on-screen panel or your computer keyboard to jam.</p><a href="#quick-start">First jam guide ↓</a>')
main=main.replace('Keys, buttons and knobs', 'Play from your computer')
assert '  <section class="section" aria-labelledby="keys-title">' in main, 'Player controls section missing'
main=main.replace('  <section class="section" aria-labelledby="keys-title">', (root/'web/rakt-play-guide.html').read_text() + '\n  <section class="section" aria-labelledby="keys-title">')

header='''<body class="rakt-player rakt-rack"><a class="skip" href="#main">Skip to content</a><header class="site-header"><div class="container site-header__row"><a class="brand" href="#play-title">RAKT · FM-1</a><nav class="site-nav" aria-label="Play page"><a href="#quick-start">First jam</a><a href="#keys-title">Computer controls</a><a href="../install/">Install on your FM-1</a></nav></div></header>'''
footer=f'''</main><footer class="site-footer"><div class="container"><p>Browser build · <a href="{source}">{sha[:7]}</a></p><p>Built on CoralFlower, ChoralRoot FM-1, Felucca and Melodee. <a href="../LICENSE">GPL-3.0</a> · <a href="../LICENSING.md">Credits and licences</a>.</p></div></footer><script src="keymap.js"></script><script src="emu.js"></script></body></html>'''
# Keep the build script's cache-busting query strings.
for filename in ('keymap.js','emu.js'):
    match=re.search(r'src="('+re.escape(filename)+r'\?[^\"]+)"',text)
    if match:footer=footer.replace('src="'+filename+'"','src="'+match.group(1)+'"')
# Refresh styles when this guide changes, including through browser/CDN caches.
css_hash=hashlib.sha256((out/'site.css').read_bytes()).hexdigest()[:10]
head=head.replace('href="../site.css"', f'href="../site.css?v={css_hash}"')
(play/'index.html').write_text(head+header+main+footer)
(out/'index.html').write_text('<!doctype html><meta charset="utf-8"><meta http-equiv="refresh" content="0;url=play/"><title>Play RAKT</title><a href="play/">Play RAKT</a>')
(out/'.nojekyll').touch()
(out/'build.json').write_text(json.dumps({'revision':sha,'wasm_sha256':hashlib.sha256((play/'rakt.wasm').read_bytes()).hexdigest()},indent=2)+'\n')
print(f'Browser site: {out} · source {sha[:7]}')
subprocess.run(['python3', str(root/'tools/make_rakt_installer.py')], check=True)
