// Locate the QNX IFS entry/structure inside a TechniSat MIB2 boot image (read-only).
import { readFileSync } from 'node:fs';
const path = process.argv[2];
const buf = readFileSync(path);
console.log('file', path, 'size', buf.length);
console.log('first 16 LE words:');
for (let i = 0; i < 16; i++) console.log(`  +0x${(i * 4).toString(16).padStart(3, '0')}: 0x${buf.readUInt32LE(i * 4).toString(16).padStart(8, '0')}`);

function findOffsets(needle, limit = buf.length) {
  const n = Buffer.from(needle, 'ascii');
  const out = [];
  let i = 0;
  while (out.length < 8) {
    const j = buf.indexOf(n, i);
    if (j < 0 || j >= limit) break;
    out.push(j);
    i = j + 1;
  }
  return out;
}
for (const s of ['procnto', 'startup', 'QNX', 'imagefs', 'valid IFS', '.text', 'shdr']) {
  console.log(`string "${s}" offsets:`, findOffsets(s).map((o) => '0x' + o.toString(16)).join(', ') || 'none');
}

// Scan for ARM B (0xEA) and BL (0xEB) opcodes in the first 128 KiB (candidate IFS entry).
console.log('first ARM B/BL opcodes (byte[3]==0xEA/0xEB, aligned):');
let shown = 0;
for (let o = 0; o < Math.min(buf.length, 0x20000) && shown < 12; o += 4) {
  const b3 = buf[o + 3];
  if (b3 === 0xea || b3 === 0xeb) {
    console.log(`  +0x${o.toString(16)}: 0x${buf.readUInt32LE(o).toString(16).padStart(8, '0')} (${b3 === 0xea ? 'B' : 'BL'})`);
    shown++;
  }
}
// Dump a few candidate round offsets where a plaintext IFS might begin.
for (const off of [0x200, 0x400, 0x1000, 0x1400, 0x2000, 0x4000]) {
  if (off + 16 <= buf.length) {
    console.log(`bytes @0x${off.toString(16)}:`, buf.subarray(off, off + 16).toString('hex'));
  }
}
