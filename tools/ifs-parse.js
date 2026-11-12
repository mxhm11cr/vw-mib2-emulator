// Format-correct QNX IFS parser (read-only), per QNX sys/startup.h and sys/image.h.
// Locates the startup header by STARTUP_HDR_SIGNATURE (0x00FF7EEB), validates fields,
// and reports the authentic entry (startup_vaddr) and load address (image_paddr).
// References: vocho/openqnx sys/image.h; markotikvic/QNX-BBB sys/startup.h.
import { readFileSync } from 'node:fs';

export const STARTUP_HDR_SIGNATURE = 0x00ff7eeb;
export const IMAGE_SIGNATURE = 'imagefs';

export function parseStartupHeader(buf, off) {
  if (off + 64 > buf.length) return null;
  const h = {
    offset: off,
    signature: buf.readUInt32LE(off),
    version: buf.readUInt16LE(off + 4),
    flags1: buf[off + 6],
    flags2: buf[off + 7],
    header_size: buf.readUInt16LE(off + 8),
    machine: buf.readUInt16LE(off + 10),
    startup_vaddr: buf.readUInt32LE(off + 12),
    paddr_bias: buf.readUInt32LE(off + 16),
    image_paddr: buf.readUInt32LE(off + 20),
    ram_paddr: buf.readUInt32LE(off + 24),
    ram_size: buf.readUInt32LE(off + 28),
    startup_size: buf.readUInt32LE(off + 32),
    stored_size: buf.readUInt32LE(off + 36),
    imagefs_paddr: buf.readUInt32LE(off + 40),
    imagefs_size: buf.readUInt32LE(off + 44),
    preboot_size: buf.readUInt16LE(off + 48),
  };
  return h;
}

export function validateStartupHeader(h) {
  const checks = {
    signature: h.signature === STARTUP_HDR_SIGNATURE,
    version: h.version === 1,
    header_size: h.header_size === 256,
    machine_arm: h.machine === 40, // EM_ARM
    entry_in_image: h.startup_vaddr >= h.image_paddr && h.startup_vaddr < h.image_paddr + h.stored_size,
  };
  checks.valid = Object.values(checks).every(Boolean);
  return checks;
}

export function findStartupHeaders(buf) {
  const pat = Buffer.alloc(4); pat.writeUInt32LE(STARTUP_HDR_SIGNATURE);
  const out = []; let i = 0;
  while ((i = buf.indexOf(pat, i)) >= 0) {
    const h = parseStartupHeader(buf, i);
    if (h) out.push(h);
    i += 4;
  }
  return out;
}

export function findImagefs(buf) {
  const out = []; let i = 0;
  while ((i = buf.indexOf(IMAGE_SIGNATURE, i)) >= 0) {
    if (i + 16 <= buf.length) {
      out.push({
        offset: i,
        flags: buf[i + 7],
        image_size: buf.readUInt32LE(i + 8),
        hdr_dir_size: buf.readUInt32LE(i + 12),
        dir_offset: buf.readUInt32LE(i + 16),
      });
    }
    i += 4;
  }
  return out;
}

if (process.argv[1] && process.argv[1].endsWith('ifs-parse.js')) {
  const buf = readFileSync(process.argv[2]);
  console.log('file size', buf.length, '0x' + buf.length.toString(16));
  const hs = findStartupHeaders(buf);
  for (const h of hs) {
    console.log('\n=== startup_header @0x' + h.offset.toString(16) + ' ===');
    for (const [k, v] of Object.entries(h)) console.log('  ' + k + ': ' + (typeof v === 'number' ? '0x' + v.toString(16) : v));
    console.log('  validation:', JSON.stringify(validateStartupHeader(h)));
  }
  const ifs = findImagefs(buf);
  for (const f of ifs) {
    console.log('\n=== imagefs @0x' + f.offset.toString(16) + ' ===');
    for (const [k, v] of Object.entries(f)) console.log('  ' + k + ': ' + (typeof v === 'number' ? '0x' + v.toString(16) : v));
  }
}
