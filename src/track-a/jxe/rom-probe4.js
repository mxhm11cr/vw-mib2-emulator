// Probe 4: definitive directory parse (self-relative names) + J9ROMClass layout dump.
import { readFileSync } from 'node:fs';
const b = readFileSync(process.argv[2] || 'firmware/analysis/rom/rom.classes');
const u32 = (o) => b.readUInt32LE(o);
const i32 = (o) => b.readInt32LE(o);
const u16 = (o) => b.readUInt16LE(o);
const hex = (v) => '0x' + (v >>> 0).toString(16);
function utf8(o) {
  if (o < 0 || o + 2 > b.length) return null;
  const len = u16(o);
  if (len === 0 || len > 512 || o + 2 + len > b.length) return null;
  const s = b.toString('latin1', o + 2, o + 2 + len);
  return /^[\x20-\x7e]+$/.test(s) ? s : null;
}
const count = u32(0x0c);
const entries = [];
let bad = 0;
for (let i = 0; i < count; i++) {
  const eo = 0x30 + i * 8;
  const name = utf8(eo + i32(eo));
  const c = u32(eo + 4);
  if (!name) bad++;
  entries.push({ i, eo, name, c });
}
console.log(`directory: ${count} entries, unresolved names: ${bad}`);
console.log('\n=== first 20 entries (name | classOff | u32@+0 | u32@+4) ===');
for (const e of entries.slice(0, 20)) {
  console.log(`${String(e.i).padStart(3)} ${e.name || '???'} | ${hex(e.c)} | ${u32(e.c)} | ${hex(u32(e.c + 4))}`);
}

function dumpClass(e, label) {
  console.log(`\n=== ${label}: "${e.name}" @${hex(e.c)} ===`);
  for (let f = 0; f < 128; f += 4) {
    const off = e.c + f;
    if (off + 4 > b.length) break;
    const raw = u32(off), v = i32(off);
    const n = utf8(off + v);
    console.log(`+${String(f).padStart(3)} ${hex(raw).padEnd(10)} i32=${String(v).padStart(10)}${n ? '  -> "' + n + '"' : ''}`);
  }
}
dumpClass(entries[0], 'pair0');
const pso = entries.find((e) => e.name === 'tsd/common/log/PrintStreamLogOutput');
if (pso) dumpClass(pso, 'PrintStreamLogOutput');

// romSize plausibility across all entries
const sizes = entries.map((e) => u32(e.c));
const plausible = sizes.filter((s) => s >= 64 && s <= 200000).length;
console.log(`\nromSize@+0 plausible (64..200000): ${plausible}/${count}`);
console.log('size histogram sample:', sizes.slice(0, 30).join(','));
