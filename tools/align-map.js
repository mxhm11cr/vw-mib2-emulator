// Map the per-region byte-ordering transformation of the TechniSat QNX image (read-only).
// For sampled windows, find the (shift, order) that maximizes ARM instruction validity,
// revealing whether the transformation is uniform, periodic, or section-based.
import { readFileSync } from 'node:fs';
const b = readFileSync(process.argv[2]);
const START = 0x228, END = 0x1E22A; // startup region (header_size .. startup_size)
function rd(off, order) {
  let v = b.readUInt32LE(off);
  if (order === 'h') v = ((v & 0xffff) << 16) | (v >>> 16);
  return v >>> 0;
}
function valid(v) {
  const c = v >>> 28, o = (v >>> 24) & 0xf;
  if (c === 0xf) return false;
  return o <= 0xb || o === 0xe;
}
function score(off, shift, order) {
  let s = 0;
  for (let i = 0; i < 32; i++) { const o = off + shift + i * 4; if (o + 4 > b.length) break; if (valid(rd(o, order))) s++; }
  return s;
}
console.log('off      best(shift,order) score  | id0 id2 h0 h2');
let prev = '';
for (let off = START; off < END; off += 0x800) {
  let best = { s: -1, sh: 0, or: 'i' };
  const cols = [];
  for (const [sh, or] of [[0, 'i'], [2, 'i'], [0, 'h'], [2, 'h']]) {
    const s = score(off, sh, or);
    cols.push(s);
    if (s > best.s) best = { s, sh, or };
  }
  const tag = `${best.sh}${best.or}`;
  const mark = tag !== prev ? '  <== change' : '';
  console.log(`0x${off.toString(16).padStart(5)}  shift=${best.sh} order=${best.or}  ${best.s}/32   | ${cols.join(' ')}${mark}`);
  prev = tag;
}
