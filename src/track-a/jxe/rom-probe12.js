// Probe 12: locate the real J9ROMFieldShape table by scanning for numFields consecutive
// {name(selfRel), fieldDesc(selfRel), modifiers} entries (12-byte stride). Correlate the
// found offset with header raw fields to identify the correct fieldsOffset field.
import { readFileSync } from 'node:fs';
import { parseRomImage, utf8At, selfRel } from './rom-image.js';
const b = readFileSync('firmware/analysis/rom/rom.classes');
const img = parseRomImage(b);
const u32 = (o) => b.readUInt32LE(o);
const isFieldDesc = (s) => s && /^[BCDFIJSZL\[]/.test(s) && s.length < 200;

function findFieldTable(c) {
  const base = c.classOff, raw = c.header.raw, n = raw[9], mOff = raw[8] + 32;
  if (n === 0) return [];
  const hits = [];
  for (let off = 96; off + n * 12 <= mOff; off += 4) {
    let good = 0;
    for (let i = 0; i < n; i++) {
      const fo = base + off + i * 12;
      const name = utf8At(b, selfRel(b, fo));
      const desc = utf8At(b, selfRel(b, fo + 4));
      if (name && /^[\x20-\x7e]+$/.test(name) && isFieldDesc(desc)) good++;
      else break;
    }
    if (good === n) hits.push(off);
  }
  return hits;
}

for (const cn of ['java/lang/Void', 'java/io/File', 'java/lang/Byte', 'com/ibm/j9/bluez/crypto/AESCipher', 'java/util/Vector', 'tsd/common/log/Logger']) {
  const c = img.classes.find((x) => x.name === cn);
  if (!c) { console.log(cn, 'not found'); continue; }
  const hits = findFieldTable(c);
  const raw = c.header.raw;
  console.log(`\n${cn}: numFields=${raw[9]} numIfaces=${raw[5]} fieldTable candidates(rel)=${hits.join(',')}`);
  console.log('  raw[6..14]=' + raw.slice(6, 15).join(',') + '  methodsOff=' + (raw[8] + 32));
  for (const h of hits) {
    const match = raw.map((v, i) => v === h ? i : -1).filter((i) => i >= 0);
    console.log(`  candidate ${h} matches raw indices: ${match.join(',')}`);
  }
}
