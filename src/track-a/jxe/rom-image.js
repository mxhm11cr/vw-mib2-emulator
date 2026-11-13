// J9 2.3 ROM image parser (Track A A2). Format established empirically from
// firmware/analysis/rom/rom.classes (jar2jxe 2.3, 2007) — see docs/TRACK_A_HMI_ANALYSIS.md.
//
// Verified layout:
//   Header (48 B): magic "J99J", version, romSize, classNameCount@0x0C, uuid@0x20
//   Class directory @0x30: classNameCount × 8 B entries:
//     +0 i32 self-relative offset -> J9UTF8 class name   (name = utf8(eo + val))
//     +4 i32 self-relative offset -> J9ROMClass          (class = (eo+4) + val)
//   J9UTF8: u16 length + bytes, padded to even alignment
//   J9ROMClass: +0 romSize(u32), +8 className(i32 self-rel), +12 superclass(i32 self-rel), ...
//   All name/structure pointers inside J9ROMClass are self-relative from the field address.
import { readFileSync } from 'node:fs';

export function utf8At(buf, off) {
  if (off < 0 || off + 2 > buf.length) return null;
  const len = buf.readUInt16LE(off);
  if (len === 0 || len > 4096 || off + 2 + len > buf.length) return null;
  return buf.toString('latin1', off + 2, off + 2 + len);
}

export function parseHeader(buf) {
  const magic = buf.toString('latin1', 0, 4);
  if (magic !== 'J99J') throw new Error(`bad ROM magic: ${magic}`);
  return {
    magic,
    version: buf.readUInt32LE(4),
    romSize: buf.readUInt32LE(8),
    classNameCount: buf.readUInt32LE(0x0c),
    field10: buf.readInt32LE(0x10),
    field14: buf.readUInt32LE(0x14),
    field18: buf.readUInt32LE(0x18),
    uuid: buf.toString('hex', 0x20, 0x30),
    headerSize: 0x30,
  };
}

export function parseDirectory(buf, header) {
  const entries = [];
  for (let i = 0; i < header.classNameCount; i++) {
    const eo = header.headerSize + i * 8;
    const name = utf8At(buf, eo + buf.readInt32LE(eo));
    const classOff = eo + 4 + buf.readInt32LE(eo + 4);
    entries.push({ index: i, entryOff: eo, name, classOff });
  }
  return entries;
}

// Self-relative pointer read: target = fieldAddress + i32(fieldAddress)
export function selfRel(buf, fieldOff) {
  return fieldOff + buf.readInt32LE(fieldOff);
}

export function readClassHeader(buf, off) {
  const u32 = (f) => buf.readUInt32LE(off + f);
  const nameOff = selfRel(buf, off + 8);
  const superOff = selfRel(buf, off + 12);
  return {
    off,
    romSize: u32(0),
    field4: u32(4),
    className: utf8At(buf, nameOff),
    superclassName: utf8At(buf, superOff),
    raw: Array.from({ length: 24 }, (_, i) => u32(i * 4)),
  };
}

export function parseRomImage(buf) {
  const header = parseHeader(buf);
  const directory = parseDirectory(buf, header);
  const classes = directory.map((e) => ({ ...e, header: readClassHeader(buf, e.classOff) }));
  return { header, directory, classes };
}

if (process.argv[1] && process.argv[1].endsWith('rom-image.js')) {
  const buf = readFileSync(process.argv[2] || 'firmware/analysis/rom/rom.classes');
  const img = parseRomImage(buf);
  console.log('header:', JSON.stringify(img.header, null, 1));
  const unresolved = img.classes.filter((c) => !c.header.className).length;
  const mismatch = img.classes.filter((c) => c.header.className !== c.name).length;
  const sizes = img.classes.map((c) => c.header.romSize);
  const plausible = sizes.filter((s) => s >= 44 && s <= 300000).length;
  console.log(`classes: ${img.classes.length}, className unresolved: ${unresolved}, name mismatch: ${mismatch}`);
  console.log(`romSize plausible(44..300000): ${plausible}/${img.classes.length}, min=${Math.min(...sizes)}, max=${Math.max(...sizes)}`);
  console.log('\nfirst 12 class headers (raw u32 fields):');
  for (const c of img.classes.slice(0, 12)) {
    const h = c.header;
    console.log(`${c.name} @0x${c.classOff.toString(16)} size=${h.romSize} super=${h.superclassName}`);
    console.log('  raw:', h.raw.map((v) => v.toString(16).padStart(8)).join(' '));
  }
  const named = (n) => img.classes.find((c) => c.name === n);
  for (const n of ['java/lang/Void', 'java/io/File', 'tsd/common/log/PrintStreamLogOutput', 'de/vw/mib/MIBMain']) {
    const c = named(n);
    if (!c) { console.log(`\n${n}: NOT in directory`); continue; }
    const h = c.header;
    console.log(`\n${n} @0x${c.classOff.toString(16)} size=${h.romSize} super=${h.superclassName} field4=${h.field4}`);
    console.log('  raw:', h.raw.map((v) => v.toString(16).padStart(8)).join(' '));
  }
}
