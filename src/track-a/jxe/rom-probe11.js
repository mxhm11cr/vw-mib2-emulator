// Probe 11: determine J9 ROM return-opcode mapping by return type.
// Compare last bytecode byte of methods with known signatures against standard JVM.
import { readFileSync } from 'node:fs';
import { parseRomImage, utf8At, selfRel } from './rom-image.js';
const b = readFileSync('firmware/analysis/rom/rom.classes');
const img = parseRomImage(b);
const u32 = (o) => b.readUInt32LE(o);

function methodsOf(c) {
  const base = c.classOff, raw = c.header.raw, mOff = raw[8] + 32;
  const starts = [];
  for (let f = mOff; f + 20 <= c.header.romSize && starts.length < raw[7]; f += 4) {
    const n = utf8At(b, selfRel(b, base + f)), s = utf8At(b, selfRel(b, base + f + 4));
    if (n && s && s.startsWith('(')) starts.push(f);
  }
  return starts.map((f, i) => {
    const m12 = u32(base + f + 12);
    const words = (m12 >>> 16) & 0xffff;
    const bcStart = base + f + 20;
    const bcEnd = i + 1 < starts.length ? base + starts[i + 1] : base + c.header.romSize;
    const len = Math.min(words * 4, bcEnd - bcStart);
    return { name: utf8At(b, selfRel(b, base + f)), sig: utf8At(b, selfRel(b, base + f + 4)), bc: b.subarray(bcStart, bcStart + len), mods: u32(base + f + 8) };
  });
}

for (const cn of ['java/lang/Void', 'java/lang/Byte', 'java/lang/Boolean', 'java/lang/Object', 'java/lang/Short']) {
  const c = img.classes.find((x) => x.name === cn);
  if (!c) continue;
  console.log(`\n=== ${cn} ===`);
  for (const m of methodsOf(c)) {
    if (m.bc.length === 0) continue;
    const ret = m.sig.slice(m.sig.indexOf(')') + 1);
    // last non-zero byte (skip padding)
    let last = m.bc.length - 1;
    while (last > 0 && m.bc[last] === 0) last--;
    const tail = [...m.bc.subarray(Math.max(0, last - 2), last + 1)].map((x) => x.toString(16).padStart(2, '0')).join(' ');
    console.log(`  ${m.name}${m.sig} ret=${ret} lastOp=0x${m.bc[last].toString(16)} tail=[${tail}]`);
  }
}
