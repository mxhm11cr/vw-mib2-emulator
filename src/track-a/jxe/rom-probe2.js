// Enhanced read-only probe: resolve name fields across multiple class-directory entries
// to determine the J9ROMClass layout empirically (Track A A2).
import { readFileSync } from 'node:fs';
const b = readFileSync(process.argv[2] || 'firmware/analysis/rom/rom.classes');
const u32 = (o) => b.readUInt32LE(o);
const i32 = (o) => b.readInt32LE(o);
const u16 = (o) => b.readUInt16LE(o);
function utf8(o) {
  if (o < 0 || o + 2 > b.length) return null;
  const len = u16(o);
  if (len === 0 || len > 512 || o + 2 + len > b.length) return null;
  const s = b.toString('latin1', o + 2, o + 2 + len);
  return /^[\x20-\x7e]+$/.test(s) ? s : null;
}
const count = u32(0x0c);
const name = (c, f) => { const off = c + f; return utf8(off + i32(off)); };
const abs = (c, f) => utf8(u32(c + f));

console.log('pair | classOff | romSize | +4abs | +8 | +12 | +16 | +20 | +60 | +64');
for (let i = 0; i < 16; i++) {
  const c = u32(0x34 + i * 8);
  const cells = [4, 8, 12, 16, 20, 60, 64].map((f) => (f === 4 ? abs(c, f) : name(c, f)) || '-');
  console.log(`${String(i).padStart(2)} | 0x${c.toString(16)} | ${u32(c)} | ${cells.join(' | ')}`);
}

console.log('\n=== name table first 16 (compare with pair order) ===');
let o = 0x30 + count * 8;
for (let i = 0; i < 16; i++) {
  const len = u16(o);
  console.log(i, b.toString('latin1', o + 2, o + 2 + len));
  o += 2 + len; if (o % 2) o++;
}

console.log('\n=== full field scan pair[0] (all U32 as field-rel name) ===');
const c0 = u32(0x34);
for (let f = 0; f < 128; f += 4) {
  const off = c0 + f;
  if (off + 4 > b.length) break;
  const n = utf8(off + i32(off));
  const a = utf8(u32(off));
  if (n || a) console.log(`+${f}: fieldRel="${n || ''}" abs="${a || ''}" raw=0x${u32(off).toString(16)}`);
}
