// Probe 8: full unfiltered dump of java/lang/Void ROM class to locate
// J9ROMMethod / J9ROMFieldShape / bytecode structures. Cross-ref: standard
// Void.class has fields TYPE, class$0; methods <init>()V, <clinit>()V.
import { readFileSync } from 'node:fs';
import { parseRomImage, utf8At, selfRel } from './rom-image.js';
const b = readFileSync(process.argv[2] || 'firmware/analysis/rom/rom.classes');
const img = parseRomImage(b);
const u32 = (o) => b.readUInt32LE(o);
const hex = (v) => '0x' + (v >>> 0).toString(16);
const c = img.classes.find((x) => x.name === 'java/lang/Void');
const base = c.classOff, size = c.header.romSize;
console.log(`Void @${hex(base)} size=${size}`);
console.log('hdr: +24=' + c.header.raw[6] + ' +28=' + c.header.raw[7] + ' +32=' + c.header.raw[8] + ' +36=' + c.header.raw[9] + ' +40=' + c.header.raw[10] + ' +44=' + c.header.raw[11] + ' +52=' + c.header.raw[13] + ' +56=' + c.header.raw[14] + ' +72=' + c.header.raw[18] + ' +76=' + c.header.raw[19] + ' +92=' + c.header.raw[23]);
for (let f = 0; f < size; f += 4) {
  const off = base + f;
  const raw = u32(off);
  const n = utf8At(b, selfRel(b, off));
  const bytes = b.toString('hex', off, off + 4);
  console.log(`+${String(f).padStart(3)} ${hex(raw).padEnd(10)} [${bytes}]${n ? '  -> "' + n.replace(/[^\x20-\x7e]/g, '.') + '"' : ''}`);
}
