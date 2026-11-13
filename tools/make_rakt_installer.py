#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Reuse CoralFlower's installer, package validation, backup and recovery UI."""
from pathlib import Path
import hashlib
import re
import shutil

root = Path(__file__).resolve().parents[1]
out = root / 'build/play-site'
source = (root / 'web/index.html').read_text()
head = source[:source.index('<body>')]
head = re.sub(r'href="(?!https?:|/|#)([^"]+)"', r'href="../\1"', head)
head = head.replace('href="../site.css"', f'href="../site.css?v={hashlib.sha256((root / "web/site.css").read_bytes()).hexdigest()[:10]}"')
head = head.replace('content="images/rakt-boot/boot.gif"', 'content="../images/rakt-boot/boot.gif"')
hero = source[source.index('  <section class="hero"'):source.index('  <!-- 2 facts')]
hero = hero.replace('<h1 class="display-1"', '<p class="rakt-faceplate__label">RAKT / FM-1 hardware</p><h1 class="display-1"')
hero = hero.replace('<span class="brand-a">RAKT</span>', 'Install RAKT')
hero = hero.replace('You can unplug it now.</p>', 'You can unplug it now.</p><p><a href="#quick-start">Start your first jam</a></p>')
hero = hero.replace('Not sure yet? Play it in your browser', 'Jam in your browser')
hero = hero.replace('Generate a band. Play it live.', 'Put a four-part band on your M-VAVE FM-1. Install RAKT over USB, then follow the guides below to start playing.')
hero = hero.replace('href="play/"', 'href="../play/"').replace('src="images/', 'src="../images/')
tools = source[source.index('        <details class="fold" id="backup"'):source.index('  <!-- 7 FAQ')]
tools = '<section class="section"><div class="container container--narrow"><h2 class="display-2">Files, backups and recovery</h2><div class="tools">' + tools
# Preserve the updater's complete module, including all validation and backup guards.
module = re.search(r'<script type="module">[\s\S]*?</script>', source)[0]
# Release manifests change independently of the page and can remain cached at a CDN edge.
module = module.replace('"firmware/releases.json"', '"../firmware/releases.json?refresh=" + Date.now()').replace('"firmware/"', '"../firmware/"')
module = module.replace('installV: "Install on your FM-1"', 'installV: "Install {name} now"').replace('installNone: "Install on your FM-1"', 'installNone: "Install latest release"')
page_script = source[source.index('<script>\n/* The page:'):source.index('</body>', source.index('<script>\n/* The page:'))]
header = '<body class="rakt-player rakt-rack rakt-install"><a class="skip" href="#main">Skip to content</a><header class="site-header"><div class="container site-header__row"><a class="brand" href="../play/">RAKT · FM-1</a><nav class="site-nav" aria-label="Install page"><a href="#quick-start">First jam</a><a href="../play/">Play in browser</a><a href="https://github.com/chasedeanda/rakt-fm1/releases">Releases</a></nav></div></header><main id="main">'
footer = '</main><footer class="site-footer"><div class="container"><p>Installer by CoralFlower / ChoralRoot / Felucca contributors. <a href="../LICENSE">GPL-3.0</a> · <a href="../LICENSING.md">Credits and licences</a></p></div></footer>'
guide = (root / 'web/rakt-play-guide.html').read_text()
rack_preview = """
<section class="section" aria-labelledby="rack-preview-title">
  <div class="container container--narrow">
    <h2 class="display-2" id="rack-preview-title">RAKT on screen</h2>
    <a class="rakt-guide__art" href="../images/rakt-rack-ui.png">
      <img src="../images/rakt-rack-ui.png" width="960" height="720" loading="lazy"
           alt="Twelve RAKT FM-1 screens showing Song, musician panels, Mix, Saved Takes, Help, Generator, ARP, FX Space and Sound Library">
    </a>
  </div>
</section>
"""
text = head + header + hero + rack_preview + guide + tools + footer + module + page_script + '</body></html>'
for filename in ('fm1pkg.js', 'fm1ota.js', 'fm1backup.js'):
    shutil.copyfile(root / 'web' / filename, out / filename)
    digest = hashlib.sha256((out / filename).read_bytes()).hexdigest()[:10]
    text = text.replace('from "./' + filename + '"', 'from "../' + filename + '?v=' + digest + '"')
ids = re.findall(r'\bid="([^"]+)"', text)
assert len(ids) == len(set(ids)), 'Duplicate installer IDs'
assert set(re.findall(r'\$\("([^"]+)"\)', module + page_script)) <= set(ids), 'Missing installer controls'
(out / 'install').mkdir(exist_ok=True)
(out / 'install/index.html').write_text(text)
(out / 'images').mkdir(exist_ok=True)
(out / 'images/rakt-boot').mkdir(exist_ok=True)
shutil.copyfile(root / 'docs/images/rakt-boot/boot.gif', out / 'images/rakt-boot/boot.gif')
shutil.copyfile(root / 'docs/images/rakt-rack-ui.png', out / 'images/rakt-rack-ui.png')
print('Packaged RAKT installer with all updater, file, backup and recovery controls')
