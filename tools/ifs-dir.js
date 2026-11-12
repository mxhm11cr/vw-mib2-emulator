// Parse the QNX imagefs directory (sys/image.h union image_dirent) and list authentic
// OS components (procnto, startup, drivers, libs). Read-only.
import { readFileSync } from 'node:fs';
import { findImagefs } from './ifs-parse.js';

const S_IFMT = 0xf000, S_IFREG = 0x8000, S_IFDIR = 0x4000, S_IFLNK = 0xa000, S_IFCHR = 0x2000;

export function listImagefs(buf, ifsOff) {
  const dirOffset = buf.readUInt32LE(ifsOff + 16);
  const hdrDirSize = buf.readUInt32LE(ifsOff + 12);
  let p = ifsOff + dirOffset;
  const end = ifsOff + hdrDirSize + 0x10000; // generous bound
  const entries = [];
  while (p + 24 <= buf.length && p < end && entries.length < 4096) {
    const size = buf.readUInt16LE(p);
    if (size === 0) break;
    const ino = buf.readUInt32LE(p + 4);
    const mode = buf.readUInt32LE(p + 8);
    const type = mode & S_IFMT;
    let name = '', foff = 0, fsize = 0;
    if (type === S_IFREG) {
      foff = buf.readUInt32LE(p + 24);
      fsize = buf.readUInt32LE(p + 28);
      name = buf.toString('utf8', p + 32, p + size).replace(/\0.*$/, '');
    } else if (type === S_IFDIR || type === S_IFLNK || type === S_IFCHR) {
      name = buf.toString('utf8', p + 24, p + size).replace(/\0.*$/, '');
    }
    if (ino !== 0) entries.push({ name, type, mode, ino, offset: foff, size: fsize });
    p += size;
  }
  return entries;
}

if (process.argv[1] && process.argv[1].endsWith('ifs-dir.js')) {
  const buf = readFileSync(process.argv[2]);
  const ifs = findImagefs(buf);
  if (!ifs.length) { console.log('no imagefs'); process.exit(1); }
  const f = ifs[0];
  console.log('imagefs @0x' + f.offset.toString(16) + ' dir_offset=0x' + f.dir_offset.toString(16));
  const entries = listImagefs(buf, f.offset);
  const typeName = (t) => t === S_IFREG ? 'file' : t === S_IFDIR ? 'dir' : t === S_IFLNK ? 'link' : t === S_IFCHR ? 'chr' : '0x' + t.toString(16);
  console.log('total entries:', entries.length);
  for (const e of entries) {
    console.log(`${typeName(e.type).padEnd(4)} ${String(e.size).padStart(9)}  ${e.name}`);
  }
}
