// Probe 3: locate real J9ROMClass structures by scanning for self-relative pointers
// to known class names (name table is ground truth). Read-only, empirical.
import { readFileSync } from 'node:fs';
const b = readFileSync(process.argv[2] || 'firmware/analysis/rom/rom.classes');
const u32 = (o) => b.readUInt32LE(o);
const i32 = (o) => b.readInt32LE(o);
const u16 = (o) => b.readUInt16LE(o);
const hex = (v) => '0x' + (v >>> 0).toString(16);
function utf8(o) {
  if (o < 0 || o + 2 > b.length) return null;
  const len = u16(o);
  if (len === 0 || len > 512 || o + 2 + len > b.length) return null;
  const s = b.toString('latin1', o + 2, o + 2 + len);
  return /^[\x20-\x7e]+$/.test(s) ? { len, str: s } : null;
}

// 1. Header +0x18 candidate region
const h18 = u32(0x18);
console.log('=== header+0x18 =', hex(h18), '===');
console.log('bytes:', b.toString('hex', h18, h18 + 64));
for (let i = 0; i < 8; i++) {
  const v = u32(h18 + i * 4);
  const n = utf8(v);
  console.log(`  [${i}] ${hex(v)}${n ? ' -> "' + n.str + '"' : ''}`);
}

// 2. Name table index (first 4096 entries or until non-name)
const count = u32(0x0c);
let o = 0x30 + count * 8;
const names = [];
while (names.length < 4096 && o + 2 < b.length) {
  const n = utf8(o);
  if (!n) break;
  names.push({ off: o, str: n.str });
  o += 2 + n.len; if (o % 2) o++;
}
console.log('\n=== name table entries parsed:', names.length, 'last:', names[names.length - 1]?.str, '===');

// 3. Scan for self-relative pointers to first 6 names
console.log('\n=== self-rel pointer scan (target = name J9UTF8 start) ===');
for (const nm of names.slice(0, 6)) {
  const hits = [];
  for (let p = 0; p + 4 <= b.length; p += 4) {
    if (p + i32(p) === nm.off) hits.push(p);
    if (hits.length >= 4) break;
  }
  console.log(`"${nm.str}" @${hex(nm.off)}: hits at ${hits.map(hex).join(', ') || 'none'}`);
  for (const h of hits.slice(0, 2)) {
    // dump surrounding 64 bytes before to find structure start
    const start = Math.max(0, h - 64);
    console.log(`  context @${hex(h)} (field), -64..+16:`);
    for (let f = -64; f <= 16; f += 4) {
      const off = h + f;
      if (off < 0 || off + 4 > b.length) continue;
      const raw = u32(off);
      const tgt = off + i32(off);
      const n2 = utf8(tgt);
      console.log(`    ${f >= 0 ? '+' : ''}${f}: ${hex(raw)}${n2 ? ' -> "' + n2.str + '"' : ''}`);
    }
  }
}
