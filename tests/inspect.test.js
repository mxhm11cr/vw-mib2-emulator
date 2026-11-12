import { test } from 'node:test';
import assert from 'node:assert';
import { detectFormat, parseElf, listZip, sha256 } from '../tools/inspect-firmware.js';

// Synthetic fixtures (clearly NOT authentic VW firmware) to verify the inspector logic.
function arm32Elf() {
  const b = Buffer.alloc(64);
  b.write('\x7fELF', 0, 'ascii');
  b[4] = 1; // ELFCLASS32
  b[5] = 1; // little-endian
  b.writeUInt16LE(0x28, 18); // EM_ARM
  return b;
}

function aarch64Elf() {
  const b = Buffer.alloc(64);
  b.write('\x7fELF', 0, 'ascii');
  b[4] = 2; b[5] = 1;
  b.writeUInt16LE(0xb7, 18); // EM_AARCH64
  return b;
}

function minimalZip(name) {
  const nameB = Buffer.from(name, 'utf8');
  const central = Buffer.alloc(46 + nameB.length);
  central.writeUInt32LE(0x02014b50, 0);
  central.writeUInt32LE(0, 20); // compSize
  central.writeUInt32LE(0, 24); // uncompSize
  central.writeUInt16LE(nameB.length, 28);
  nameB.copy(central, 46);
  const eocd = Buffer.alloc(22);
  eocd.writeUInt32LE(0x06054b50, 0);
  eocd.writeUInt16LE(1, 8);
  eocd.writeUInt16LE(1, 10);
  eocd.writeUInt32LE(central.length, 12);
  eocd.writeUInt32LE(4, 16); // central dir starts after the 4-byte local-header magic
  return Buffer.concat([Buffer.from([0x50, 0x4b, 0x03, 0x04]), central, eocd]);
}

test('detects ARM32 ELF and reports ARMv7-A ISA (MIB2 Standard expectation)', () => {
  const b = arm32Elf();
  assert.equal(detectFormat(b), 'elf');
  const e = parseElf(b);
  assert.equal(e.class, 32);
  assert.equal(e.isa, 'ARM (ARMv7-A 32-bit)');
});

test('detects AArch64 ELF (MIB3 expectation)', () => {
  assert.equal(parseElf(aarch64Elf()).isa, 'AArch64');
});

test('detects 7z, ext4, android-boot magics', () => {
  assert.equal(detectFormat(Buffer.from([0x37, 0x7a, 0xbc, 0xaf, 0x27, 0x1c])), '7z');
  const ext = Buffer.alloc(2048); ext.writeUInt16LE(0xef53, 1024 + 56);
  assert.equal(detectFormat(ext), 'ext4');
  assert.equal(detectFormat(Buffer.from('ANDROID!xxxxxxxx')), 'android-boot');
});

test('lists zip central directory entries', () => {
  const z = minimalZip('metainfo2.txt');
  assert.equal(detectFormat(z), 'zip');
  const { entries } = listZip(z);
  assert.equal(entries.length, 1);
  assert.equal(entries[0].name, 'metainfo2.txt');
});

test('sha256 is deterministic', () => {
  assert.equal(sha256(Buffer.from('abc')), sha256(Buffer.from('abc')));
  assert.equal(sha256(Buffer.from('abc')).length, 64);
});
