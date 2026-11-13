// Probe 7: full annotation of java/lang/Void ROM class (584 B) to identify
// interfaces/methods/fields/constantPool offsets and internal layouts.
import { readFileSync } from 'node:fs';
import { parseRomImage, utf8At, selfRel } from './rom-image.js';
const b = readFileSync(process.argv[2] || 'firmware/analysis/rom/rom.classes');
const img = parseRomImage(b);
const u32 = (o) => b.readUInt32LE(o);
const i32 = (o) => b.readInt32LE(o);
const hex = (v) => '0x' + (v >>> 0).toString(16);

const c = img.classes.find((x) => x.name === 'java/lang/Void');
const base = c.classOff, size = c.header.romSize;
console.log(`Void @${hex(base)} romSize=${size}`);
console.log('header raw[0..17]:', c.header.raw.slice(0, 18).map((v) => v.toString(16)).join(' '));

// annotate every u32 in the class: self-rel name targets
console.log('\n=== full class dump (self-rel annotations) ===');
for (let f = 0; f < size; f += 4) {
  const off = base + f;
  const raw = u32(off);
  const n = utf8At(b, selfRel(b, off));
  // also try as absolute offset into name table
  const tag = n ? `  -> "${n}"` : '';
  if (f < 96 || n || (raw > 0 && raw < size)) console.log(`+${String(f).padStart(3)} ${hex(raw).padEnd(10)}${tag}`);
}

// candidate offsets from header
const h = c.header.raw;
console.log('\n=== candidate region dumps ===');
for (const [label, f] of [['+24', 24], ['+32', 32], ['+40', 40], ['+52', 52], ['+56', 56], ['+72', 72], ['+76', 76]]) {
  const rel = h[f / 4];
  if (rel === 0 || rel >= size) { console.log(`${label}=${hex(rel)} (out of range)`); continue; }
  console.log(`\n${label} = ${hex(rel)} (region @+${rel}):`);
  for (let k = 0; k < 48 && rel + k < size; k += 4) {
    const off = base + rel + k;
    const n = utf8At(b, selfRel(b, off));
    console.log(`  +${rel + k} ${hex(u32(off)).padEnd(10)}${n ? ' -> "' + n + '"' : ''}`);
  }
}
