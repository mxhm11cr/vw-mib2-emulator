// J9 2.3 ROM -> standard JVM .class converter (Track A A2).
// Format empirically validated against firmware rom.classes (see rom-probe*.js):
//   directory: (selfRelName, selfRelClass) pairs @0x30
//   J9ROMClass: +0 romSize, +8 className, +12 superclass, +16 modifiers,
//               +20 numInterfaces, +28 numMethods, +36 numFields
//   methodsOffset = raw[8] + 32  (validated 1415/1415)
//   J9ROMMethod: 20-byte header {name, sig, modifiers, m12, m16} + inline bytecode
//     m12 = maxStack(u16) | bytecodeWords(u16)<<16 ; bytecode is LE-operands, padded to 4B
//   CP @+112 (class-relative), 8-byte items:
//     Class(selfRelName, 2)  String(selfRelName, 1)
//     Ref(classIdx1based, selfRelNAT)  NameAndType(selfRelName, selfRelDesc)
//   J9 bytecode: standard opcodes but ALL multi-byte operands LITTLE-ENDIAN -> convert to BE.
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { join } from 'node:path';
import { parseRomImage, utf8At, selfRel } from './rom-image.js';

const u32 = (b, o) => b.readUInt32LE(o);
const i32 = (b, o) => b.readInt32LE(o);
const printable = (s) => !!s && s.length > 0 && /^[\x20-\x7e]*$/.test(s);
// Reject bytecode/garbage masquerading as text (control chars), but allow unicode (0x80+).
const textOk = (s) => !!s && s.length > 0 && !/[\x00-\x08\x0b\x0c\x0e-\x1f]/.test(s);

