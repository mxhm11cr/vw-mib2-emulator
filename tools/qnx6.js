// Read-only QNX6 (power-safe) filesystem reader for MIB2 eMMC dumps.
// On-disk layout per Linux include/linux/qnx6_fs.h, fs/qnx6/{inode,dir}.c.
// No writes to the image. Block cache keeps indirect-block reads cheap.
import { openSync, readSync, closeSync, statSync } from 'node:fs';

const QNX6_MAGIC = 0x68191122;
const INODE_SIZE = 0x80;
const DIR_ENTRY_SIZE = 0x20;
const SHORT_NAME_MAX = 27;
const S_IFMT = 0xf000, S_IFDIR = 0x4000, S_IFREG = 0x8000, S_IFLNK = 0xa000;

export class Qnx6 {
  constructor(path, partStartByte = 0) {
    this.fd = openSync(path, 'r');
    this.base = partStartByte;
    this.cache = new Map();
    this.fileSize = statSync(path).size;
    const sb = this.readRaw(this.base + 0x2000, 0x200);
    if (sb.readUInt32LE(0) !== QNX6_MAGIC) throw new Error('bad QNX6 magic @0x2000');
    this.blocksize = sb.readUInt32LE(0x30);
    this.numInodes = sb.readUInt32LE(0x34);
    this.numBlocks = sb.readUInt32LE(0x3c);
    this.ptrbits = Math.log2(this.blocksize / 4);
    this.blksOff = (0x2000 + 0x1000) / this.blocksize; // bootblock + superblock area
    this.inodeRoot = this.rootNode(sb, 0x48);
    this.longRoot = this.rootNode(sb, 0x48 + 0x50 * 2); // Inode, Bitmap, Longfile
    this.inodesPerBlock = this.blocksize / INODE_SIZE;
  }

  rootNode(sb, off) {
    const ptr = [];
    for (let i = 0; i < 16; i++) ptr.push(sb.readUInt32LE(off + 8 + i * 4));
    return { size: Number(sb.readBigUInt64LE(off)), ptr, levels: sb[off + 72] };
  }

  readRaw(byteOff, len) {
    const b = Buffer.alloc(len);
    readSync(this.fd, b, 0, len, byteOff);
    return b;
  }

  // physical block (relative to partition) -> cached Buffer
  readBlock(phys) {
    let b = this.cache.get(phys);
    if (b) return b;
    b = this.readRaw(this.base + phys * this.blocksize, this.blocksize);
    if (this.cache.size > 20000) this.cache.clear();
    this.cache.set(phys, b);
    return b;
  }

  // map logical block `no` through a depth-level pointer tree
  blockMap(no, depth, ptr) {
    const mask = (1 << this.ptrbits) - 1;
    let bitdelta = this.ptrbits * depth;
    let levelptr = no >> bitdelta;
    if (levelptr > 15) return -1;
    let block = ptr[levelptr] + this.blksOff;
    for (let i = 0; i < depth; i++) {
      if (block < this.blksOff) return -1;
      const bh = this.readBlock(block);
      bitdelta -= this.ptrbits;
      levelptr = (no >> bitdelta) & mask;
      const p = bh.readUInt32LE(levelptr * 4);
      if (p === 0xffffffff) return -1;
      block = p + this.blksOff;
    }
    return block;
  }

  readInode(ino) {
    if (ino < 1 || ino > this.numInodes) return null;
    const idx = ino - 1;
    const no = Math.floor(idx / this.inodesPerBlock);
    const offs = (idx % this.inodesPerBlock) * INODE_SIZE;
    const phys = this.blockMap(no, this.inodeRoot.levels, this.inodeRoot.ptr);
    if (phys < 0) return null;
    const b = this.readBlock(phys);
    const mode = b.readUInt16LE(offs + 32);
    const ptr = [];
    for (let i = 0; i < 16; i++) ptr.push(b.readUInt32LE(offs + 36 + i * 4));
    return {
      ino,
      size: Number(b.readBigUInt64LE(offs)),
      mode,
      type: mode & S_IFMT,
      mtime: b.readUInt32LE(offs + 20),
      levels: b[offs + 100],
      status: b[offs + 101],
      ptr,
    };
  }

