// Build a JDK-loadable classpath JAR from authentic J9 ROM images (Track A A4).
// Converts HMI + platform ROM classes to standard .class files and packs them into a single
// (STORE-method) JAR — one file write, far faster than thousands of individual class files on
// Windows. java.* is skipped (the JDK bootstrap provides it). Read-only on the firmware.
import { readFileSync, writeFileSync } from 'node:fs';
import zlib from 'node:zlib';
import { parseRomImage } from './rom-image.js';
import { convertClass } from './rom-to-class.js';

// Minimal ZIP writer (STORE method, no compression). Sufficient for a classpath JAR.
function buildZip(entries) {
  const locals = [], centrals = [];
  let offset = 0;
  for (const { name, data } of entries) {
    const nameBuf = Buffer.from(name, 'utf8');
    const crc = zlib.crc32(data) >>> 0;
    const lh = Buffer.alloc(30);
    lh.writeUInt32LE(0x04034b50, 0);
    lh.writeUInt16LE(20, 4);            // version needed
    lh.writeUInt16LE(0, 6);             // flags
    lh.writeUInt16LE(0, 8);             // method = STORE
    lh.writeUInt16LE(0, 10);            // mod time
    lh.writeUInt16LE(0x21, 12);         // mod date (1980-01-01)
    lh.writeUInt32LE(crc, 14);
    lh.writeUInt32LE(data.length, 18);  // compressed size
    lh.writeUInt32LE(data.length, 22);  // uncompressed size
    lh.writeUInt16LE(nameBuf.length, 26);
    lh.writeUInt16LE(0, 28);            // extra len
    locals.push(lh, nameBuf, data);

    const ch = Buffer.alloc(46);
    ch.writeUInt32LE(0x02014b50, 0);
    ch.writeUInt16LE(20, 4);            // version made by
    ch.writeUInt16LE(20, 6);            // version needed
    ch.writeUInt16LE(0, 8);             // flags
    ch.writeUInt16LE(0, 10);            // method
    ch.writeUInt16LE(0, 12);            // mod time
    ch.writeUInt16LE(0x21, 14);         // mod date
    ch.writeUInt32LE(crc, 16);
    ch.writeUInt32LE(data.length, 20);
    ch.writeUInt32LE(data.length, 24);
    ch.writeUInt16LE(nameBuf.length, 28);
    // extra/comment/disk/attrs = 0
    ch.writeUInt32LE(offset, 42);       // local header offset
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

export function buildClasspathJar(romPaths, outJar) {
  const byName = new Map();
  let ok = 0, fail = 0, skip = 0;
  const fails = {};
  for (const romPath of romPaths) {
    const b = readFileSync(romPath);
    const img = parseRomImage(b);
    for (const c of img.classes) {
      if (c.name.startsWith('java/')) { skip++; continue; }
      const entryName = c.name + '.class';
      if (byName.has(entryName)) continue; // first ROM wins (HMI before platform)
      try {
        const { bytes } = convertClass(b, c.name, img);
        byName.set(entryName, bytes);
        ok++;
      } catch (e) {
        fail++;
        const k = e.message.replace(/[0-9]+/g, 'N').slice(0, 45);
        fails[k] = (fails[k] || 0) + 1;
      }
    }
  }
  const entries = [...byName.entries()].map(([name, data]) => ({ name, data }));
  writeFileSync(outJar, buildZip(entries));
  return { ok, fail, skip, fails, entries: entries.length, bytes: entries.reduce((s, e) => s + e.data.length, 0) };
}

if (process.argv[1] && process.argv[1].endsWith('build-hmi-classpath.js')) {
  const outJar = process.argv[2];
  const roms = process.argv.slice(3);
  const r = buildClasspathJar(roms, outJar);
  console.log(`classpath JAR -> ${outJar} (${r.entries} classes, ${r.bytes} bytes uncompressed)`);
  console.log(`converted ok=${r.ok} fail=${r.fail} skip(java.*)=${r.skip}`);
  console.log('failure categories:', JSON.stringify(r.fails, null, 1));
}