export function parseCP(b, classOff, cpStartRel, cpEndRel, fieldStart = -1, fieldEnd = -1) {
  const items = [];
  const start = classOff + cpStartRel, end = classOff + cpEndRel;
  const fStart = fieldStart >= 0 ? classOff + fieldStart : -1;
  const fEnd = fieldEnd >= 0 ? classOff + fieldEnd : -1;
  for (let off = start; off + 8 <= end; off += 8) {
    const s1 = u32(b, off), s2 = u32(b, off + 4);
    const name1 = utf8At(b, selfRel(b, off));
    const desc2 = utf8At(b, selfRel(b, off + 4));
    // Field-table slots are NOT part of the J9 CP index space (bytecode CP indices skip
    // them). Exclude them entirely so items-array position == J9 CP index.
    if (fStart >= 0 && off >= fStart && off < fEnd) continue;
    if (s2 === 2 && printable(name1)) items.push({ shape: 'Class', name: name1, off });
    else if (s2 === 1) {
      // String constant: resolve name allowing length 0 (empty "" is a valid constant).
      const tgt = selfRel(b, off);
      let nm = null;
      if (tgt >= 0 && tgt + 2 <= b.length) {
        const len = b.readUInt16LE(tgt);
        if (len <= 4096 && tgt + 2 + len <= b.length) nm = b.toString('latin1', tgt + 2, tgt + 2 + len);
      }
      if (nm !== null && !/[\x00-\x08\x0b\x0c\x0e-\x1f]/.test(nm)) items.push({ shape: 'String', name: nm, off });
      else items.push({ shape: 'Unknown', s1, s2, off });
    }
    else if (s1 > 0 && s1 < 4096) {
      // Ref: slot1 = 1-based class CP index (small positive); slot2 = self-rel NameAndType.
      // Validate the target: embedded field-table modifier words can masquerade as Refs
      // whose natOff resolves to garbage (non-ASCII) -> would corrupt the standard CP.
      const natOff = off + 4 + i32(b, off + 4);
      const natName = utf8At(b, selfRel(b, natOff));
      const natDesc = utf8At(b, selfRel(b, natOff + 4));
      if (printable(natName) && printable(natDesc) && /^[A-Za-z_$<(]/.test(natName)) items.push({ shape: 'Ref', classIndex: s1, natOff, natName, natDesc, off });
      else items.push({ shape: 'Unknown', s1, s2, off });
    } else if (printable(name1) && printable(desc2)) items.push({ shape: 'NameAndType', name: name1, desc: desc2, off });
    else items.push({ shape: 'Unknown', s1, s2, off });
  }
  return items;
}

class CPBuilder {
  constructor() { this.entries = [null]; this.map = new Map(); }
  add(tag, key) {
    if (key && this.map.has(key)) return this.map.get(key);
    const idx = this.entries.length;
    this.entries.push({ tag });
    if (key) this.map.set(key, idx);
    return idx;
  }
  utf8(s) { const i = this.add(1, 'u:' + s); this.entries[i].bytes = Buffer.from(s, 'latin1'); return i; }
  clazz(name) { const i = this.add(7, 'c:' + name); this.entries[i].nameIdx = this.utf8(name); return i; }
  string(s) { const i = this.add(8, 's:' + s); this.entries[i].utfIdx = this.utf8(s); return i; }
  nat(name, desc) { const i = this.add(12, 'nt:' + name + ':' + desc); this.entries[i].nameIdx = this.utf8(name); this.entries[i].descIdx = this.utf8(desc); return i; }
  ref(tag, classIdx, natIdx) { const i = this.add(tag, `r:${tag}:${classIdx}:${natIdx}`); this.entries[i].classIdx = classIdx; this.entries[i].natIdx = natIdx; return i; }
  integer(v) { const i = this.add(3, 'i:' + v); this.entries[i].value = v | 0; return i; }
  float(bits) { const i = this.add(4, 'f:' + bits); this.entries[i].bits = bits >>> 0; return i; }
  long(lo, hi) { const k = 'l:' + lo + ':' + hi; if (this.map.has(k)) return this.map.get(k); const i = this.add(5, k); this.entries[i].lo = lo >>> 0; this.entries[i].hi = hi >>> 0; this.entries.push(null); return i; } // 2 CP slots (only for new entries)
  double(lo, hi) { const k = 'd:' + lo + ':' + hi; if (this.map.has(k)) return this.map.get(k); const i = this.add(6, k); this.entries[i].lo = lo >>> 0; this.entries[i].hi = hi >>> 0; this.entries.push(null); return i; }
  serialize() {
    const out = [];
    const u2 = (v) => { const x = Buffer.alloc(2); x.writeUInt16BE(v); return x; };
    const u4 = (v) => { const x = Buffer.alloc(4); x.writeUInt32BE(v >>> 0); return x; };
    for (let i = 1; i < this.entries.length; i++) {
      const e = this.entries[i];
      if (!e) continue; // second slot of Long/Double
      if (e.tag === 1) out.push(Buffer.from([1]), u2(e.bytes.length), e.bytes);
      else if (e.tag === 3) out.push(Buffer.from([3]), u4(e.value));
      else if (e.tag === 4) out.push(Buffer.from([4]), u4(e.bits));
      else if (e.tag === 5 || e.tag === 6) out.push(Buffer.from([e.tag]), u4(e.hi), u4(e.lo));
      else if (e.tag === 7) out.push(Buffer.from([7]), u2(e.nameIdx));
      else if (e.tag === 8) out.push(Buffer.from([8]), u2(e.utfIdx));
      else if (e.tag === 12) out.push(Buffer.from([12]), u2(e.nameIdx), u2(e.descIdx));
      else if (e.tag === 9 || e.tag === 10 || e.tag === 11) out.push(Buffer.from([e.tag]), u2(e.classIdx), u2(e.natIdx));
      else throw new Error('unknown tag ' + e.tag);
    }
    return { count: this.entries.length, bytes: Buffer.concat(out) };
  }
}

export function buildStandardCP(items) {
  const cp = new CPBuilder();
  const j9ToStd = [];
  for (let i = 0; i < items.length; i++) {
    const it = items[i];
    if (it.shape === 'Class') j9ToStd[i + 1] = cp.clazz(it.name);
    else if (it.shape === 'String') j9ToStd[i + 1] = cp.string(it.name);
    // NameAndType items from the grid walk are NOT added: the 12-byte field table shifts
    // the NAT section off the 8-byte CP grid, so grid-walked NATs are misaligned (name/desc
    // swapped). Correct NATs are created in pass 2 from each Ref's exact resolved natOff.
    else if (it.shape === 'Constant') {
      if (it.type === 'int') j9ToStd[i + 1] = cp.integer(it.value);
      else if (it.type === 'float') j9ToStd[i + 1] = cp.float(it.bits);
      else if (it.type === 'long') j9ToStd[i + 1] = cp.long(it.lo, it.hi);
      else if (it.type === 'double') j9ToStd[i + 1] = cp.double(it.lo, it.hi);
    } else j9ToStd[i + 1] = null;
  }
  for (let i = 0; i < items.length; i++) {
    const it = items[i];
    if (it.shape !== 'Ref') continue;
    const classStd = j9ToStd[it.classIndex];
    if (!classStd || !it.natName || !it.natDesc) continue;
    // Build the NameAndType directly from the Ref's resolved target. The NAT section
    // is NOT on the 8-byte CP grid (the 12-byte field table shifts its alignment by
    // numFields*12 mod 8), so grid-index lookup of natOff fails; use the parsed name/desc.
    const natStd = cp.nat(it.natName, it.natDesc);
    const tag = it.natDesc.startsWith('(') ? 10 : 9;
    j9ToStd[i + 1] = cp.ref(tag, classStd, natStd);
  }
  const indexMap = new Map();
  for (let i = 1; i <= items.length; i++) if (j9ToStd[i]) indexMap.set(i, j9ToStd[i]);
  return { cp, indexMap };
}

// ---- bytecode: LE operands -> BE, CP index remap, compute maxLocals ----
const CP16 = new Set([0x13, 0x14, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xbb, 0xbd, 0xc0, 0xc1, 0xc5, 0xf9]);
const BRANCH = new Set([0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xc6, 0xc7]);
const LOAD = { 0x15: 1, 0x16: 2, 0x17: 1, 0x18: 2, 0x19: 1, 0x36: 1, 0x37: 2, 0x38: 1, 0x39: 2, 0x3a: 1 };
const LOAD_N = {}; // opcode -> [baseIndex, size]
for (let k = 0; k < 4; k++) { LOAD_N[0x1a + k] = [k, 1]; LOAD_N[0x1e + k] = [k, 2]; LOAD_N[0x22 + k] = [k, 1]; LOAD_N[0x26 + k] = [k, 2]; LOAD_N[0x2a + k] = [k, 1]; LOAD_N[0x3b + k] = [k, 1]; LOAD_N[0x3f + k] = [k, 2]; LOAD_N[0x43 + k] = [k, 1]; LOAD_N[0x47 + k] = [k, 2]; LOAD_N[0x4b + k] = [k, 1]; }

// J9 renumbers returns: 0xAC=return0(void), 0xAD=return1(1-word), 0xAE=return2(2-word).
// Map to standard JVM return opcodes by the method's return type (empirically verified
// across Void/Byte/Boolean/Object — see rom-probe11.js).
function mapReturn(op, ret) {
  if (op === 0xac) return 0xb1; // return0 -> return (void)
  if (op === 0xad) { // return1 (1-word)
    if (ret === 'F') return 0xae; // freturn
    if (ret === 'L' || ret === '[') return 0xb0; // areturn
    return 0xac; // ireturn (I/B/S/Z/C)
  }
  if (op === 0xae) return ret === 'D' ? 0xaf : 0xad; // return2: dreturn / lreturn
  if (op === 0xaf) { // returnFromJava: generic return, type-dispatched
    if (ret === 'V') return 0xb1;
    if (ret === 'J') return 0xad;
    if (ret === 'D') return 0xaf;
    if (ret === 'F') return 0xae;
    if (ret === 'L' || ret === '[') return 0xb0;
    return 0xac;
  }
  return op;
}
// J9 private single-byte opcodes verified against standard bytecode (rom-probe / javap):
//   0xD7 = aload_0 variant (Throwable.initCause aligns 1:1 with standard).
const J9_PRIVATE_1BYTE = { 0xd7: 0x2a };

// Operand lengths (excluding opcode) for opcodes not specially handled in instrLen.
const OP_LEN = (() => {
  const t = new Array(256).fill(0);
  t[0x10] = 1; t[0x11] = 2; t[0x84] = 2; t[0xa9] = 1; t[0xbc] = 1;
  for (const o of [0x15, 0x16, 0x17, 0x18, 0x19, 0x36, 0x37, 0x38, 0x39, 0x3a]) t[o] = 1;
  return t;
})();

export function convertBytecode(bc, indexMap, retType, refDesc) {
  const out = Buffer.from(bc);
  let i = 0, maxLocals = 0;
  const sw16 = (p) => { const v = out.readUInt16LE(p); out.writeUInt16BE(v, p); return v; };
  const sw32 = (p) => { const v = out.readUInt32LE(p); out.writeUInt32BE(v, p); return v; };
  const remap = (p, wide) => {
    const idx = wide ? out.readUInt16LE(p) : out[p];
    const m = indexMap.get(idx);
    if (m === undefined) throw new Error(`CP index ${idx} (op@${i}) not mapped`);
    if (wide) out.writeUInt16BE(m, p); else out[p] = m;
  };
  const local = (idx, size) => { maxLocals = Math.max(maxLocals, idx + size); };
  while (i < out.length) {
    const op = out[i];
    if (op === 0x00 && i > 0 && (out[i - 1] === 0xac || out[i - 1] === 0xb1)) break;
    if (op === 0xc4) {
      const op2 = out[i + 1];
      if (op2 === 0x84) { const idx = sw16(i + 2); sw16(i + 4); local(idx, 1); i += 6; }
      else { const idx = sw16(i + 2); if (LOAD[op2]) local(idx, LOAD[op2]); i += 4; }
      continue;
    }
    if (op === 0xaa || op === 0xab) {
      const pad = (4 - ((i + 1) % 4)) % 4;
      const base = i + 1 + pad;
      sw32(base);
      if (op === 0xaa) {
        const lo = out.readInt32LE(base + 4), hi = out.readInt32LE(base + 8);
        sw32(base + 4); sw32(base + 8);
        for (let k = 0; k <= hi - lo; k++) sw32(base + 12 + k * 4);
        i = base + 12 + (hi - lo + 1) * 4;
      } else {
        const n = out.readInt32LE(base + 4); sw32(base + 4);
        for (let k = 0; k < n; k++) { sw32(base + 8 + k * 8); sw32(base + 12 + k * 8); }
        i = base + 8 + n * 8;
      }
      continue;
    }
    if (op === 0xac || op === 0xad || op === 0xae || op === 0xaf) { out[i] = mapReturn(op, retType); i += 1; continue; }
    // 0xB0 (areturn) and 0xB1 (return) are UNCHANGED standard opcodes in J9.
    if (J9_PRIVATE_1BYTE[op] !== undefined) { out[i] = J9_PRIVATE_1BYTE[op]; i += 1; continue; }
    if (op === 0xe7) { // J9 invokeinterfacequick: e7 00 b9 <idx16 LE> -> b9 <idx16 BE> <count> <0>
      const idx = out.readUInt16LE(i + 3);
      const m = indexMap.get(idx);
      if (m === undefined) throw new Error(`CP index ${idx} (invokeinterface@${i}) not mapped`);
      const desc = refDesc ? refDesc.get(idx) : null;
      out[i] = 0xb9; out.writeUInt16BE(m, i + 1); out[i + 3] = desc ? paramSlots(desc, false) : 1; out[i + 4] = 0;
      i += 5; continue;
    }
    if (op >= 0xca && op <= 0xff && !CP16.has(op)) throw new Error(`J9 private opcode 0x${op.toString(16)} @${i} needs handling`);
    if (op === 0x12) { remap(i + 1, false); i += 2; continue; } // ldc
    if (op === 0xb9 || op === 0xba) { remap(i + 1, true); i += 5; continue; } // invokeinterface/invokedynamic
    if (CP16.has(op)) { if (op === 0xf9) out[i] = 0x14; remap(i + 1, true); i += (op === 0xc5 ? 4 : 3); continue; }
    if (op === 0xbc) { i += 2; continue; } // newarray (atype, not CP)
    if (BRANCH.has(op)) { sw16(i + 1); i += 3; continue; }
    if (op === 0x11) { sw16(i + 1); i += 3; continue; } // sipush
    if (op === 0x84) { local(out[i + 1], 1); i += 3; continue; } // iinc
    if (LOAD[op]) { local(out[i + 1], LOAD[op]); i += 2; continue; }
    if (LOAD_N[op]) { local(LOAD_N[op][0], LOAD_N[op][1]); i += 1; continue; }
    if (op === 0x10) { i += 2; continue; } // bipush
    if (op === 0xc8 || op === 0xc9) { sw32(i + 1); i += 5; continue; } // goto_w/jsr_w
    // single-byte ops (incl. all _0.._3 arithmetic, stack, return, etc.)
    i += 1;
  }
  return { bytecode: out.subarray(0, i), maxLocals };
}

// Instruction byte length for tolerant walking (used by const-reference scanning).
function instrLen(bc, p) {
  const op = bc[p];
  if (op === 0xc4) return bc[p + 1] === 0x84 ? 6 : 4;
  if (op === 0xaa || op === 0xab) {
    const pad = (4 - ((p + 1) % 4)) % 4;
    const def = p + 1 + pad;
    if (def + 8 > bc.length) return bc.length - p;
    if (op === 0xaa) { const lo = bc.readInt32BE(def + 4), hi = bc.readInt32BE(def + 8); return def + 12 + (hi - lo + 1) * 4 - p; }
    return def + 8 + bc.readInt32BE(def + 4) * 8 - p;
  }
  if (op === 0xb9 || op === 0xba) return 5;
  if (op === 0xc5) return 4;
  if (op === 0x12) return 2;
  if (CP16.has(op)) return 3;
  if (BRANCH.has(op)) return 3;
  if (op === 0xc8 || op === 0xc9) return 5;
  if (op === 0xd7) return 1;
  if (op === 0xe7) return 5; // J9 invokeinterfacequick
  return 1 + (OP_LEN[op] || 0);
}

// Net stack word change for opcodes whose effect is fixed (field/method access handled
// separately via descriptors). Used to track stack depth so each ldc constant can be
// matched to the consumer (field type or method parameter type) that determines its type.
function stackDelta(op, bc, p) {
  if (op === 0x01) return 1;
  if (op >= 0x02 && op <= 0x08) return 1;
  if (op === 0x09 || op === 0x0a) return 2;
  if (op >= 0x0b && op <= 0x0d) return 1;
  if (op === 0x0e || op === 0x0f) return 2;
  if (op === 0x10 || op === 0x11) return 1;
  if (op === 0x15 || op === 0x17 || op === 0x19) return 1;
  if (op === 0x16 || op === 0x18) return 2;
  if (op >= 0x1a && op <= 0x2d) return (op >= 0x1e && op <= 0x21) || (op >= 0x26 && op <= 0x29) ? 2 : 1;
  if (op >= 0x2e && op <= 0x35) return (op === 0x2f || op === 0x31) ? 0 : -1;
  if (op >= 0x36 && op <= 0x3a) return (op === 0x37 || op === 0x39) ? -2 : -1;
  if (op >= 0x3b && op <= 0x4e) return (op >= 0x3f && op <= 0x42) || (op >= 0x47 && op <= 0x4a) ? -2 : -1;
  if (op >= 0x4f && op <= 0x56) return (op === 0x50 || op === 0x52) ? -4 : -3;
  if (op === 0x57) return -1;
  if (op === 0x58) return -2;
  if (op >= 0x59 && op <= 0x5b) return 1;
  if (op >= 0x5c && op <= 0x5e) return 2;
  if (op === 0x5f) return 0;
  if (op >= 0x60 && op <= 0x73) { const k = (op - 0x60) % 4; return (k === 1 || k === 3) ? -2 : -1; }
  if (op >= 0x74 && op <= 0x77) return 0;
  if (op >= 0x78 && op <= 0x83) return -1;
  if (op === 0x84) return 0;
  if (op >= 0x85 && op <= 0x93) { const push2 = [0x85, 0x87, 0x8c, 0x8d, 0x8f].includes(op); const pop2 = [0x88, 0x89, 0x8a, 0x8e, 0x8f, 0x90].includes(op); return (push2 ? 2 : 1) - (pop2 ? 2 : 1); }
  if (op === 0x94 || op === 0x97 || op === 0x98) return -3;
  if (op === 0x95 || op === 0x96) return -1;
  if (op >= 0x99 && op <= 0x9e) return -1;
  if (op >= 0x9f && op <= 0xa6) return -2;
  if (op === 0xa7) return 0;
  if (op === 0xa8) return 1;
  if (op === 0xaa || op === 0xab) return -1;
  if (op >= 0xac && op <= 0xb1) return (op === 0xad || op === 0xaf) ? -2 : -1;
  if (op === 0xbb) return 1;
  if (op === 0xbc || op === 0xbd || op === 0xbe || op === 0xc0 || op === 0xc1) return 0;
  if (op === 0xbf || op === 0xc2 || op === 0xc3 || op === 0xc6 || op === 0xc7) return -1;
  if (op === 0xc5) return 1 - bc[p + 3];
  if (op === 0xc8) return 0;
  if (op === 0xc9) return 1;
  if (op === 0xd7) return 1;
  if (op === 0xc4) { const o2 = bc[p + 1]; if (o2 === 0x84) return 0; if (o2 >= 0x36 && o2 <= 0x3a) return (o2 === 0x37 || o2 === 0x39) ? -2 : -1; return (o2 === 0x16 || o2 === 0x18) ? 2 : 1; }
  return 0;
}
const typeWords = (t) => (t === 'J' || t === 'D') ? 2 : (t === 'V' ? 0 : 1);
const typeKind = (t) => t === 'F' ? 'float' : t === 'J' ? 'long' : t === 'D' ? 'double' : 'IBSZC'.includes(t) ? 'int' : null;
function paramTypes(desc) {
  const args = desc.slice(1, desc.indexOf(')'));
  const out = [];
  for (let i = 0; i < args.length; i++) {
    const ch = args[i];
    if (ch === 'L') { const e = args.indexOf(';', i); out.push(args.slice(i, e + 1)); i = e; }
    else if (ch === '[') { let j = i; while (args[j + 1] === '[') j++; if (args[j + 1] === 'L') { const e = args.indexOf(';', j); out.push(args.slice(i, e + 1)); i = e; } else { out.push(args.slice(i, j + 2)); i = j + 1; } }
    else out.push(ch);
  }
  return out;
}

// Resolve each ldc/ldc2_w constant's type by tracking stack depth and matching it to the
// consumer that pops it: putfield/putstatic field descriptor, or invoke* parameter type.
// J9 encodes Integer/Float (and Long/Double) identically, so the consumer type is authoritative.
function scanConstKinds(b, base, mStarts, romSize, items) {
  const kinds = new Map();
  const refDesc = new Map();
  items.forEach((it, i) => { if (it.shape === 'Ref') refDesc.set(i + 1, it.natDesc); });
  const resolve = (pending, valueDepth, words, kind) => {
    if (!kind) return;
    for (const e of pending) if (e.depth === valueDepth && e.words === words) { kinds.set(e.idx, kind); return; }
  };
  for (let mi = 0; mi < mStarts.length; mi++) {
    const words = (u32(b, base + mStarts[mi] + 12) >>> 16) & 0xffff;
    if (!words) continue;
    const bcStart = base + mStarts[mi] + 20;
    const bcEnd = mi + 1 < mStarts.length ? base + mStarts[mi + 1] : base + romSize;
    const bc = b.subarray(bcStart, Math.min(bcStart + words * 4, bcEnd));
    let p = 0, depth = 0, pending = [];
    const prune = () => { pending = pending.filter((e) => e.depth + e.words <= depth); };
    while (p < bc.length) {
      const op = bc[p];
      const len = Math.max(1, instrLen(bc, p));
      if (op === 0x12 || op === 0x13) {
        const idx = op === 0x13 ? bc.readUInt16LE(p + 1) : bc[p + 1];
        if (!kinds.has(idx)) kinds.set(idx, 'ldc');
        pending.push({ idx, depth, words: 1 });
        depth += 1; p += len; prune(); continue;
      }
      if (op === 0x14 || op === 0xf9) {
        const idx = bc.readUInt16LE(p + 1);
        if (!kinds.has(idx)) kinds.set(idx, 'long');
        pending.push({ idx, depth, words: 2 });
        depth += 2; p += len; prune(); continue;
      }
      if (op === 0xb2 || op === 0xb4) { // getstatic/getfield: push field type
        const desc = refDesc.get(bc.readUInt16LE(p + 1));
        depth += typeWords(desc) - (op === 0xb4 ? 1 : 0);
        p += len; prune(); continue;
      }
      if (op === 0xb3 || op === 0xb5) { // putstatic/putfield: pop value (+objectref)
        const desc = refDesc.get(bc.readUInt16LE(p + 1));
        const vw = typeWords(desc);
        resolve(pending, depth - vw, vw, typeKind(desc));
        depth -= vw + (op === 0xb5 ? 1 : 0);
        p += len; prune(); continue;
      }
      if (op === 0xb6 || op === 0xb7 || op === 0xb8 || op === 0xb9 || op === 0xe7) {
        const idx = op === 0xe7 ? bc.readUInt16LE(p + 3) : bc.readUInt16LE(p + 1);
        const desc = refDesc.get(idx);
        if (desc && desc.startsWith('(')) {
          const params = paramTypes(desc);
          const isStatic = op === 0xb8;
          const argWords = params.reduce((s, t) => s + typeWords(t), 0) + (isStatic ? 0 : 1);
          let d = depth - argWords + (isStatic ? 0 : 1);
          for (const t of params) { const w = typeWords(t); resolve(pending, d, w, typeKind(t)); d += w; }
          const ret = desc.slice(desc.indexOf(')') + 1);
          depth -= argWords; depth += typeWords(ret);
        }
        p += len; prune(); continue;
      }
      depth += stackDelta(op, bc, p);
      if (depth < 0) depth = 0;
      p += len; prune();
    }
  }
  return kinds;
}

function classifyConstants(items, kinds, className) {
  const mathClass = /java\/lang\/(Double|Float|Math|StrictMath)$/.test(className);
  for (let i = 0; i < items.length; i++) {
    const it = items[i];
    if (it.shape !== 'Unknown') continue;
    const kind = kinds.get(i + 1);
    if (!kind) continue;
    let type;
    if (kind === 'int') type = 'int';
    else if (kind === 'float') type = 'float';
    else if (kind === 'double') type = 'double';
    else if (kind === 'long') type = mathClass ? 'double' : 'long';
    else type = it.s2 === 0 ? 'int' : 'float'; // 'ldc' unresolved fallback (stack resolver handles most)
    if (type === 'int') items[i] = { shape: 'Constant', type: 'int', value: it.s1 | 0, off: it.off };
    else if (type === 'float') items[i] = { shape: 'Constant', type: 'float', bits: it.s1 >>> 0, off: it.off };
    else if (type === 'long') items[i] = { shape: 'Constant', type: 'long', lo: it.s1, hi: it.s2, off: it.off };
    else items[i] = { shape: 'Constant', type: 'double', lo: it.s1, hi: it.s2, off: it.off };
  }
}

export function readMethod(b, mOff) {
  const m12 = u32(b, mOff + 12);
  return {
    off: mOff,
    name: utf8At(b, selfRel(b, mOff)),
    sig: utf8At(b, selfRel(b, mOff + 4)),
    modifiers: u32(b, mOff + 8),
    maxStack: m12 & 0xffff,
    bcWords: (m12 >>> 16) & 0xffff,
    m16: u32(b, mOff + 16),
  };
}

function paramSlots(sig, isStatic) {
  let n = isStatic ? 0 : 1;
  const args = sig.slice(1, sig.indexOf(')'));
  for (let i = 0; i < args.length; i++) {
    const ch = args[i];
    if (ch === 'J' || ch === 'D') n += 2;
    else if (ch === 'L') { n += 1; i = args.indexOf(';', i); }
    else if (ch === '[') { while (args[i + 1] === '[') i++; if (args[i + 1] === 'L') i = args.indexOf(';', i); n += 1; }
    else n += 1;
  }
  return n;
}

export function writeClassFile({ className, superName, modifiers, interfaces, fields, methods, cp }) {
  const u2 = (v) => { const x = Buffer.alloc(2); x.writeUInt16BE(v); return x; };
  const u4 = (v) => { const x = Buffer.alloc(4); x.writeUInt32BE(v); return x; };
  // Phase 1: add ALL CP entries first (serialize must see the complete pool).
  const thisIdx = cp.clazz(className);
  const superIdx = superName ? cp.clazz(superName) : 0;
  const ifaceIdx = interfaces.map((n) => cp.clazz(n));
  const fd = fields.map((f) => ({ mods: f.modifiers & 0xffff, n: cp.utf8(f.name), d: cp.utf8(f.desc) }));
  const md = methods.map((m) => {
    const hasCode = m.bytecode && m.bytecode.length > 0;
    return { mods: m.modifiers & 0xffff, n: cp.utf8(m.name), d: cp.utf8(m.sig), hasCode, codeN: hasCode ? cp.utf8('Code') : 0, m };
  });
  // Phase 2: serialize the now-complete pool.
  const ser = cp.serialize();
  // Phase 3: assemble.
  const parts = [u4(0xcafebabe), u2(0), u2(46), u2(ser.count), ser.bytes];
  parts.push(u2(modifiers & 0xffff), u2(thisIdx), u2(superIdx), u2(ifaceIdx.length));
  for (const i of ifaceIdx) parts.push(u2(i));
  parts.push(u2(fd.length));
  for (const f of fd) parts.push(u2(f.mods), u2(f.n), u2(f.d), u2(0));
  parts.push(u2(md.length));
  for (const x of md) {
    parts.push(u2(x.mods), u2(x.n), u2(x.d), u2(x.hasCode ? 1 : 0));
    if (x.hasCode) {
      const m = x.m;
      const code = Buffer.concat([u2(m.maxStack), u2(m.maxLocals), u4(m.bytecode.length), m.bytecode, u2(0), u2(0)]);
      parts.push(u2(x.codeN), u4(code.length), code);
    }
  }
  parts.push(u2(0));
  return Buffer.concat(parts);
}

// Convert one ROM class to a standard .class file. Returns { bytes, info, lines }.
// Parse the variable-length field table (offset raw[10]+40). Compile-time-constant fields
// carry extra ConstantValue words; scan to the next valid identifier to skip them.
// Returns the fields plus the region [start,end) to exclude from CP classification.
function parseFields(b, base, raw, methodsOffset) {
  const fields = [];
  const isFieldName = (s) => !!s && /^[A-Za-z_$][A-Za-z0-9_$]*$/.test(s);
  const start = raw[10] + 40;
  let fpos = base + start;
  const fEnd = base + methodsOffset;
  for (let i = 0; i < raw[9]; i++) {
    const name = utf8At(b, selfRel(b, fpos)), desc = utf8At(b, selfRel(b, fpos + 4));
    // Mask J9 modifier bits to JVM field access flags; strip 0x1000 (J9 synthetic marker,
    // represented as a Synthetic attribute in standard classes — the flag crashes the JVM
    // field layout builder for patched core classes like java.lang.Void).
    if (name && desc) {
      let mods = u32(b, fpos + 8) & 0xefff;
      if ((mods & 0x50) === 0x50) mods &= ~0x40; // final+volatile is illegal; J9 volatile bit is a flag
      fields.push({ name, desc, modifiers: mods });
    }
    fpos += 12;
    if (i < raw[9] - 1) while (fpos + 4 <= fEnd && !isFieldName(utf8At(b, selfRel(b, fpos)))) fpos += 4;
  }
  return { fields, start, end: fpos - base };
}

export function convertClass(b, target, img = parseRomImage(b)) {
  const lines = [];
  const c = img.classes.find((x) => x.name === target);
  if (!c) throw new Error('class not found: ' + target);
  const base = c.classOff, h = c.header, raw = h.raw;
  const methodsOffset = raw[8] + 32;
  const { fields, start: fStart, end: fEnd } = parseFields(b, base, raw, methodsOffset);
  const items = parseCP(b, base, 112, methodsOffset, fStart, fEnd);
  const mStarts = [];
  const isMethodName = (s) => !!s && /^[A-Za-z_$<][A-Za-z0-9_$<>]*$/.test(s);
  for (let f = methodsOffset; f + 20 <= h.romSize && mStarts.length < raw[7]; f += 4) {
    const n = utf8At(b, selfRel(b, base + f));
    const s = utf8At(b, selfRel(b, base + f + 4));
    if (isMethodName(n) && s && s.startsWith('(')) mStarts.push(f);
  }
  const kinds = scanConstKinds(b, base, mStarts, h.romSize, items);
  classifyConstants(items, kinds, target);
  const shapes = {};
  for (const it of items) shapes[it.shape] = (shapes[it.shape] || 0) + 1;
  lines.push(`${target}: romSize=${h.romSize} methods=${raw[7]} fields=${raw[9]} cp=${items.length} shapes=${JSON.stringify(shapes)}`);
  const { cp, indexMap } = buildStandardCP(items);
  const refDesc = new Map();
  items.forEach((it, i) => { if (it.shape === 'Ref') refDesc.set(i + 1, it.natDesc); });
  const methods = [];
  for (let i = 0; i < mStarts.length; i++) {
    const m = readMethod(b, base + mStarts[i]);
    const bcStart = base + mStarts[i] + 20;
    const bcEnd = i + 1 < mStarts.length ? base + mStarts[i + 1] : base + h.romSize;
    const isAbstract = (m.modifiers & 0x0400) !== 0, isNative = (m.modifiers & 0x0100) !== 0;
    let bytecode = Buffer.alloc(0), maxLocals = paramSlots(m.sig, (m.modifiers & 0x0008) !== 0);
    if (!isAbstract && !isNative && m.bcWords > 0) {
      const rawBc = b.subarray(bcStart, Math.min(bcStart + m.bcWords * 4, bcEnd));
      const conv = convertBytecode(rawBc, indexMap, m.sig.slice(m.sig.indexOf(')') + 1, m.sig.indexOf(')') + 2), refDesc);
      bytecode = conv.bytecode;
      maxLocals = Math.max(maxLocals, conv.maxLocals);
    }
    methods.push({ ...m, bytecode, maxLocals });
    lines.push(`  ${m.name}${m.sig} mods=0x${m.modifiers.toString(16)} stack=${m.maxStack} locals=${maxLocals} bc=${bytecode.length}B`);
  }
  const bytes = writeClassFile({ className: target, superName: h.superclassName, modifiers: raw[4], interfaces: [], fields, methods, cp });
  return { bytes, info: { className: target, romSize: h.romSize, methods: methods.length, fields: fields.length, cpItems: items.length, shapes }, lines };
}

if (process.argv[1] && process.argv[1].endsWith('rom-to-class.js')) {
  const b = readFileSync(process.argv[2] || 'firmware/analysis/rom/rom.classes');
  const target = process.argv[3] || 'java/lang/Void';
  const { bytes, lines } = convertClass(b, target);
  for (const l of lines) console.log(l);
  const outDir = 'firmware/analysis/converted';
  mkdirSync(join(outDir, target.split('/').slice(0, -1).join('/')), { recursive: true });
  const outPath = join(outDir, target + '.class');
  writeFileSync(outPath, bytes);
  console.log('written:', outPath, bytes.length, 'bytes');
}
