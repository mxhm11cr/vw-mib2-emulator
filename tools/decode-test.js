// Objectively test byte-order transformations of the QNX startup code to find which
// yields coherent ARM instructions. Read-only. Scores ARM validity per transformation.
import { readFileSync } from 'node:fs';
const b = readFileSync(process.argv[2]);
const START = 0x222; // image_paddr + header_size in the file (standard _start region)

function words(off, n, xform) {
  const out = [];
  for (let i = 0; i < n; i++) {
    const o = off + i * 4;
    let v = b.readUInt32LE(o);
    if (xform === 'hswap') v = ((v & 0xffff) << 16) | (v >>> 16);
    else if (xform === 'bswap') v = ((v & 0xff) << 24) | ((v & 0xff00) << 8) | ((v >>> 8) & 0xff00) | (v >>> 24);
    out.push(v >>> 0);
  }
  return out;
}
function wordsShift(off, n, shift) {
  const out = [];
  for (let i = 0; i < n; i++) out.push(b.readUInt32LE(off + shift + i * 4));
  return out;
}
// Rough ARM validity: reject obvious data; accept common instruction encodings.
function valid(w) {
  const cond = w >>> 28;
  const op = (w >>> 24) & 0xf;
  if (cond === 0xf) return (w >>> 24) === 0xf5 || (w >>> 24) === 0xf7 || (w >>> 20) === 0xfe; // unconditional rare
  // data-processing immediate / register, LDR/STR, B/BL, MOVW/MOVT, MRC/MCR, STM/LDM
  if (op === 0x0 || op === 0x1 || op === 0x2 || op === 0x3) return true; // DP
  if (op === 0x4 || op === 0x5 || op === 0x6 || op === 0x7) return true; // LDR/STR
  if (op === 0x8 || op === 0x9) return true; // STM/LDM
  if (op === 0xa || op === 0xb) return true; // B/BL
  if (op === 0xe) return true; // coproc / DP
  if (op === 0x3 && ((w >>> 20) & 0x1f) === 0x10) return true; // MOVW/MOVT (0xe30/0xe34)
  return false;
}
function score(ws) { return ws.filter(valid).length; }
function show(label, ws) {
  console.log(`\n${label}: valid ${score(ws)}/${ws.length}`);
  console.log('  ' + ws.slice(0, 12).map((w) => w.toString(16).padStart(8, '0')).join(' '));
}
const N = 32;
show('identity LE', words(START, N, 'id'));
show('halfword-swap', words(START, N, 'hswap'));
show('byte-swap (BE)', words(START, N, 'bswap'));
show('shift +2 bytes', wordsShift(START, N, 2));
show('shift +1 byte', wordsShift(START, N, 1));
show('shift +3 bytes', wordsShift(START, N, 3));
