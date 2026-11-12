#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render the fixed Signature 8 and comparison fixtures through actual firmware DSP."""
import argparse, csv, hashlib, json, subprocess, wave, array, html
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
NAMES=['EMBER KEYS','GHOST CHOIR','NOIR TINES','APOLLO VEIL','FROSTWIRE','OFFBEAT DUB','CEDAR MALLET','RAZOR PULSE']
FX=['Velvet','Wide Drift','Silver Plate','Deep Space','Phase Sweep','Slap','Silver Plate','Bite']
FIDS=[2,18,10,34,1,17,26,33,36,44,43,49,52,59]
VINDS=[23]
INHERITED=['BREATH PAD','DX TINE','AURORA','BELL BASS','ORGANISH','DX MARIMBA','HOLLOW','SYNC BASS']

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir',type=Path,default=ROOT/'build/signature8/auditions')
    args=parser.parse_args();out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    binary=ROOT/'build/host/cf_signature8_test'
    catalog=json.loads((ROOT/'assets/sounds/chase3-catalog.json').read_text())
    if isinstance(catalog,dict):catalog=catalog['sounds']
    byid={p['id']:p['name'] for p in catalog}
    comparisons=[byid[f'CF3-F{i:03}'] for i in FIDS]+[byid[f'CF3-V{i:03}'] for i in VINDS]+INHERITED
    revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    rows=[]
    jobs=[(name,mode,'signature') for name,fx in zip(NAMES,FX) for mode in ('dry',fx)]
    jobs += [(name,'dry','comparison') for name in dict.fromkeys(comparisons)]
    for name,mode,kind in jobs:
        slug=name.lower().replace(' ','-')+'-'+mode.lower().replace(' ','-')
        file=out/(slug+'-'+revision[:8]+'.wav')
        result=subprocess.run([str(binary),'--render',name,mode,str(file)],cwd=ROOT,text=True,capture_output=True)
        (out/(slug+'.log')).write_text(result.stdout+result.stderr)
        line=next((x for x in result.stdout.splitlines() if x.startswith('SIG8_RENDER,')),None)
        if not line:
            raise RuntimeError(f'Fixture failed for {name}: {result.stdout}{result.stderr}')
        values=next(csv.reader([line]))[1:]
        row=dict(zip(['name','fx','seconds','peak_dbfs','rms_dbfs','clipped_samples','peak_voices','remaining_voices','host_cpu_seconds_per_audio_second','mean_pcm','tail_rms_dbfs'],values))
        row.update(kind=kind,file=file.name,sha256=hashlib.sha256(file.read_bytes()).hexdigest(),technical_pass=result.returncode==0)
        # Check the raw WAV independently of renderer assertions. No normalization or trimming.
        with wave.open(str(file),'rb') as f:
            assert f.getnchannels()==2 and f.getsampwidth()==2 and f.getframerate()==44100
            data=array.array('h',f.readframes(f.getnframes()))
        row['pcm_clipped_samples']=sum(abs(x)>=32767 for x in data)
        rows.append(row);print(name,mode,row['peak_dbfs'],row['peak_voices'],row['technical_pass'],flush=True)
    (out/'manifest.json').write_text(json.dumps(dict(base_revision=revision,dirty=bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT)),sample_rate=44100,normalization='none',fixture='C4 vel45/80/115; Cmaj9 2s; A3-C4-E4-G4 at250ms; C3/C5; C4 sustain3s/release2s; extend decay to voice silence (90s cap)',hardware_cpu='not measured; host CPU ratio includes file output and is not an FM-1 estimate',renders=rows),indent=2)+'\n')
    with (out/'levels.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
    assert all(r['technical_pass'] and not r['pcm_clipped_samples'] for r in rows if r['kind']=='signature'), 'Signature technical failure: inspect reports; do not redesign recipes silently'
    # Repeat every Signature file in a fresh process: PCM identity, not timing identity.
    repeat=out/'reproducibility.wav'
    for row in rows:
        if row['kind']!='signature': continue
        subprocess.run([str(binary),'--render',row['name'],row['fx'],str(repeat)],cwd=ROOT,check=True,capture_output=True)
        assert hashlib.sha256(repeat.read_bytes()).hexdigest()==row['sha256']
    repeat.unlink()
    page=['<!doctype html><meta charset="utf-8"><title>RAKT Signature 8</title><style>body{font:16px system-ui;max-width:900px;margin:40px auto;background:#101719;color:#eef4ed}article{padding:20px;border-bottom:1px solid #456}audio{width:100%}</style><h1>CORD Signature 8</h1><p>Exact supplied recipes · real firmware DSP · no normalization. Technical validation, not an artist-similarity claim.</p>']
    for kind in ('signature','comparison'):
        page.append('<h2>'+kind.title()+'</h2>')
        for row in rows:
            if row['kind']!=kind: continue
            page.append(f'<article><h3>{html.escape(row["name"])} · {html.escape(row["fx"])}</h3><audio controls preload="none" src="{html.escape(row["file"])}"></audio></article>')
    (out/'index.html').write_text(''.join(page))
    print(f'{len(rows)} real-engine WAVs; no normalization; manifest and CSV: {out}')
if __name__=='__main__':main()
