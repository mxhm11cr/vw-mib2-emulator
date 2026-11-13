// Read-only J9 ROM image structure probe (Track A A2 converter development).
// Empirically resolves: header fields, class directory pairs, J9ROMClass layout,
// self-relative offset base, J9UTF8 name table. No firmware modification.
import { readFileSync } from 'node:fs';

const path = process.argv[2] || 'firmware/analysis/rom/rom.classes';
const b = readFileSync(path);
const u32 = (o) => b.readUInt32LE(o);
const i32 = (o) => b.readInt32LE(o);
const u16 = (o) => b.readUInt16LE(o);
const hex = (v) => '0x' + (v >>> 0).toString(16);

function utf8(o) {
  if (o < 0 || o + 2 > b.length) return null;
  const len = u16(o);
  if (len === 0 || len > 512 || o + 2 + len > b.length) return null;
  const s = b.toString('latin1', o + 2, o + 2 + len);
  if (!/^[\x20-\x7e]+$/.test(s)) return null;
  return { off: o, len, str: s };
}

console.log('=== HEADER ===');
console.log('magic:', b.toString('latin1', 0, 4));
console.log('version:', hex(u32(4)));
console.log('romSize:', u32(8), 'fileSize:', b.length, 'delta:', b.length - u32(8));
for (const f of [0x0c, 0x10, 0x14, 0x18, 0x1c]) {
  console.log(`+${f.toString(16)}: u32=${u32(f)} i32=${i32(f)} hex=${hex(u32(f))}`);
}
console.log('uuid:', b.toString('hex', 0x20, 0x30));

const count = u32(0x0c);
const tableEnd = 0x30 + count * 8;
console.log('\n=== CLASS DIRECTORY (candidate count=' + count + ', end=' + hex(tableEnd) + ') ===');
console.log('bytes at tableEnd:', b.toString('latin1', tableEnd, tableEnd + 40).replace(/[^\x20-\x7e]/g, '.'));
for (let i = 0; i < 6; i++) {
  const a = u32(0x30 + i * 8), c = u32(0x34 + i * 8);
  const tries = [];
  for (const d of [0, -2, 2, -4, 4, -6, 6]) {
    const n = utf8(a + d);
    if (n) tries.push(`a${d >= 0 ? '+' : ''}${d}="${n.str}"`);
  }
  console.log(`pair[${i}] a=${hex(a)} c=${hex(c)} | ${tries.join(' ') || 'a: no utf8 nearby'}`);
}

console.log('\n=== J9ROMClass candidate @ first pair.c ===');
const c0 = u32(0x34);
console.log('classOff:', hex(c0), 'romSize(+0):', u32(c0));
for (let f = 0; f < 24; f++) {
  const off = c0 + f * 4;
  if (off + 4 > b.length) break;
  const raw = u32(off), v = i32(off);
  const fieldRel = off + v, classRel = c0 + v;
  const n1 = utf8(fieldRel), n2 = utf8(classRel);
  const ann1 = n1 ? ` FIELD-REL="${n1.str}"` : '';
  const ann2 = n2 ? ` CLASS-REL="${n2.str}"` : '';
  console.log(`+${(f * 4).toString().padStart(2)} raw=${hex(raw).padEnd(10)} i32=${String(v).padStart(10)} fieldRel=${hex(fieldRel)} classRel=${hex(classRel)}${ann1}${ann2}`);
}

console.log('\n=== NAME TABLE @ tableEnd ===');
let o = tableEnd;
for (let i = 0; i < 8 && o + 2 < b.length; i++) {
  const n = utf8(o);
  if (!n) { console.log(hex(o), 'not utf8, bytes:', b.toString('hex', o, o + 8)); break; }
  console.log(hex(o), `len=${n.len}`, n.str);
  o += 2 + n.len;
  if (o % 2) o++;
}

console.log('\n=== SELF-REL BASE TEST on pair[0].c ===');
for (const f of [8, 12, 16, 20]) {
  const off = c0 + f, v = i32(off);
  for (const [label, target] of [['field', off + v], ['class', c0 + v], ['image', v]]) {
    const n = utf8(target);
    if (n) console.log(`+${f} ${label}-rel -> ${hex(target)} = "${n.str}"`);
  }
}