  // read full file content (bounded by maxBytes)
  fileData(inode, maxBytes = 64 * 1024 * 1024) {
    const size = Math.min(inode.size, maxBytes);
    const out = Buffer.alloc(size);
    const nblocks = Math.ceil(inode.size / this.blocksize);
    let copied = 0;
    for (let no = 0; no < nblocks && copied < size; no++) {
      const phys = this.blockMap(no, inode.levels, inode.ptr);
      if (phys < 0) break;
      const b = this.readBlock(phys);
      const n = Math.min(this.blocksize, size - copied);
      b.copy(out, copied, 0, n);
      copied += n;
    }
    return out.subarray(0, copied);
  }

  longName(deLongInode) {
    const byteOff = deLongInode * this.blocksize;
    const no = Math.floor(byteOff / this.blocksize);
    const phys = this.blockMap(no, this.longRoot.levels, this.longRoot.ptr);
    if (phys < 0) return null;
    const b = this.readBlock(phys);
    const lfSize = b.readUInt16LE(0);
    if (lfSize > 510) return null;
    return b.toString('utf8', 2, 2 + lfSize);
  }

  readDir(inode) {
    const data = this.fileData(inode);
    const entries = [];
    for (let p = 0; p + DIR_ENTRY_SIZE <= data.length; p += DIR_ENTRY_SIZE) {
      const ino = data.readUInt32LE(p);
      const size = data[p + 4];
      if (!ino || !size) continue;
      let name;
      if (size > SHORT_NAME_MAX) {
        const longIno = data.readUInt32LE(p + 8);
        name = this.longName(longIno);
        if (name == null) name = `<long:${longIno}>`;
      } else {
        name = data.toString('utf8', p + 5, p + 5 + size);
      }
      entries.push({ name, ino });
    }
    return entries;
  }

  close() { closeSync(this.fd); }
}

export function typeName(t) {
  return t === S_IFDIR ? 'dir' : t === S_IFREG ? 'file' : t === S_IFLNK ? 'link' : '0x' + t.toString(16);
}

// CLI: node tools/qnx6.js <img> <partStartLBA> ls [path]
//      node tools/qnx6.js <img> <partStartLBA> find <substr>
//      node tools/qnx6.js <img> <partStartLBA> get <path> <outFile>
if (process.argv[1] && process.argv[1].endsWith('qnx6.js')) {
  const [img, lbaStr, cmd, ...rest] = process.argv.slice(2);
  const fs = new Qnx6(img, parseInt(lbaStr, 10) * 512);
  console.log(`QNX6 blocksize=${fs.blocksize} numBlocks=${fs.numBlocks} numInodes=${fs.numInodes} inodeLevels=${fs.inodeRoot.levels} longLevels=${fs.longRoot.levels}`);
  const root = fs.readInode(1);
  const resolve = (path) => {
    let cur = root;
    for (const part of path.split('/').filter(Boolean)) {
      if (cur.type !== S_IFDIR) return null;
      const e = fs.readDir(cur).find((x) => x.name === part);
      if (!e) return null;
      cur = fs.readInode(e.ino);
    }
    return cur;
  };
  if (cmd === 'ls') {
    const path = rest[0] || '/';
    const node = resolve(path);
    if (!node) { console.log('not found:', path); process.exit(1); }
    if (node.type !== S_IFDIR) { console.log(`${typeName(node.type)} ${node.size} ${path}`); }
    else for (const e of fs.readDir(node)) {
      const c = fs.readInode(e.ino);
      console.log(`${typeName(c.type).padEnd(4)} ${String(c.size).padStart(10)}  ${e.name}`);
    }
  } else if (cmd === 'find') {
    const needle = (rest[0] || '').toLowerCase();
    let count = 0;
    const walk = (node, path, depth) => {
      if (depth > 12 || count > 5000) return;
      let entries;
      try { entries = fs.readDir(node); } catch { return; }
      for (const e of entries) {
        if (e.name === '.' || e.name === '..') continue;
        const c = fs.readInode(e.ino);
        if (!c) continue;
        const full = path + '/' + e.name;
        if (e.name.toLowerCase().includes(needle)) { console.log(`${typeName(c.type).padEnd(4)} ${String(c.size).padStart(10)}  ${full}`); count++; }
        if (c.type === S_IFDIR) walk(c, full, depth + 1);
      }
    };
    walk(root, '', 0);
    console.log('matches:', count);
  } else if (cmd === 'get') {
    const node = resolve(rest[0]);
    if (!node || node.type !== S_IFREG) { console.log('not a file:', rest[0]); process.exit(1); }
    const data = fs.fileData(node, 512 * 1024 * 1024);
    const { writeFileSync } = await import('node:fs');
    writeFileSync(rest[1], data);
    console.log(`extracted ${data.length} bytes -> ${rest[1]}`);
  }
  fs.close();
}