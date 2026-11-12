// Read-only firmware inspector for MIB2 (and MIB3) images/packages.
// Never modifies the input. Reports provenance hash, container format, embedded
// filesystems, and an ELF instruction-set census (MIB2 Standard expects ARMv7-A 32-bit).
import { createHash } from 'node:crypto';
import { readFileSync, statSync } from 'node:fs';

export function sha256(buf) {
  return createHash('sha256').update(buf).digest('hex');
}

export function detectFormat(buf) {
  if (buf.length >= 6 && buf[0] === 0x37 && buf[1] === 0x7a && buf[2] === 0xbc && buf[3] === 0xaf && buf[4] === 0x27 && buf[5] === 0x1c) return '7z';
  if (buf.length >= 4 && buf[0] === 0x50 && buf[1] === 0x4b && (buf[2] === 0x03 || buf[2] === 0x05) && buf[3] === 0x04) return 'zip';
  if (buf.length >= 4 && buf[0] === 0x1f && buf[1] === 0x8b) return 'gzip';
  if (buf.length >= 8 && buf.toString('ascii', 0, 8) === 'ANDROID!') return 'android-boot';
  if (buf.length >= 4 && buf.readUInt32LE(0) === 0xed26ff3a) return 'android-sparse';
  if (buf.length >= 4 && buf[0] === 0x7f && buf[1] === 0x45 && buf[2] === 0x4c && buf[3] === 0x46) return 'elf';
  if (buf.length >= 0x8006 && buf.toString('ascii', 0x8001, 0x8006) === 'CD001') return 'iso9660';
  if (buf.length >= 1080 && buf.readUInt16LE(1024 + 56) === 0xef53) return 'ext4';
  const head = buf.toString('ascii', 0, Math.min(buf.length, 512));
  if (/metainfo/i.test(head)) return 'mib-metainfo-text';
  return 'unknown';
}

const ELF_MACHINE = { 0x03: 'x86', 0x3e: 'x86-64', 0x28: 'ARM (ARMv7-A 32-bit)', 0xb7: 'AArch64', 0x08: 'MIPS', 0x14: 'PowerPC', 0xf3: 'RISC-V' };

export function parseElf(buf) {
  if (detectFormat(buf) !== 'elf') return null;
  const cls = buf[4] === 2 ? 64 : 32;
  const le = buf[5] === 1;
  const machine = le ? buf.readUInt16LE(18) : buf.readUInt16BE(18);
  return { class: cls, endian: le ? 'little' : 'big', machine, isa: ELF_MACHINE[machine] || `unknown(0x${machine.toString(16)})` };
}

export function listZip(buf) {
  if (detectFormat(buf) !== 'zip') return null;
  let eocd = -1;
  for (let i = buf.length - 22; i >= 0 && i >= buf.length - 22 - 65536; i--) {
    if (buf.readUInt32LE(i) === 0x06054b50) { eocd = i; break; }
  }
  if (eocd < 0) return { entries: [], note: 'EOCD not found' };
  const count = buf.readUInt16LE(eocd + 10);
  let off = buf.readUInt32LE(eocd + 16);
  const entries = [];
  for (let n = 0; n < count && off + 46 <= buf.length; n++) {
    if (buf.readUInt32LE(off) !== 0x02014b50) break;
    const compSize = buf.readUInt32LE(off + 20);
    const uncompSize = buf.readUInt32LE(off + 24);
    const nameLen = buf.readUInt16LE(off + 28);
    const extraLen = buf.readUInt16LE(off + 30);
    const commentLen = buf.readUInt16LE(off + 32);
    const name = buf.toString('utf8', off + 46, off + 46 + nameLen);
    entries.push({ name, compSize, uncompSize });
    off += 46 + nameLen + extraLen + commentLen;
  }
  return { entries };
}

export function inspect(path) {
  const buf = readFileSync(path);
  const st = statSync(path);
  const format = detectFormat(buf);
  const report = { path, size: st.size, sha256: sha256(buf), format };
  if (format === 'elf') report.elf = parseElf(buf);
  if (format === 'zip') report.zip = listZip(buf);
  if (format === 'ext4') report.ext4 = { superblockMagic: '0xEF53', blockSize: 1024 << buf.readUInt32LE(1024 + 24) };
  return report;
}

if (process.argv[1] && process.argv[1].endsWith('inspect-firmware.js')) {
  const path = process.argv[2];
  if (!path) { console.error('usage: node tools/inspect-firmware.js <image-or-package>'); process.exit(2); }
  console.log(JSON.stringify(inspect(path), null, 2));
}
