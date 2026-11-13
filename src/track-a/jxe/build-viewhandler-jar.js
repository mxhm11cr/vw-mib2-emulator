// Build a standard viewhandler.jar from authentic skin viewhandler.zip (Track A).
// Extracts all JXE ROM images inside viewhandler.zip, converts them using rom-to-class.js,
// and packages standard Java .class files into viewhandler.jar.
// This allows de.vw.mib.lsctng.internal.SkinClassLoaderImpl (viewhandler.format=jar)
// to load authentic Volkswagen view handlers directly on JDK 21 without IBM J9 XIPClassLoader.

import { readFileSync, writeFileSync } from 'node:fs';
import zlib from 'node:zlib';
import { parseRomImage } from './rom-image.js';
import { convertClass } from './rom-to-class.js';

function buildZip(entries) {
  const locals = [], centrals = [];
  let offset = 0;
  for (const { name, data } of entries) {
    const nameBuf = Buffer.from(name, 'utf8');
    const crc = zlib.crc32(data) >>> 0;
    const lh = Buffer.alloc(30);
    lh.writeUInt32LE(0x04034b50, 0);
    lh.writeUInt16LE(20, 4);
    lh.writeUInt16LE(0, 6);
    lh.writeUInt16LE(0, 8); // STORE
    lh.writeUInt16LE(0, 10);
    lh.writeUInt16LE(0x21, 12);
    lh.writeUInt32LE(crc, 14);
    lh.writeUInt32LE(data.length, 18);
    lh.writeUInt32LE(data.length, 22);
    lh.writeUInt16LE(nameBuf.length, 26);
    lh.writeUInt16LE(0, 28);
    locals.push(lh, nameBuf, data);

    const ch = Buffer.alloc(46);
    ch.writeUInt32LE(0x02014b50, 0);
    ch.writeUInt16LE(20, 4);
    ch.writeUInt16LE(20, 6);
    ch.writeUInt16LE(0, 8);
    ch.writeUInt16LE(0, 10);
    ch.writeUInt16LE(0x21, 12);
    ch.writeUInt16LE(0x21, 14);
    ch.writeUInt32LE(crc, 16);
    ch.writeUInt32LE(data.length, 20);
    ch.writeUInt32LE(data.length, 24);
    ch.writeUInt16LE(nameBuf.length, 28);
    ch.writeUInt32LE(offset, 42);
    centrals.push(ch, nameBuf);
    offset += lh.length + nameBuf.length + data.length;
  }
  const cdBuf = Buffer.concat(centrals);
  const eocd = Buffer.alloc(22);
  eocd.writeUInt32LE(0x06054b50, 0);
  eocd.writeUInt16LE(entries.length, 8);
  eocd.writeUInt16LE(entries.length, 10);
  eocd.writeUInt32LE(cdBuf.length, 12);
  eocd.writeUInt32LE(offset, 16);
  return Buffer.concat([...locals, cdBuf, eocd]);
}

export function buildViewhandlerJar(zipPath, outJar) {
  const buf = readFileSync(zipPath);
  let eocd = -1;
  for (let i = buf.length - 22; i >= 0; i--) {
    if (buf.readUInt32LE(i) === 0x06054b50) { eocd = i; break; }
  }
  if (eocd < 0) throw new Error('EOCD not found in ' + zipPath);
  let cdOff = buf.readUInt32LE(eocd + 16);
  const count = buf.readUInt16LE(eocd + 10);

  const jarEntries = [];
  let converted = 0;

  for (let n = 0; n < count; n++) {
    const compSize = buf.readUInt32LE(cdOff + 20);
    const nameLen = buf.readUInt16LE(cdOff + 28);
    const extraLen = buf.readUInt16LE(cdOff + 30);
    const commentLen = buf.readUInt16LE(cdOff + 32);
    const localOff = buf.readUInt32LE(cdOff + 42);
    const jxeName = buf.slice(cdOff + 46, cdOff + 46 + nameLen).toString('utf8');
    cdOff += 46 + nameLen + extraLen + commentLen;

    const lhNameLen = buf.readUInt16LE(localOff + 26);
    const lhExtraLen = buf.readUInt16LE(localOff + 28);
    const raw = zlib.inflateRawSync(buf.slice(localOff + 30 + lhNameLen + lhExtraLen, localOff + 30 + lhNameLen + lhExtraLen + compSize));

    let innerEocd = -1;
    for (let i = raw.length - 22; i >= 0; i--) {
      if (raw.readUInt32LE(i) === 0x06054b50) { innerEocd = i; break; }
    }
    if (innerEocd < 0) continue;
    let innerCdOff = raw.readUInt32LE(innerEocd + 16);
    const innerCount = raw.readUInt16LE(innerEocd + 10);
    for (let k = 0; k < innerCount; k++) {
      const nLen = raw.readUInt16LE(innerCdOff + 28);
      const xLen = raw.readUInt16LE(innerCdOff + 30);
      const cLen = raw.readUInt16LE(innerCdOff + 32);
      const local = raw.readUInt32LE(innerCdOff + 42);
      const cComp = raw.readUInt32LE(innerCdOff + 20);
      const iName = raw.slice(innerCdOff + 46, innerCdOff + 46 + nLen).toString('utf8');
      innerCdOff += 46 + nLen + xLen + cLen;
      if (iName === 'rom.classes') {
        const lNameLen = raw.readUInt16LE(local + 26);
        const lExtraLen = raw.readUInt16LE(local + 28);
        const romRaw = raw.slice(local + 30 + lNameLen + lExtraLen, local + 30 + lNameLen + lExtraLen + cComp);
        const rom = parseRomImage(romRaw);
        for (const cls of rom.classes) {
          const res = convertClass(romRaw, cls.name, rom);
          jarEntries.push({ name: cls.name + '.class', data: res.bytes });
          converted++;
        }
      }
    }
  }

  const jarBuf = buildZip(jarEntries);
  writeFileSync(outJar, jarBuf);
  return { entries: jarEntries.length, bytes: jarBuf.length };
}

if (process.argv[1] && process.argv[1].endsWith('build-viewhandler-jar.js')) {
  const inZip = process.argv[2] || 'firmware/analysis/hmizr/tsd/tmp/hmi/Resources/skin1/viewhandler.zip';
  const outJar = process.argv[3] || 'firmware/analysis/hmizr/tsd/tmp/hmi/Resources/skin1/viewhandler.jar';
  console.log(`[viewhandler] converting ${inZip} -> ${outJar} ...`);
  const t0 = Date.now();
  const r = buildViewhandlerJar(inZip, outJar);
  console.log(`[viewhandler] done: ${r.entries} classes packed into ${r.bytes} bytes in ${Date.now() - t0}ms`);
}
