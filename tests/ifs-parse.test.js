import { test } from 'node:test';
import assert from 'node:assert';
import { readFileSync } from 'node:fs';
import {
  parseStartupHeader, validateStartupHeader, findStartupHeaders, findImagefs,
  STARTUP_HDR_SIGNATURE,
} from '../tools/ifs-parse.js';

// Independently-constructed reference startup header per QNX sys/startup.h layout.
function referenceHeader() {
  const b = Buffer.alloc(256);
  b.writeUInt32LE(0x00ff7eeb, 0);   // signature
  b.writeUInt16LE(1, 4);            // version
  b[6] = 0x01;                      // flags1 VIRTUAL
  b[7] = 0x00;                      // flags2
  b.writeUInt16LE(256, 8);          // header_size
  b.writeUInt16LE(40, 10);          // machine EM_ARM
  b.writeUInt32LE(0x10805fb4, 12);  // startup_vaddr
  b.writeUInt32LE(0, 16);           // paddr_bias
  b.writeUInt32LE(0x10800008, 20);  // image_paddr
  b.writeUInt32LE(0x1039e, 24);     // ram_paddr
  b.writeUInt32LE(0x91eef8, 28);    // ram_size
  b.writeUInt32LE(0x1e108, 32);     // startup_size
  b.writeUInt32LE(0xb0003, 36);     // stored_size
  b.writeUInt32LE(0x900df0, 40);    // imagefs_paddr
  b.writeUInt32LE(0x40008, 44);     // imagefs_size
  b.writeUInt16LE(0xf000, 48);      // preboot_size
  return b;
}

test('parses independently-constructed reference startup header', () => {
  const h = parseStartupHeader(referenceHeader(), 0);
  assert.equal(h.signature, STARTUP_HDR_SIGNATURE);
  assert.equal(h.version, 1);
  assert.equal(h.header_size, 256);
  assert.equal(h.machine, 40);
  assert.equal(h.startup_vaddr, 0x10805fb4);
  assert.equal(h.image_paddr, 0x10800008);
  assert.equal(h.startup_size, 0x1e108);
  assert.equal(validateStartupHeader(h).valid, true);
});

test('validator rejects bad machine / version / signature', () => {
  const bad = referenceHeader(); bad.writeUInt16LE(0xee, 10); // wrong machine
  assert.equal(validateStartupHeader(parseStartupHeader(bad, 0)).machine_arm, false);
  const bad2 = referenceHeader(); bad2.writeUInt16LE(9, 4); // wrong version
  assert.equal(validateStartupHeader(parseStartupHeader(bad2, 0)).version, false);
  const bad3 = referenceHeader(); bad3.writeUInt32LE(0xdeadbeef, 0);
  assert.equal(validateStartupHeader(parseStartupHeader(bad3, 0)).signature, false);
});

test('authentic P0369 firmware: valid startup header establishes entry reliably', () => {
  const buf = readFileSync('firmware/extracted/P0369/cpu/cpuimage_sec_stdNavi/33/default/mibstd2_cpu.boot');
  const hs = findStartupHeaders(buf);
  const valid = hs.filter((h) => validateStartupHeader(h).valid);
  assert.equal(valid.length, 1, 'exactly one valid startup header');
  const h = valid[0];
  assert.equal(h.offset, 0x122);
  assert.equal(h.machine, 40);            // ARM
  assert.equal(h.startup_vaddr, 0x10805fb4);
  assert.equal(h.image_paddr, 0x10800008);
  // the coincidental second signature must be rejected
  assert.ok(hs.some((x) => x.offset === 0x3c1414 && !validateStartupHeader(x).valid));
  const ifs = findImagefs(buf);
  assert.ok(ifs.some((f) => f.offset === 0x12c10));
});
