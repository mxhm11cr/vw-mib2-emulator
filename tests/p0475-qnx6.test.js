import { test } from 'node:test';
import assert from 'node:assert';
import { readFileSync, existsSync } from 'node:fs';
import { Qnx6 } from '../tools/qnx6.js';
import { decompressLzoBlocks } from '../tools/lzo1x.js';
import { parseRomImage } from '../src/track-a/jxe/rom-image.js';
import { convertClass } from '../src/track-a/jxe/rom-to-class.js';

const imgPath = 'firmware/MST2_EU_VW_PQ_P0475T_IMG_CP_OFF_SWAP/MST2_EU_VW_PQ_P0475T_IMG_CP_OFF_SWAP.img';
const ifsPath = 'firmware/analysis/p0475/tsd.mibstd2.hmi.ifs';
const cleanJxePath = 'firmware/analysis/p0475/tsd.mibstd2.hmi.v2.jxe';
const p0369Rom = 'firmware/analysis/p0369/hmi/rom.classes';
const p0475Rom = 'firmware/analysis/p0475/hmi/rom.classes';

// QNX6 power-safe filesystem reader: P0475 eMMC part0 (LBA 32) is an installed MIB2 system.
(existsSync(imgPath) ? test : test.skip)('P0475 QNX6: superblock + root directory parse', () => {
  const fs = new Qnx6(imgPath, 32 * 512);
  assert.equal(fs.blocksize, 2048);
  assert.equal(fs.numInodes, 60016);
  const root = fs.readInode(1);
  const names = fs.readDir(root).map((e) => e.name);
  assert.ok(names.includes('.') && names.includes('..'), 'root has . and ..');
  assert.ok(names.includes('j9') && names.includes('tsd'), 'root has j9 and tsd');
  fs.close();
});

// Controlled byte-perfect proof: platform JXE via LZO (IFS) == via QNX6 (direct), 0 diffs.
(existsSync(ifsPath) && existsSync(cleanJxePath) ? test : test.skip)('LZO decompression is byte-perfect (IFS vs QNX6 control)', () => {
  const buf = readFileSync(ifsPath);
  const data = decompressLzoBlocks(buf, buf.readUInt32LE(32));
  assert.equal(data.length, data.readUInt32LE(8), 'decompressed size == image_size');
  assert.equal(data.toString('latin1', 0, 7), 'imagefs');
  // platform JXE dirent: offset 3836, size 5786259
  const fromIfs = data.subarray(3836, 3836 + 5786259);
  const clean = readFileSync(cleanJxePath);
  assert.equal(fromIfs.length, clean.length);
  assert.ok(fromIfs.equals(clean), 'IFS-extracted platform JXE byte-identical to QNX6-extracted');
});

// Authentic HMI ROMs: structural consistency + MIBMain converts to a standard class file.
for (const [label, rom, expected] of [['P0369', p0369Rom, 19087], ['P0475', p0475Rom, 19384]]) {
  (existsSync(rom) ? test : test.skip)(`${label} HMI rom.classes: consistent + MIBMain converts`, () => {
    const b = readFileSync(rom);
    const img = parseRomImage(b);
    assert.equal(img.header.magic, 'J99J');
    assert.equal(img.classes.length, expected);
    assert.ok(img.classes.every((c) => c.header.className === c.name), 'all directory names match class headers');
    assert.ok(img.classes.every((c) => c.classOff >= 0 && c.classOff + c.header.romSize <= b.length), 'all class bounds valid');
    const main = img.classes.find((c) => c.name === 'de/vw/mib/MIBMain');
    assert.ok(main, 'de/vw/mib/MIBMain present');
    const { bytes, info } = convertClass(b, 'de/vw/mib/MIBMain', img);
    assert.equal(bytes.readUInt32BE(0), 0xcafebabe);
    assert.equal(info.methods, 7);
    assert.equal(info.fields, 11);
  });
}