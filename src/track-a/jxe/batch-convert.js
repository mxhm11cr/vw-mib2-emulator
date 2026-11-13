// Track A A3: batch-convert all ROM classes to standard .class files.
// Reports success/failure statistics and categorizes failures to drive format completion.
// Usage: node src/track-a/jxe/batch-convert.js [romPath] [outDir]
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { parseRomImage } from './rom-image.js';
import { convertClass } from './rom-to-class.js';

const romPath = process.argv[2] || 'firmware/analysis/rom/rom.classes';
const outDir = process.argv[3] || 'firmware/analysis/converted-all';
const b = readFileSync(romPath);
const img = parseRomImage(b);

let ok = 0, fail = 0;
const categories = new Map();
const samples = new Map();
for (const c of img.classes) {
  try {
    const { bytes } = convertClass(b, c.name, img);
    const p = join(outDir, c.name + '.class');
    mkdirSync(dirname(p), { recursive: true });
    writeFileSync(p, bytes);
    ok++;
  } catch (e) {
    fail++;
    const msg = e.message;
    const cat = /CP index .* not mapped/.test(msg) ? 'CP-index-unmapped'
      : /private opcode/.test(msg) ? 'J9-private-opcode'
      : /class not found/.test(msg) ? 'class-not-found'
      : /Cannot read|out of range|undefined/.test(msg) ? 'parse-error'
      : 'other';
    categories.set(cat, (categories.get(cat) || 0) + 1);
    if (!samples.has(cat)) samples.set(cat, []);
    if (samples.get(cat).length < 6) samples.get(cat).push(`${c.name}: ${msg}`);
  }
}
console.log(`batch convert: OK=${ok} FAIL=${fail} / ${img.classes.length}`);
console.log('failure categories:', JSON.stringify(Object.fromEntries(categories)));
for (const [cat, list] of samples) {
  console.log(`\n[${cat}] samples:`);
  for (const s of list) console.log('  ' + s);
}
