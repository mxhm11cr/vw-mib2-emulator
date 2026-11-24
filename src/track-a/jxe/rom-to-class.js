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
      if (nm !== null && !/\x00/.test(nm)) items.push({ shape: 'String', name: nm, off });
      else items.push({ shape: 'Unknown', s1, s2, off });
    }
    else if (s1 > 0 && s1 < 4096) {
      // Ref: slot1 = 1-based class CP index (small positive); slot2 = self-rel NameAndType.
      // Validate the target: embedded field-table modifier words can masquerade as Refs
      // whose natOff resolves to garbage (non-ASCII) -> would corrupt the standard CP.
      const natOff = off + 4 + i32(b, off + 4);
      if (natOff < 0 || natOff + 8 > b.length) { items.push({ shape: 'Unknown', s1, s2, off }); continue; }
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
  constructor() {
    this.entries = [null];
    this.map = new Map();
    this.utf8Map = new Map();
    this.utf8List = [];
  }
  add(tag, key) {
    if (key && this.map.has(key)) return this.map.get(key);
    const idx = this.entries.length;
    this.entries.push({ tag });
    if (key) this.map.set(key, idx);
    return idx;
  }
  getUtf8(s) {
    if (this.utf8Map.has(s)) return this.utf8Map.get(s);
    const ref = { s, idx: -1, valueOf() { return this.idx; } };
    this.utf8Map.set(s, ref);
    this.utf8List.push(ref);
    return ref;
  }
  utf8(s) { return this.getUtf8(s); }
  clazz(name) {
    const k = 'c:' + name;
    if (this.map.has(k)) return this.map.get(k);
    const uRef = this.getUtf8(name);
    const i = this.add(7, k);
    this.entries[i].uRef = uRef;
    return i;
  }
  string(s) {
    const k = 's:' + s;
    if (this.map.has(k)) return this.map.get(k);
    const uRef = this.getUtf8(s);
    const i = this.add(8, k);
    this.entries[i].uRef = uRef;
    return i;
  }
  nat(name, desc) {
    const k = 'nt:' + name + ':' + desc;
    if (this.map.has(k)) return this.map.get(k);
    const nameRef = this.getUtf8(name);
    const descRef = this.getUtf8(desc);
    const i = this.add(12, k);
    this.entries[i].nameRef = nameRef;
    this.entries[i].descRef = descRef;
    return i;
  }
  ref(tag, classIdx, natIdx) {
    const k = `r:${tag}:${classIdx}:${natIdx}`;
    if (this.map.has(k)) return this.map.get(k);
    const i = this.add(tag, k);
    this.entries[i].classIdx = classIdx;
    this.entries[i].natIdx = natIdx;
    return i;
  }
  integer(v) { const i = this.add(3, 'i:' + v); this.entries[i].value = v | 0; return i; }
  float(bits) { const i = this.add(4, 'f:' + bits); this.entries[i].bits = bits >>> 0; return i; }
  long(lo, hi) {
    const k = 'l:' + lo + ':' + hi;
    if (this.map.has(k)) return this.map.get(k);
    const i = this.add(5, k);
    this.entries[i].lo = lo >>> 0;
    this.entries[i].hi = hi >>> 0;
    this.entries.push(null);
    return i;
  }
  double(lo, hi) {
    const k = 'd:' + lo + ':' + hi;
    if (this.map.has(k)) return this.map.get(k);
    const i = this.add(6, k);
    this.entries[i].lo = lo >>> 0;
    this.entries[i].hi = hi >>> 0;
    this.entries.push(null);
    return i;
  }
  finalizeUtf8() {
    for (const u of this.utf8List) {
      if (u.idx === -1) {
        const i = this.entries.length;
        this.entries.push({ tag: 1, bytes: Buffer.from(u.s, 'latin1') });
        u.idx = i;
      }
    }
    for (let i = 1; i < this.entries.length; i++) {
      const e = this.entries[i];
      if (!e) continue;
      if (e.tag === 7) e.nameIdx = e.uRef.idx;
      else if (e.tag === 8) e.utfIdx = e.uRef.idx;
      else if (e.tag === 12) { e.nameIdx = e.nameRef.idx; e.descIdx = e.descRef.idx; }
    }
  }
  serialize() {
    this.finalizeUtf8();
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

export function buildStandardCP(items, ifaceRefIdx = null, ifaces = null, ldc8 = null) {
  const cp = new CPBuilder();
  const j9ToStd = [];
  // Pass 1: Allocate entries used by 0x12 (ldc) first so their standard CP indices are guaranteed <= 255.
  if (ldc8) {
    for (const j9Idx of ldc8) {
      const it = items[j9Idx - 1];
      if (!it) continue;
      if (it.shape === 'Class') j9ToStd[j9Idx] = cp.clazz(it.name);
      else if (it.shape === 'String') j9ToStd[j9Idx] = cp.string(it.name);
      else if (it.shape === 'Constant') {
        if (it.type === 'int') j9ToStd[j9Idx] = cp.integer(it.value);
        else if (it.type === 'float') j9ToStd[j9Idx] = cp.float(it.bits);
      }
    }
  }
  // Pass 2: Allocate remaining constants and classes
  for (let i = 0; i < items.length; i++) {
    const j9Idx = i + 1;
    if (j9ToStd[j9Idx]) continue;
    const it = items[i];
    if (it.shape === 'Class') j9ToStd[j9Idx] = cp.clazz(it.name);
    else if (it.shape === 'String') j9ToStd[j9Idx] = cp.string(it.name);
    // NameAndType items from the grid walk are NOT added: the 12-byte field table shifts
    // the NAT section off the 8-byte CP grid, so grid-walked NATs are misaligned (name/desc
    // swapped). Correct NATs are created in pass 2 from each Ref's exact resolved natOff.
    else if (it.shape === 'Constant') {
      if (it.type === 'int') j9ToStd[j9Idx] = cp.integer(it.value);
      else if (it.type === 'float') j9ToStd[j9Idx] = cp.float(it.bits);
      else if (it.type === 'long') j9ToStd[j9Idx] = cp.long(it.lo, it.hi);
      else if (it.type === 'double') j9ToStd[j9Idx] = cp.double(it.lo, it.hi);
    } else j9ToStd[j9Idx] = null;
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
    // invokeinterface (0xb9/0xe7) requires an InterfaceMethodref (tag 11), not Methodref (10).
    // Two signals: target class is a known interface (ROM metadata; reliable for firmware
    // interfaces), or the ref is used by an invokeinterface opcode (covers JDK interfaces).
    const targetClass = items[it.classIndex - 1];
    const targetIsIface = ifaces && targetClass && targetClass.shape === 'Class' && ifaces.has(targetClass.name);
    const isIface = it.natDesc.startsWith('(') && (targetIsIface || (ifaceRefIdx && ifaceRefIdx.has(i + 1)));
    const tag = it.natDesc.startsWith('(') ? (isIface ? 11 : 10) : 9;
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

// J9 renumbers returns (0xAC..0xB1). Standard JVM requires return opcodes to strictly match
// the method's return type (JVM spec §6.5: return/ireturn/lreturn/freturn/dreturn/areturn).
function mapReturn(op, ret) {
  if (ret === 'V') return 0xb1; // return (void)
  if (ret === 'J') return 0xad; // lreturn (long)
  if (ret === 'D') return 0xaf; // dreturn (double)
  if (ret === 'F') return 0xae; // freturn (float)
  if (ret === 'L' || ret === '[') return 0xb0; // areturn (ref)
  return 0xac; // ireturn (I, Z, B, C, S)
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
    if (wide) out.writeUInt16BE(m, p); else {
      if (m > 255) throw new Error(`CP index ${m} exceeds 255 for single-byte ldc (op@${i})`);
      out[p] = m;
    }
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
    if (op >= 0xac && op <= 0xb1) { out[i] = mapReturn(op, retType); i += 1; continue; }
    if (J9_PRIVATE_1BYTE[op] !== undefined) { out[i] = J9_PRIVATE_1BYTE[op]; i += 1; continue; }
    if (op === 0xe7) { // J9 invokeinterfacequick: e7 00 b9 <idx16 LE> -> b9 <idx16 BE> <count> <0>
      const idx = out.readUInt16LE(i + 3);
      const m = indexMap.get(idx);
      if (m === undefined) throw new Error(`CP index ${idx} (invokeinterface@${i}) not mapped`);
      const dObj = refDesc ? refDesc.get(idx) : null;
      const desc = typeof dObj === 'string' ? dObj : (dObj ? dObj.natDesc : null);
      out[i] = 0xb9; out.writeUInt16BE(m, i + 1); out[i + 3] = desc ? paramSlots(desc, false) : 1; out[i + 4] = 0;
      i += 5; continue;
    }
    if (op === 0xd5) { // J9 wide iinc: d5 <local16 LE> <const16 LE> 00 -> c4 84 <local16 BE> <const16 BE>
      const loc = out.readUInt16LE(i + 1);
      const inc = out.readInt16LE(i + 3);
      out[i] = 0xc4;
      out[i + 1] = 0x84;
      out.writeUInt16BE(loc, i + 2);
      out.writeInt16BE(inc, i + 4);
      local(loc, 1);
      i += 6;
      continue;
    }
    if (op === 0xe5) { // J9 quick return from empty switch (pops switch key beneath return value and returns)
      if (i >= 4 && out[i - 4] === 0x1b && out[i - 3] === 0x08 && out[i - 2] === 0x7a && out[i - 1] === 0x01) {
        // Pattern C (invoke): 1b 08 7a 01 e5 -> nop nop nop aconst_null areturn
        out[i - 4] = 0x00; out[i - 3] = 0x00; out[i - 2] = 0x00;
        out[i] = mapReturn(0xb0, retType);
        i += 1; continue;
      }
      if (i >= 3 && out[i - 3] === 0x1b && out[i - 2] === 0x15 && out[i - 1] === 0x04) {
        // Pattern A (a0..a4): 1b 15 04 e5 -> nop iload_4 ireturn
        out[i - 3] = 0x00;
        out[i] = mapReturn(0xac, retType);
        i += 1; continue;
      }
      if (i >= 4 && out[i - 4] === 0x15 && out[i - 3] === 0x05 && out[i - 2] === 0x15 && out[i - 1] === 0x04) {
        // Pattern B (getIndex): 15 05 15 04 e5 -> nop nop iload_4 ireturn
        out[i - 4] = 0x00; out[i - 3] = 0x00;
        out[i] = mapReturn(0xac, retType);
        i += 1; continue;
      }
      out[i] = mapReturn(0xac, retType);
      i += 1; continue;
    }
    if (op >= 0xca && op <= 0xff && !CP16.has(op)) throw new Error(`J9 private opcode 0x${op.toString(16)} @${i} needs handling`);
    if (op === 0x12) { remap(i + 1, false); i += 2; continue; } // ldc
    if (op === 0xb9 || op === 0xba) { remap(i + 1, true); i += 5; continue; } // invokeinterface/invokedynamic
    if (CP16.has(op)) {
      if (op === 0xf9) out[i] = 0x14;
      if (op === 0xb7 && refDesc) {
        const idx = out.readUInt16LE(i + 1);
        const info = typeof refDesc.get === 'function' ? refDesc.get(idx) : null;
        if (info) {
          const natDesc = typeof info === 'string' ? info : info.natDesc;
          const natName = info.natName || '';
          const isObject = !!info.isObject;
          if ((isObject && natName !== '<init>' && natName !== 'clone') ||
              natName === 'wait' || natName === 'notify' || natName === 'notifyAll' || natName === 'getClass' ||
              natDesc === '()Ljava/lang/Class;') {
            out[i] = 0xb6; // invokespecial Object method -> invokevirtual
          }
        }
      }
      remap(i + 1, true); i += (op === 0xc5 ? 4 : 3); continue;
    }
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
    if (op === 0xaa) { const lo = bc.readInt32LE(def + 4), hi = bc.readInt32LE(def + 8); return def + 12 + (hi - lo + 1) * 4 - p; }
    return def + 8 + bc.readInt32LE(def + 4) * 8 - p;
  }
  if (op === 0xb9 || op === 0xba) return 5;
  if (op === 0xc5) return 4;
  if (op === 0x12) return 2;
  if (CP16.has(op)) return 3;
  if (BRANCH.has(op)) return 3;
  if (op === 0xc8 || op === 0xc9) return 5;
  if (op === 0xd5) return 6;
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
  if (op === 0x84 || op === 0xd5) return 0;
  if (op >= 0x85 && op <= 0x93) { const push2 = [0x85, 0x87, 0x8c, 0x8d, 0x8f].includes(op); const pop2 = [0x88, 0x89, 0x8a, 0x8e, 0x8f, 0x90].includes(op); return (push2 ? 2 : 1) - (pop2 ? 2 : 1); }
  if (op === 0x94 || op === 0x97 || op === 0x98) return -3;
  if (op === 0x95 || op === 0x96) return -1;
  if (op >= 0x99 && op <= 0x9e) return -1;
  if (op >= 0x9f && op <= 0xa6) return -2;
  if (op === 0xa7) return 0;
  if (op === 0xa8) return 1;
  if (op === 0xaa || op === 0xab) return -1;
  if ((op >= 0xac && op <= 0xb1) || op === 0xe5) return (op === 0xad || op === 0xaf) ? -2 : -1;
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
    for (let i = 0; i < pending.length; i++) {
      const e = pending[i];
      if (e.depth === valueDepth && e.words === words) {
        const existing = kinds.get(e.idx);
        if (existing !== 'float' && existing !== 'double') {
          kinds.set(e.idx, kind);
        }
        pending.splice(i, 1);
        return;
      }
    }
  };
  for (let mi = 0; mi < mStarts.length; mi++) {
    const words = (u32(b, base + mStarts[mi] + 12) >>> 16) & 0xffff;
    if (!words) continue;
    const mSig = utf8At(b, selfRel(b, base + mStarts[mi] + 4));
    const rawRet = mSig ? mSig.slice(mSig.indexOf(')') + 1) : '';
    const retType = rawRet ? rawRet[0] : 'V';
    const bcStart = base + mStarts[mi] + 20;
    const bcEnd = mi + 1 < mStarts.length ? base + mStarts[mi + 1] : base + romSize;
    const bc = b.subarray(bcStart, Math.min(bcStart + words * 4, bcEnd));

    const stateAt = new Map();
    const queue = [];
    const addSuccessor = (succ, depth, pending) => {
      if (succ < 0 || succ >= bc.length) return;
      const ex = stateAt.get(succ);
      if (!ex) {
        stateAt.set(succ, { depth, pending: pending.slice() });
        queue.push(succ);
      } else {
        let changed = false;
        for (const e of pending) {
          if (!ex.pending.some((x) => x.idx === e.idx && x.depth === e.depth)) {
            ex.pending.push(e);
            changed = true;
          }
        }
        if (changed) queue.push(succ);
      }
    };

    addSuccessor(0, 0, []);

    // Check for exception handlers
    const exOff = base + mStarts[mi] + 20 + words * 4;
    if (exOff + 4 <= b.length && exOff + 4 <= bcEnd) {
      const catchCount = b.readUInt16LE(exOff);
      if (catchCount > 0 && catchCount < 100) {
        for (let c = 0; c < catchCount; c++) {
          const cOff = exOff + 4 + c * 16;
          if (cOff + 16 <= b.length && cOff + 16 <= bcEnd) {
            const handlerPc = b.readUInt16LE(cOff + 4);
            addSuccessor(handlerPc, 1, []);
          }
        }
      }
    }

    const processOffset = (p) => {
      const state = stateAt.get(p);
      let depth = state.depth;
      let pending = state.pending.slice();
      const prune = () => { pending = pending.filter((e) => e.depth + e.words <= depth); };
      const op = bc[p];
      const len = Math.max(1, instrLen(bc, p));

      if (op === 0x12 || op === 0x13) {
        const idx = op === 0x13 ? bc.readUInt16LE(p + 1) : bc[p + 1];
        if (!kinds.has(idx)) kinds.set(idx, 'ldc');
        pending.push({ idx, depth, words: 1 });
        depth += 1; prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x14 || op === 0xf9) {
        const idx = bc.readUInt16LE(p + 1);
        if (!kinds.has(idx)) kinds.set(idx, 'long');
        pending.push({ idx, depth, words: 2 });
        depth += 2; prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0xb2 || op === 0xb4) { // getstatic/getfield
        const desc = refDesc.get(bc.readUInt16LE(p + 1));
        depth += typeWords(desc) - (op === 0xb4 ? 1 : 0);
        prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0xb3 || op === 0xb5) { // putstatic/putfield
        const desc = refDesc.get(bc.readUInt16LE(p + 1));
        const vw = typeWords(desc);
        resolve(pending, depth - vw, vw, typeKind(desc));
        depth -= vw + (op === 0xb5 ? 1 : 0);
        prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x62 || op === 0x66 || op === 0x6a || op === 0x6e || op === 0x72) { // fadd, fsub, fmul, fdiv, frem
        resolve(pending, depth - 1, 1, 'float');
        resolve(pending, depth - 2, 1, 'float');
        depth -= 1; prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x76 || op === 0x8b) { // fneg, f2i
        resolve(pending, depth - 1, 1, 'float');
        prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if ((op >= 0xac && op <= 0xb1) || op === 0xe5) { // returns
        if (retType === 'D') {
          resolve(pending, depth - 2, 2, 'double');
          depth -= 2;
        } else if (retType === 'J') {
          resolve(pending, depth - 2, 2, 'long');
          depth -= 2;
        } else if (retType === 'F') {
          resolve(pending, depth - 1, 1, 'float');
          depth -= 1;
        } else if ('IZBCS'.includes(retType)) {
          resolve(pending, depth - 1, 1, 'int');
          depth -= 1;
        } else if (retType === 'L' || retType === '[') {
          depth -= 1;
        }
        if (depth < 0) depth = 0;
        prune();
        return;
      }
      if (op === 0xbf) { // athrow
        depth -= 1;
        if (depth < 0) depth = 0;
        prune();
        return;
      }
      if (op === 0x8c || op === 0x8d) { // f2l, f2d
        resolve(pending, depth - 1, 1, 'float');
        depth += 1; prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x95 || op === 0x96) { // fcmpl, fcmpg
        resolve(pending, depth - 1, 1, 'float');
        resolve(pending, depth - 2, 1, 'float');
        depth -= 1; prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x51) { // fastore
        resolve(pending, depth - 1, 1, 'float');
        resolve(pending, depth - 2, 1, 'int');
        depth -= 3; prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x38 || (op >= 0x43 && op <= 0x46)) { // fstore, fstore_0..3
        resolve(pending, depth - 1, 1, 'float');
        depth -= 1; prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x63 || op === 0x67 || op === 0x6b || op === 0x6f || op === 0x73) { // dadd, dsub, dmul, ddiv, drem
        resolve(pending, depth - 2, 2, 'double');
        resolve(pending, depth - 4, 2, 'double');
        depth -= 2; prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x77 || op === 0x8f) { // dneg, d2l
        resolve(pending, depth - 2, 2, 'double');
        prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x8e || op === 0x90) { // d2i, d2f
        resolve(pending, depth - 2, 2, 'double');
        depth -= 1; prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x97 || op === 0x98) { // dcmpl, dcmpg
        resolve(pending, depth - 2, 2, 'double');
        resolve(pending, depth - 4, 2, 'double');
        depth -= 3; prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x52) { // dastore
        resolve(pending, depth - 2, 2, 'double');
        resolve(pending, depth - 3, 1, 'int');
        depth -= 4; prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x39 || (op >= 0x47 && op <= 0x4a)) { // dstore, dstore_0..3
        resolve(pending, depth - 2, 2, 'double');
        depth -= 2; prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x61 || op === 0x65 || op === 0x69 || op === 0x6d || op === 0x71 || op === 0x7e || op === 0x80 || op === 0x82) { // ladd..lxor
        resolve(pending, depth - 2, 2, 'long');
        resolve(pending, depth - 4, 2, 'long');
        depth -= 2; prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x75 || op === 0x8a) { // lneg, l2d
        resolve(pending, depth - 2, 2, 'long');
        prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x88 || op === 0x89) { // l2i, l2f
        resolve(pending, depth - 2, 2, 'long');
        depth -= 1; prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x94) { // lcmp
        resolve(pending, depth - 2, 2, 'long');
        resolve(pending, depth - 4, 2, 'long');
        depth -= 3; prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x50) { // lastore
        resolve(pending, depth - 2, 2, 'long');
        resolve(pending, depth - 3, 1, 'int');
        depth -= 4; prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x37 || (op >= 0x3f && op <= 0x42)) { // lstore, lstore_0..3
        resolve(pending, depth - 2, 2, 'long');
        depth -= 2; prune();
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0x85 || op === 0x86 || op === 0x87) { // i2l, i2f, i2d
        resolve(pending, depth - 1, 1, 'int');
        depth += op === 0x86 ? 0 : 1; prune();
        addSuccessor(p + len, depth, pending);
        return;
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
        prune();
        addSuccessor(p + len, depth, pending);
        return;
      }

      // Branch instructions:
      if (op === 0xa7) { // goto (16-bit LE relative offset)
        const target = p + bc.readInt16LE(p + 1);
        addSuccessor(target, depth, pending);
        return;
      }
      if (op === 0xc8) { // goto_w (32-bit LE relative offset)
        const target = p + bc.readInt32LE(p + 1);
        addSuccessor(target, depth, pending);
        return;
      }
      if ((op >= 0x99 && op <= 0xa6) || op === 0xc6 || op === 0xc7) { // conditional branch
        const target = p + bc.readInt16LE(p + 1);
        depth += stackDelta(op, bc, p);
        if (depth < 0) depth = 0;
        prune();
        addSuccessor(target, depth, pending);
        addSuccessor(p + len, depth, pending);
        return;
      }
      if (op === 0xaa) { // tableswitch
        depth += stackDelta(op, bc, p);
        if (depth < 0) depth = 0;
        prune();
        const pad = (4 - ((p + 1) % 4)) % 4;
        const def = p + 1 + pad;
        if (def + 12 <= bc.length) {
          const dflt = bc.readInt32LE(def);
          const lo = bc.readInt32LE(def + 4), hi = bc.readInt32LE(def + 8);
          addSuccessor(p + dflt, depth, pending);
          for (let k = 0; k <= hi - lo && def + 12 + (k + 1) * 4 <= bc.length; k++) {
            addSuccessor(p + bc.readInt32LE(def + 12 + k * 4), depth, pending);
          }
        }
        return;
      }
      if (op === 0xab) { // lookupswitch
        depth += stackDelta(op, bc, p);
        if (depth < 0) depth = 0;
        prune();
        const pad = (4 - ((p + 1) % 4)) % 4;
        const def = p + 1 + pad;
        if (def + 8 <= bc.length) {
          const dflt = bc.readInt32LE(def);
          const npairs = bc.readInt32LE(def + 4);
          addSuccessor(p + dflt, depth, pending);
          for (let k = 0; k < npairs && def + 8 + k * 8 + 8 <= bc.length; k++) {
            addSuccessor(p + bc.readInt32LE(def + 8 + k * 8 + 4), depth, pending);
          }
        }
        return;
      }

      // Default: other instructions
      depth += stackDelta(op, bc, p);
      if (depth < 0) depth = 0;
      prune();
      addSuccessor(p + len, depth, pending);
    };

    while (queue.length > 0) {
      processOffset(queue.shift());
    }

    // Sweep any unvisited instructions to ensure no unreachable code is missed
    for (let p2 = 0; p2 < bc.length; p2 += Math.max(1, instrLen(bc, p2))) {
      if (!stateAt.has(p2)) {
        addSuccessor(p2, 0, []);
        while (queue.length > 0) {
          processOffset(queue.shift());
        }
      }
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
  for (const t of paramTypes(sig)) n += typeWords(t);
  return n;
}

export function writeClassFile({ className, superName, modifiers, interfaces, fields, methods, cp }) {
  const u2 = (v) => { const x = Buffer.alloc(2); x.writeUInt16BE(v); return x; };
  const u4 = (v) => { const x = Buffer.alloc(4); x.writeUInt32BE(v); return x; };
  // Phase 1: add ALL CP entries first (serialize must see the complete pool).
  const thisIdx = cp.clazz(className);
  const superIdx = superName ? cp.clazz(superName) : 0;
  const ifaceIdx = interfaces.map((n) => cp.clazz(n));
  const constName = cp.utf8('ConstantValue');
  const fd = fields.map((f) => {
    let constIdx = 0;
    if (f.constVal && (f.modifiers & 0x0008) !== 0) {
      if (f.constVal.type === 'String' && f.constVal.cpIdx) constIdx = f.constVal.cpIdx;
      else if (f.constVal.type === 'int') constIdx = cp.integer(f.constVal.val);
      else if (f.constVal.type === 'float') constIdx = cp.float(f.constVal.bits);
      else if (f.constVal.type === 'long') constIdx = cp.long(f.constVal.lo, f.constVal.hi);
      else if (f.constVal.type === 'double') constIdx = cp.double(f.constVal.lo, f.constVal.hi);
    }
    return { mods: f.modifiers & 0xffff, n: cp.utf8(f.name), d: cp.utf8(f.desc), constIdx, constName: constIdx ? constName : 0 };
  });
  const md = methods.map((m) => {
    const hasCode = m.bytecode && m.bytecode.length > 0;
    let thrownIdx = [];
    if (m.thrownExceptions && m.thrownExceptions.length > 0) {
      thrownIdx = m.thrownExceptions.map((n) => cp.clazz(n));
    }
    return {
      mods: m.modifiers & 0xffff,
      n: cp.utf8(m.name),
      d: cp.utf8(m.sig),
      hasCode,
      codeN: hasCode ? cp.utf8('Code') : 0,
      thrownIdx,
      exN: thrownIdx.length > 0 ? cp.utf8('Exceptions') : 0,
      m
    };
  });
  // Phase 2: serialize the now-complete pool.
  const ser = cp.serialize();
  // Phase 3: assemble.
  const parts = [u4(0xcafebabe), u2(0), u2(46), u2(ser.count), ser.bytes];
  parts.push(u2(modifiers & 0xffff), u2(thisIdx), u2(superIdx), u2(ifaceIdx.length));
  for (const i of ifaceIdx) parts.push(u2(i));
  parts.push(u2(fd.length));
  for (const f of fd) {
    if (f.constIdx) {
      parts.push(u2(f.mods), u2(f.n), u2(f.d), u2(1));
      parts.push(u2(f.constName), u4(2), u2(f.constIdx));
    } else {
      parts.push(u2(f.mods), u2(f.n), u2(f.d), u2(0));
    }
  }
  parts.push(u2(md.length));
  for (const x of md) {
    const attrCount = (x.hasCode ? 1 : 0) + (x.thrownIdx.length > 0 ? 1 : 0);
    parts.push(u2(x.mods), u2(x.n), u2(x.d), u2(attrCount));
    if (x.hasCode) {
      const m = x.m;
      const exParts = [];
      if (m.exceptions && m.exceptions.length > 0) {
        exParts.push(u2(m.exceptions.length));
        for (const e of m.exceptions) {
          exParts.push(u2(e.startPc), u2(e.endPc), u2(e.handlerPc), u2(e.catchType));
        }
      } else {
        exParts.push(u2(0));
      }
      const code = Buffer.concat([
        u2(m.maxStack),
        u2(m.maxLocals),
        u4(m.bytecode.length),
        m.bytecode,
        Buffer.concat(exParts),
        u2(0)
      ]);
      parts.push(u2(x.codeN), u4(code.length), code);
    }
    if (x.thrownIdx.length > 0) {
      const thParts = [u2(x.thrownIdx.length)];
      for (const idx of x.thrownIdx) thParts.push(u2(idx));
      const thBuf = Buffer.concat(thParts);
      parts.push(u2(x.exN), u4(thBuf.length), thBuf);
    }
  }
  parts.push(u2(0));
  return Buffer.concat(parts);
}

// Convert one ROM class to a standard .class file. Returns { bytes, info, lines }.
// Parse the field table (offset raw[10]+40). In J9 ROM format, compile-time-constant
// fields have modifier bit 0x00400000 set and are followed by 1 word (or 2 words for J/D)
// of ConstantValue data. Returns the fields plus the region [start,end) to exclude from CP classification.
function parseFields(b, base, raw, methodsOffset) {
  const fields = [];
  const start = raw[10] + 40;
  let fpos = base + start;
  const fEnd = base + methodsOffset;
  for (let i = 0; i < raw[9]; i++) {
    if (fpos + 12 > fEnd) break;
    const name = utf8At(b, selfRel(b, fpos)), desc = utf8At(b, selfRel(b, fpos + 4));
    const rawMods = u32(b, fpos + 8);
    const hasConst = (rawMods & 0x00400000) !== 0;
    const constWords = hasConst ? ((desc === 'J' || desc === 'D') ? 2 : 1) : 0;
    let constVal = null;
    if (hasConst) {
      if (desc === 'Ljava/lang/String;') {
        constVal = { type: 'String', j9CpIdx: b.readInt32LE(fpos + 12) };
      } else if (desc === 'J') {
        constVal = { type: 'long', lo: b.readInt32LE(fpos + 12), hi: b.readInt32LE(fpos + 16) };
      } else if (desc === 'D') {
        constVal = { type: 'double', lo: b.readInt32LE(fpos + 12), hi: b.readInt32LE(fpos + 16) };
      } else if (desc === 'F') {
        constVal = { type: 'float', bits: b.readUInt32LE(fpos + 12) };
      } else if ('ZBCSI'.includes(desc)) {
        constVal = { type: 'int', val: b.readInt32LE(fpos + 12) };
      }
    }
    // Mask J9 modifier bits to JVM field access flags; strip 0x1000 (J9 synthetic marker,
    // represented as a Synthetic attribute in standard classes — the flag crashes the JVM
    // field layout builder for patched core classes like java.lang.Void).
    if (name && desc) {
      let mods = rawMods & 0xefff;
      if ((mods & 0x50) === 0x50) mods &= ~0x40; // final+volatile is illegal; J9 volatile bit is a flag
      fields.push({ name, desc, modifiers: mods, constVal });
    }
    fpos += 12 + constWords * 4;
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
  // Collect J9 CP indices used by invokeinterface (0xe7 quick / 0xb9) so those refs are
  // emitted as InterfaceMethodref. Scanning the opcode usage works even for JDK interfaces
  // (e.g. java/util/Set) that are not present in the firmware ROM.
  const ifaceRefIdx = new Set();
  const ldc8 = new Set();
  for (let i = 0; i < mStarts.length; i++) {
    const bcWords = b.readUInt16LE(base + mStarts[i] + 14);
    if (bcWords === 0) continue;
    const bcStart = base + mStarts[i] + 20;
    const bcEnd = i + 1 < mStarts.length ? base + mStarts[i + 1] : base + h.romSize;
    const bc = b.subarray(bcStart, Math.min(bcStart + bcWords * 4, bcEnd));
    for (let p = 0; p < bc.length;) {
      const op = bc[p];
      if (op === 0x12 && p + 1 < bc.length) ldc8.add(bc[p + 1]);
      if (op === 0xe7 && p + 5 <= bc.length) ifaceRefIdx.add(bc.readUInt16LE(p + 3));
      else if (op === 0xb9 && p + 5 <= bc.length) ifaceRefIdx.add(bc.readUInt16LE(p + 1));
      const len = instrLen(bc, p);
      p += len > 0 ? len : 1;
    }
  }
  if (!img._ifaces) img._ifaces = new Set(img.classes.filter((c) => (c.header.raw[4] & 0x0200) !== 0).map((c) => c.name));
  const { cp, indexMap } = buildStandardCP(items, ifaceRefIdx, img._ifaces, ldc8);
  for (const f of fields) {
    if (f.constVal && f.constVal.type === 'String') {
      const stdIdx = indexMap.get(f.constVal.j9CpIdx);
      if (stdIdx) f.constVal.cpIdx = stdIdx;
      else {
        const cpSlotOff = base + 112 + (f.constVal.j9CpIdx - 1) * 8;
        const str = utf8At(b, selfRel(b, cpSlotOff));
        if (str !== null) f.constVal.cpIdx = cp.string(str);
      }
    }
  }
  const refDesc = new Map();
  items.forEach((it, i) => {
    if (it.shape === 'Ref') {
      const cls = items[it.classIndex - 1];
      const isObject = cls && cls.shape === 'Class' && cls.name === 'java/lang/Object';
      refDesc.set(i + 1, { natDesc: it.natDesc, natName: it.natName, isObject });
    }
  });
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
    const exceptions = [];
    const thrownExceptions = [];
    if ((m.modifiers & 0x00020000) !== 0) {
      const exOff = base + mStarts[i] + 20 + m.bcWords * 4;
      if (exOff + 4 <= b.length && exOff + 4 <= bcEnd) {
        const catchCount = b.readUInt16LE(exOff);
        const throwsCount = b.readUInt16LE(exOff + 2);
        for (let k = 0; k < catchCount; k++) {
          const eOff = exOff + 4 + k * 16;
          if (eOff + 16 <= b.length && eOff + 16 <= bcEnd) {
            const startPc = b.readUInt32LE(eOff);
            const endPc = b.readUInt32LE(eOff + 4);
            const handlerPc = b.readUInt32LE(eOff + 8);
            const j9CatchType = b.readUInt32LE(eOff + 12);
            let catchType = 0;
            if (j9CatchType !== 0) {
              catchType = indexMap.get(j9CatchType) || 0;
              if (!catchType && items[j9CatchType - 1] && items[j9CatchType - 1].shape === 'Class') {
                catchType = cp.clazz(items[j9CatchType - 1].name);
              }
            }
            exceptions.push({ startPc, endPc, handlerPc, catchType });
          }
        }
        for (let t = 0; t < throwsCount; t++) {
          const tOff = exOff + 4 + catchCount * 16 + t * 4;
          if (tOff + 4 <= b.length && tOff + 4 <= bcEnd) {
            const className = utf8At(b, selfRel(b, tOff));
            if (className) thrownExceptions.push(className);
          }
        }
      }
    }
    methods.push({ ...m, bytecode, maxLocals, exceptions, thrownExceptions });
    lines.push(`  ${m.name}${m.sig} mods=0x${m.modifiers.toString(16)} stack=${m.maxStack} locals=${maxLocals} bc=${bytecode.length}B ex=${exceptions.length}`);
  }
  // Interface list: raw[5]=numInterfaces, raw[6] (offset 24) is a self-rel pointer to an
  // array of self-rel pointers to interface class names (verified: MIBMain$1 -> Runnable).
  const interfaces = [];
  const numIfaces = raw[5];
  if (numIfaces > 0 && numIfaces < 256) {
    const arr = selfRel(b, base + 24);
    if (arr >= 0 && arr + numIfaces * 4 <= b.length) {
      for (let i = 0; i < numIfaces; i++) {
        const nm = utf8At(b, selfRel(b, arr + i * 4));
        if (nm && nm.length < 256 && /^[\w$/]+$/.test(nm)) interfaces.push(nm);
      }
    }
  }
  const bytes = writeClassFile({ className: target, superName: h.superclassName, modifiers: raw[4], interfaces, fields, methods, cp });
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
