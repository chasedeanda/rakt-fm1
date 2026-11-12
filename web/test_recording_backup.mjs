// SPDX-License-Identifier: GPL-3.0-only
// Focused recording backup specs, without running the installer regression suite.
import {spawnSync} from "node:child_process";
import {bkU32,bkR32,bkPack,bkUnpack,bkCrc,CF_BACKUP_IDS,captureBackup,restoreBackup,backupFileName,objectNames} from "./fm1backup.js";
let failed=0;
const ok=(x,s)=>{console.log((x?"PASS: ":"FAIL: ")+s);if(!x)failed++;};
const fill=(n,seed)=>Uint8Array.from({length:n},(_,i)=>(i*31+seed)&255);
const cfData=()=>[50,51,52,53,54,55].map(id=>[id,fill(id===55?1136:3840,id)]);
function backupSide(version, ids, objs = [], refuse = []) {   // refuse: ids whose content the firmware refuses (rc 2)
  const b = { objs: new Map(objs), log: [], staged: null, restarts: 0 };
  b.handle = (cmd, a) => {
    if (cmd === 1) return [...new TextEncoder().encode(version), 0, 0, 0, 0, 0, 0, 0, 0, 0x42, 1, 3];
    if (cmd === 65) {
      const o = [1, 0, ids.length];
      for (const id of ids) { const v = b.objs.get(id) || new Uint8Array(0); o.push(id, ...bkU32(v.length), ...bkU32(v.length ? bkCrc(v) : 0)); }
      return o;
    }
    if (cmd === 66) { const id = a[0], off = bkR32(a, 1), n = a[6] | a[7] << 7, v = b.objs.get(id); return [id, 0, ...bkU32(off), n & 127, n >> 7, ...bkPack(v.subarray(off, off + n))]; }
    if (cmd === 67) {
      const [op, id] = a;
      if (op === 0) { b.staged = { id, size: bkR32(a, 2), crc: bkR32(a, 7), bytes: [] }; return [op, id, ids.includes(id) ? 0 : 1]; }
      if (op === 1) { b.staged.bytes.push(...bkUnpack(a.slice(7), Math.min(256, b.staged.size - bkR32(a, 2)))); return [op, id, 0]; }
      if (op === 2) { const v = Uint8Array.from(b.staged.bytes); const rc = bkCrc(v) === b.staged.crc && !refuse.includes(id) ? 0 : 2; if (!rc) { b.objs.set(id, v); b.log.push(id); } return [op, id, rc]; }
      return [op, id, 0];
    }
    if (cmd === 72 && /^(ChoralRoot|CoralFlower|CORD|RAKT)/.test(version)) { b.restarts++; return [0]; }
    return null;
  };
  return b;
}
// A backup's object order is not a storage transaction order: archived chunks must
// precede independent takes, even when the JSON objects have been rearranged.
for (const [sourceVersion,targetVersion] of [["CoralFlower dev","CoralFlower dev"],["CoralFlower 1.0","CORD 1.0"],["CORD 1.0","CoralFlower 1.0"],["CORD 1.0","CORD 1.0"],["CORD 1.0","RAKT 0.1.0"],["RAKT 0.1.0","CORD 1.0"]]) {
  const ids = CF_BACKUP_IDS;
  const takes = [[71,2556],[72,220],[73,3804]].map(([id,size]) => [id,fill(size,id)]);
  const source = backupSide(sourceVersion,ids,[...cfData(),...takes]);
  const file = await captureBackup(async ([cmd,args]) => Uint8Array.from(source.handle(cmd,args)),sourceVersion);
  file.objects.reverse();
  const dest = backupSide(targetVersion,ids);
  await restoreBackup(async ([cmd,args]) => Uint8Array.from(dest.handle(cmd,args)),file);
  ok(takes.every(([id,v]) => dest.objs.get(id)?.every((b,i) => b===v[i])) &&
     Math.max(...[50,51,52,53,54,55].map((id) => dest.log.indexOf(id))) <
     Math.min(...[71,72,73].map((id) => dest.log.indexOf(id))),
     "recordings: reordered backup restores archives first and preserves all three takes");
  const py=spawnSync("python3",["-c",`
import json,sys
sys.path.insert(0,"tools")
import fm1_install as m
class Device:
    def __init__(self): self.log=[]
    def request(self,cmd,args,timeout):
        if cmd==m.BK_LIST:
            a=[1,0,len(m.CF_IDS)]
            for i in m.CF_IDS: a += [i]+m.bk_u32(0)+m.bk_u32(0)
            return a
        op,i=args[:2]
        if op==2: self.log.append(i)
        return [op,i,0]
d=Device();m.restore(d,json.load(sys.stdin))
assert max(d.log.index(i) for i in range(50,56)) < min(d.log.index(i) for i in (71,72,73))
assert all(i in d.log for i in (71,72,73))
`],{input:JSON.stringify(file),encoding:"utf8"});
  ok(py.status===0,"recordings: Python restorer handles the same reordered backup ("+py.stderr.trim()+")");
}

ok(backupFileName("CORD 1.0",new Date("2026-11-08T00:00:00Z"))==="cord-backup-20261108.json","CORD dated backup filename");
ok(!/ChaseFlower|CoralFlower/.test(objectNames([30,31,50,56,59,68,69,70,71,74,80,85])),"RAKT labels for every fork-owned backup object");
if(failed)process.exitCode=1;
