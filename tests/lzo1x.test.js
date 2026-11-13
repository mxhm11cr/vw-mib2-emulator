import { test } from 'node:test';
import assert from 'node:assert';
import { readFileSync, existsSync } from 'node:fs';
import { decompressLzoBlocks } from '../tools/lzo1x.js';

const ifsPath = 'firmware/analysis/hmizr/tsd/tmp/hmi/tsd.mibstd2.hmi.ifs';
const haveIfs = existsSync(ifsPath);

(haveIfs ? test : test.skip)('QNX HMI IFS LZO decompression yields valid imagefs', () => {
  const buf = readFileSync(ifsPath);
  assert.equal(buf[6] & 0x0c, 0x8, 'startup flags1 indicates LZO');
  const startupSize = buf.readUInt32LE(32);
  const data = decompressLzoBlocks(buf, startupSize);
  assert.ok(data.length > 40000000, 'decompressed image > 40MB, got ' + data.length);
  assert.equal(data.toString('latin1', 0, 7), 'imagefs', 'imagefs signature at start');
  assert.equal(data.readUInt32LE(16), 0x5c, 'dir_offset recovered as 0x5c');
  assert.ok(data.includes(Buffer.from('J99J')), 'contains J9 ROM images');
});
