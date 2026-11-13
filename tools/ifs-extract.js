// QNX IFS extraction (read-only). Decompresses LZO/ZLIB/UCL startup-compressed images,
// parses the image_header + image_dirent directory (per QNX sys/image.h), and extracts files.
// Compression framing verified against QNX dumpifs.c; LZO1X via ./lzo1x.js.
// Usage: node tools/ifs-extract.js <ifs> [list | <file-to-extract> <out>]
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';
import { decompressLzoBlocks } from './lzo1x.js';
import zlib from 'node:zlib';

const S_IFMT = 0xf000, S_IFREG = 0x8000, S_IFDIR = 0x4000, S_IFLNK = 0xa000;

export function loadIfs(buf) {
  // startup_header @0: signature u32, version u16, flags1 u8 @6, flags2 u8 @7, ...
  const flags1 = buf[6];
  const startupSize = buf.readUInt32LE(32);
  const compress = flags1 & 0x0c; // ZLIB=0x4, LZO=0x8, UCL=0xc
  let data;
  if (compress === 0x8) {
    data = decompressLzoBlocks(buf, startupSize);
  } else if (compress === 0x4) {
    data = zlib.inflateSync(buf.subarray(startupSize));
  } else if (compress === 0xc) {
    throw new Error('UCL compression not yet implemented');
  } else {
    data = buf.subarray(startupSize);
  }
  const ipos = data.indexOf(Buffer.from('imagefs'));
  if (ipos < 0) throw new Error('no imagefs signature in decompressed image');
  const image_size = data.readUInt32LE(ipos + 8);
  const hdr_dir_size = data.readUInt32LE(ipos + 12);
  const dir_offset = data.readUInt32LE(ipos + 16);
  const entries = [];
  let p = ipos + dir_offset;
  const end = ipos + hdr_dir_size;
  while (p + 24 <= end && p + 24 <= data.length) {
    const size = data.readUInt16LE(p);
    if (size < 24) break;
    const ino = data.readUInt32LE(p + 4);
    const mode = data.readUInt32LE(p + 8);
    const type = mode & S_IFMT;
    let name = '', foff = 0, fsize = 0;
    if (type === S_IFREG) {
      foff = data.readUInt32LE(p + 24);
      fsize = data.readUInt32LE(p + 28);
      name = data.toString('latin1', p + 32, p + size).replace(/\0.*$/, '');
    } else if (type === S_IFDIR) {
      name = data.toString('latin1', p + 24, p + size).replace(/\0.*$/, '');
    } else if (type === S_IFLNK) {
      name = data.toString('latin1', p + 28, p + size).replace(/\0.*$/, '');
    }
    if (ino !== 0 && name) entries.push({ name, type, offset: foff, size: fsize, mode });
    p += size;
  }
  return { data, ipos, image_size, dir_offset, hdr_dir_size, compress, entries };
}

if (process.argv[1] && process.argv[1].endsWith('ifs-extract.js')) {
  const buf = readFileSync(process.argv[2]);
  const ifs = loadIfs(buf);
  console.log(`imagefs @0x${ifs.ipos.toString(16)} compress=${ifs.compress} image_size=${ifs.image_size} dir_offset=0x${ifs.dir_offset.toString(16)} entries=${ifs.entries.length}`);
  const arg = process.argv[3];
  if (!arg || arg === 'list') {
    for (const e of ifs.entries) {
      const t = e.type === S_IFREG ? 'file' : e.type === S_IFDIR ? 'dir ' : 'link';
      console.log(`${t} ${String(e.size).padStart(9)}  ${e.name}`);
    }
  } else {
    const e = ifs.entries.find((x) => x.name === arg || x.name.endsWith('/' + arg) || x.name.endsWith(arg));
    if (!e) { console.error('not found:', arg); process.exit(1); }
    const out = process.argv[4] || ('firmware/analysis/' + e.name.split('/').pop());
    mkdirSync(dirname(out), { recursive: true });
    const content = ifs.data.subarray(ifs.ipos + e.offset, ifs.ipos + e.offset + e.size);
    writeFileSync(out, content);
    console.log(`extracted ${e.name} (${e.size} bytes) -> ${out}`);
  }
}
