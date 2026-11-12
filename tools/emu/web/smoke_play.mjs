// SPDX-License-Identifier: GPL-3.0-only
// Exercise the actual deployable WASM: boot, render pixels, play a root, produce finite audio.
import fs from 'node:fs';
const bytes=fs.readFileSync('build/emu-web/rakt.wasm');
let mem;
const wasi=new Proxy({clock_time_get:(_id,_p,out)=>{new DataView(mem.buffer).setBigUint64(out,0n,true);return 0;},fd_write:(_fd,iov,n,out)=>{const d=new DataView(mem.buffer);let written=0;for(let i=0;i<n;i++)written+=d.getUint32(iov+8*i+4,true);d.setUint32(out,written,true);return 0;},proc_exit:(n)=>{throw Error('WASM exit '+n);}},{get:(o,k)=>o[k]||(()=>0)});
const {instance}=await WebAssembly.instantiate(bytes,{wasi_snapshot_preview1:wasi,env:new Proxy({},{get:()=>()=>0})});
const ex=instance.exports;mem=ex.memory;if(ex._initialize)ex._initialize();ex.web_boot();ex.web_master(724);
for(let i=0;i<900;i++)ex.web_render(128);
const lcd=new Uint16Array(mem.buffer,ex.web_fb(),57600);if(new Set(lcd).size<3)throw Error('LCD did not render');
ex.web_keys(1<<7);let peak=0;
for(let b=0;b<160;b++){ex.web_render(128);for(const ptr of [ex.web_out_l(),ex.web_out_r()])for(const v of new Float32Array(mem.buffer,ptr,128)){if(!Number.isFinite(v))throw Error('Nonfinite audio');peak=Math.max(peak,Math.abs(v));}}
ex.web_keys(0);if(peak<0.0001)throw Error('Root produced no sound');
console.log('PASS: WASM boots, draws the native screen and produces audio; peak='+peak.toFixed(4));
