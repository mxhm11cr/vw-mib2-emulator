// Probe 9: decisive layout resolution using java/util/Map (interface, 14 abstract
// methods, 0 fields). Header: +24=88 +28=14 +32=84 +36=0 +40=72 +44=0.
// Also re-examine Void +360 region as raw hex (method table vs CP).
import { readFileSync } from 'node:fs';
import { parseRomImage, utf8At, selfRel } from './rom-image.js';
const b = readFileSync(process.argv[2] || 'firmware/analysis/rom/rom.classes');
const img = parseRomImage(b);
const u32 = (o) => b.readUInt32LE(o);
const hex = (v) => '0x' + (v >>> 0).toString(16);
function dump(c, from, to, label) {
  console.log(`\n=== ${label}: ${c.name} @${hex(c.classOff)} size=${c.header.romSize} ===`);
  console.log('hdr[6..11]:', c.header.raw.slice(6, 12).map((v) => hex(v)).join(' '));
  for (let f = from; f < to && f < c.header.romSize; f += 4) {
    const off = c.classOff + f;
    const n = utf8At(b, selfRel(b, off));
    console.log(`+${String(f).padStart(3)} ${hex(u32(off)).padEnd(10)}${n ? '  -> "' + n + '"' : ''}`);
  }
}
const map = img.classes.find((x) => x.name === 'java/util/Map');
dump(map, 72, 200, 'Map data region');
const voidC = img.classes.find((x) => x.name === 'java/lang/Void');
console.log('\n=== Void +352..+416 raw (method table candidate @+360) ===');
for (let f = 352; f < 416; f += 4) {
  const off = voidC.classOff + f;
  console.log(`+${f} ${b.toString('hex', off, off + 4)}  ${hex(u32(off))}`);
}
// Where are Map method names? scan for self-rel ptrs to "size","isEmpty","containsKey"
console.log('\n=== Map: locate method name pointers ===');
const nameTableStart = 0x30 + img.header.classNameCount * 8;
for (const target of ['size', 'isEmpty', 'containsKey', 'hashCode']) {
  // find J9UTF8 offset in name table
  let o = nameTableStart, found = -1;
  for (let i = 0; i < 3000 && o + 2 < b.length; i++) {
    const len = b.readUInt16LE(o);
    if (len === 0 || len > 512) break;
    const s = b.toString('latin1', o + 2, o + 2 + len);
    if (s === target) { found = o; break; }
    o += 2 + len; if (o % 2) o++;
  }
  if (found < 0) { console.log(target, ': name not found'); continue; }
  const hits = [];
  for (let f = 0; f < map.header.romSize; f += 4) {
    const off = map.classOff + f;
    if (selfRel(b, off) === found) hits.push(f);
  }
  console.log(`"${target}" name@${hex(found)}: Map class-relative hits at +${hits.join(', +')}`);
}
