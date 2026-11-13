// Probe 10: decisive layout validation across ALL classes.
// H1: methodsOffset = raw[8] + 32  (Map: 84+32=116 ✓, Void: 360+32=392 ✓)
// H2: fieldsOffset  = raw[6]       (Void: 296 -> TYPE/class$0 ✓)
// Validate by resolving method name+sig self-rel pointers and field name+desc.
import { readFileSync } from 'node:fs';
import { parseRomImage, utf8At, selfRel } from './rom-image.js';
const b = readFileSync(process.argv[2] || 'firmware/analysis/rom/rom.classes');
const img = parseRomImage(b);
const isDesc = (s) => s && /^\(/.test(s);           // method descriptor
const isFieldDesc = (s) => s && /^[BCDFIJSZL\[]/.test(s); // field descriptor

let mOk = 0, mBad = 0, fOk = 0, fBad = 0, mSkip = 0;
const badExamples = [];
for (const c of img.classes) {
  const h = c.header, base = c.classOff, raw = h.raw;
  const mOff = raw[8] + 32;
  if (h.raw[7] === 0) { mSkip++; } // numMethods==0
  else if (mOff + 8 <= h.romSize) {
    const name = utf8At(b, selfRel(b, base + mOff));
    const sig = utf8At(b, selfRel(b, base + mOff + 4));
    if (name && isDesc(sig)) mOk++;
    else { mBad++; if (badExamples.length < 8) badExamples.push(`${c.name}: mOff=${mOff} name=${name} sig=${sig}`); }
  } else { mBad++; if (badExamples.length < 8) badExamples.push(`${c.name}: mOff=${mOff} >= romSize=${h.romSize}`); }

  const fOff = raw[6];
  if (raw[9] === 0) { /* no fields */ }
  else if (fOff + 8 <= h.romSize) {
    const name = utf8At(b, selfRel(b, base + fOff));
    const desc = utf8At(b, selfRel(b, base + fOff + 4));
    if (name && isFieldDesc(desc)) fOk++;
    else { fBad++; if (badExamples.length < 16) badExamples.push(`${c.name}: fOff=${fOff} name=${name} desc=${desc} (fields)`); }
  } else fBad++;
}
console.log(`H1 methodsOffset=raw[8]+32: OK=${mOk} BAD=${mBad} skip(numMethods=0)=${mSkip} / ${img.classes.length}`);
console.log(`H2 fieldsOffset=raw[6]:     OK=${fOk} BAD=${fBad}`);
console.log('bad examples:', badExamples.slice(0, 12));

// For failures, scan for the real first method offset and report delta vs raw[8]
console.log('\n=== scan-based method offset for first 6 H1 failures ===');
let shown = 0;
for (const c of img.classes) {
  if (shown >= 6) break;
  const h = c.header, base = c.classOff, raw = h.raw;
  const mOff = raw[8] + 32;
  const name = mOff + 8 <= h.romSize ? utf8At(b, selfRel(b, base + mOff)) : null;
  const sig = mOff + 8 <= h.romSize ? utf8At(b, selfRel(b, base + mOff + 4)) : null;
  if (name && isDesc(sig)) continue;
  if (raw[7] === 0) continue;
  // scan
  let found = -1;
  for (let f = 72; f + 8 <= h.romSize; f += 4) {
    const n = utf8At(b, selfRel(b, base + f));
    const s = utf8At(b, selfRel(b, base + f + 4));
    if (n && isDesc(s)) { found = f; break; }
  }
  console.log(`${c.name}: raw[8]=${raw[8]} H1=${mOff} scanned=${found} delta=${found - raw[8]} numMethods=${raw[7]}`);
  shown++;
}
