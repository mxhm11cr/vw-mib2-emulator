// Isolate QNX IFS header signatures: constants present in dumpifs that occur exactly
// once in the boot image header region. Read-only analysis.
import { readFileSync } from 'node:fs';
const dump = readFileSync(process.argv[2]);
const boot = readFileSync(process.argv[3]);
const dset = new Set();
for (let o = 0; o + 4 <= dump.length; o += 4) dset.add(dump.readUInt32LE(o));
const found = [];
for (const v of dset) {
  const b = Buffer.alloc(4); b.writeUInt32LE(v);
  let c = 0, i = 0, first = -1;
  while ((i = boot.indexOf(b, i)) >= 0) { if (first < 0) first = i; c++; i += 4; }
  if (c === 1 && first >= 0) found.push({ v, off: first });
}
found.sort((a, b) => a.off - b.off);
console.log('candidate signatures (once in boot header region):');
for (const r of found) console.log('  0x' + r.v.toString(16).padStart(8, '0') + ' @ 0x' + r.off.toString(16));
for (const r of found.slice(0, 6)) {
  console.log('\nheader @0x' + r.off.toString(16) + ' (sig 0x' + r.v.toString(16) + '):');
  for (let i = 0; i < 12; i++) console.log('  +0x' + (i * 4).toString(16) + ': 0x' + boot.readUInt32LE(r.off + i * 4).toString(16).padStart(8, '0'));
}
