// Probe 5: determine what directory.classOff points to (header vs data region).
// Dump small class Void, PrintStreamLogOutput region, and File prefix.
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
function dump(base, from, to, label) {
  console.log(`\n=== ${label} (base ${hex(base)}) ===`);
  for (let f = from; f < to; f += 4) {
    const off = base + f;
    if (off < 0 || off + 4 > b.length) continue;
    const raw = u32(off), v = i32(off);
    const n = utf8(off + v);
    console.log(`${f >= 0 ? '+' : ''}${String(f).padStart(4)} ${hex(raw).padEnd(10)}${n ? '  -> "' + n + '"' : ''}`);
  }
}
const count = u32(0x0c);
const entries = [];
for (let i = 0; i < count; i++) {
  const eo = 0x30 + i * 8;
  entries.push({ name: utf8(eo + i32(eo)), c: u32(eo + 4) });
}
const find = (n) => entries.find((e) => e.name === n);

const voidE = find('java/lang/Void');
dump(voidE.c, -16, 128, `java/lang/Void dirOff=${hex(voidE.c)}`);

const pso = find('tsd/common/log/PrintStreamLogOutput');
dump(pso.c, -320, 16, `PrintStreamLogOutput dirOff=${hex(pso.c)} (before)`);
// the self-rel name hit from probe3
dump(0x551070, -16, 128, 'PrintStreamLogOutput name-hit @0x551070');

const fileE = find('java/io/File');
dump(fileE.c, -64, 8, `java/io/File dirOff=${hex(fileE.c)} (prefix)`);

// Check: is dirOff maybe pointing to fields, with class header at dirOff - X?
// For Void: scan backwards for a self-rel pointer to "java/lang/Void"
const voidNameOff = (() => { let o = 0x30 + count * 8; for (;;) { const n = utf8(o); if (!n) break; if (n === 'java/lang/Void') return o; o += 2 + u16(o); if (o % 2) o++; } })();
console.log('\nVoid name J9UTF8 @', hex(voidNameOff));
for (let p = Math.max(0, voidE.c - 512); p < voidE.c + 512; p += 4) {
  if (p + i32(p) === voidNameOff) console.log('  self-rel ptr to Void name at', hex(p), '(delta from dirOff:', p - voidE.c, ')');
}
