// Probe 6: definitively resolve directory second-value semantics.
// For each entry: nameOff = eo + i32(eo) (ground truth). Test target = u32(eo+4) [absolute]
// vs target = (eo+4) + i32(eo+4) [self-relative]. Scan target structure for a self-rel
// pointer resolving to nameOff -> that field is className. Report match offset.
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
  return /^[\x20-\x7e]+$/.test(s) ? s : null;
}
const count = u32(0x0c);
let absHits = 0, selfHits = 0;
const fieldOffsets = {};
console.log('idx | name | absTarget className@ | selfTarget className@');
for (let i = 0; i < 40; i++) {
  const eo = 0x30 + i * 8;
  const nameOff = eo + i32(eo);
  const name = utf8(nameOff);
  const absT = u32(eo + 4);
  const selfT = (eo + 4) + i32(eo + 4);
  const scan = (t) => {
    if (t < 0 || t + 256 > b.length) return null;
    for (let f = 0; f < 256; f += 4) {
      if (t + f + i32(t + f) === nameOff) return f;
    }
    return null;
  };
  const a = scan(absT), s = scan(selfT);
  if (a !== null) { absHits++; fieldOffsets['abs+' + a] = (fieldOffsets['abs+' + a] || 0) + 1; }
  if (s !== null) { selfHits++; fieldOffsets['self+' + s] = (fieldOffsets['self+' + s] || 0) + 1; }
  if (i < 20 || a !== null || s !== null) {
    console.log(`${String(i).padStart(2)} | ${name} | ${a !== null ? '+' + a : '-'} | ${s !== null ? '+' + s : '-'}`);
  }
}
console.log(`\nabsHits=${absHits} selfHits=${selfHits} (of 40)`);
console.log('className field offset histogram:', JSON.stringify(fieldOffsets));

// Also test: maybe table starts elsewhere. Check header +0x14=28 well-known classes hypothesis:
// if first 28 U32 at 0x30 are well-known class offsets, class list starts at 0x30+28*4=0xA0.
console.log('\n=== alt table start 0xA0 test (first 5) ===');
for (let i = 0; i < 5; i++) {
  const eo = 0xA0 + i * 8;
  const nameOff = eo + i32(eo);
  const name = utf8(nameOff);
  const c = u32(eo + 4);
  console.log(`${i} eo=${hex(eo)} name=${name || '???'} c=${hex(c)} u32@c=${u32(c)}`);
}
