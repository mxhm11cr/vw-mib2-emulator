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

export function parseCP(b, classOff, cpStartRel, cpEndRel) {
  const items = [];
  const start = classOff + cpStartRel, end = classOff + cpEndRel;
  for (let off = start; off + 8 <= end; off += 8) {
    const s1 = u32(b, off), s2 = u32(b, off + 4);
    const name1 = utf8At(b, selfRel(b, off));
    const desc2 = utf8At(b, selfRel(b, off + 4));
    if (s2 === 2 && name1) items.push({ shape: 'Class', name: name1, off });
    else if (s2 === 1 && name1) items.push({ shape: 'String', name: name1, off });
    else if (s1 > 0 && s1 < 4096 && desc2 === null) {
      const natOff = off + 4 + i32(b, off + 4);
      items.push({ shape: 'Ref', classIndex: s1, natOff, natName: utf8At(b, selfRel(b, natOff)), natDesc: utf8At(b, selfRel(b, natOff + 4)), off });
    } else if (name1 && desc2) items.push({ shape: 'NameAndType', name: name1, desc: desc2, off });
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
  serialize() {
    const out = [];
    const u2 = (v) => { const x = Buffer.alloc(2); x.writeUInt16BE(v); return x; };
    for (let i = 1; i < this.entries.length; i++) {
      const e = this.entries[i];
      if (e.tag === 1) out.push(Buffer.from([1]), u2(e.bytes.length), e.bytes);
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
    else if (it.shape === 'NameAndType') j9ToStd[i + 1] = cp.nat(it.name, it.desc);
    else j9ToStd[i + 1] = null;
  }
  for (let i = 0; i < items.length; i++) {
    const it = items[i];
    if (it.shape !== 'Ref') continue;
    const classStd = j9ToStd[it.classIndex];
    const natJ9 = items.findIndex((x) => x.off === it.natOff) + 1;
    const natStd = j9ToStd[natJ9];
    if (!classStd || !natStd) continue;
    const tag = it.natDesc && it.natDesc.startsWith('(') ? 10 : 9;
    j9ToStd[i + 1] = cp.ref(tag, classStd, natStd);
  }
  const indexMap = new Map();
  for (let i = 1; i <= items.length; i++) if (j9ToStd[i]) indexMap.set(i, j9ToStd[i]);
  return { cp, indexMap };
}

// ---- bytecode: LE operands -> BE, CP index remap, compute maxLocals ----
const CP16 = new Set([0x13, 0x14, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xbb, 0xbd, 0xc0, 0xc1, 0xc5]);
const BRANCH = new Set([0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xc6, 0xc7]);
const LOAD = { 0x15: 1, 0x16: 2, 0x17: 1, 0x18: 2, 0x19: 1, 0x36: 1, 0x37: 2, 0x38: 1, 0x39: 2, 0x3a: 1 };
const LOAD_N = {}; // opcode -> [baseIndex, size]
for (let k = 0; k < 4; k++) { LOAD_N[0x1a + k] = [k, 1]; LOAD_N[0x1e + k] = [k, 2]; LOAD_N[0x22 + k] = [k, 1]; LOAD_N[0x26 + k] = [k, 2]; LOAD_N[0x2a + k] = [k, 1]; LOAD_N[0x3b + k] = [k, 1]; LOAD_N[0x3f + k] = [k, 2]; LOAD_N[0x43 + k] = [k, 1]; LOAD_N[0x47 + k] = [k, 2]; LOAD_N[0x4b + k] = [k, 1]; }

// J9 renumbers returns: 0xAC=return0(void), 0xAD=return1(1-word), 0xAE=return2(2-word).
// Map to standard JVM return opcodes by the method's return type (empirically verified
// across Void/Byte/Boolean/Object — see rom-probe11.js).
function mapReturn(op, ret) {
  if (op === 0xac) return 0xb1; // return0 -> return (void)
  if (op === 0xad) { // return1
    if (ret === 'F') return 0xae; // freturn
    if (ret === 'L' || ret === '[') return 0xb0; // areturn
    return 0xac; // ireturn (I/B/S/Z/C)
  }
  if (op === 0xae) return ret === 'D' ? 0xaf : 0xad; // dreturn / lreturn
  return op;
}

export function convertBytecode(bc, indexMap, retType) {
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
    if (op === 0xac || op === 0xad || op === 0xae) { out[i] = mapReturn(op, retType); i += 1; continue; }
    if (op === 0xaf || op === 0xb0 || op === 0xb1) throw new Error(`J9 private opcode 0x${op.toString(16)} @${i} needs handling`);
    if (op === 0x12) { remap(i + 1, false); i += 2; continue; } // ldc
    if (op === 0xb9 || op === 0xba) { remap(i + 1, true); i += 5; continue; } // invokeinterface/invokedynamic
    if (CP16.has(op)) { remap(i + 1, true); i += (op === 0xc5 ? 4 : 3); continue; }
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
export function convertClass(b, target, img = parseRomImage(b)) {
  const lines = [];
  const c = img.classes.find((x) => x.name === target);
  if (!c) throw new Error('class not found: ' + target);
  const base = c.classOff, h = c.header, raw = h.raw;
  const methodsOffset = raw[8] + 32;
  const items = parseCP(b, base, 112, methodsOffset);
  const shapes = {};
  for (const it of items) shapes[it.shape] = (shapes[it.shape] || 0) + 1;
  lines.push(`${target}: romSize=${h.romSize} methods=${raw[7]} fields=${raw[9]} cp=${items.length} shapes=${JSON.stringify(shapes)}`);
  const { cp, indexMap } = buildStandardCP(items);
  const mStarts = [];
  for (let f = methodsOffset; f + 20 <= h.romSize && mStarts.length < raw[7]; f += 4) {
    const n = utf8At(b, selfRel(b, base + f));
    const s = utf8At(b, selfRel(b, base + f + 4));
    if (n && s && s.startsWith('(')) mStarts.push(f);
  }
  const methods = [];
  for (let i = 0; i < mStarts.length; i++) {
    const m = readMethod(b, base + mStarts[i]);
    const bcStart = base + mStarts[i] + 20;
    const bcEnd = i + 1 < mStarts.length ? base + mStarts[i + 1] : base + h.romSize;
    const isAbstract = (m.modifiers & 0x0400) !== 0, isNative = (m.modifiers & 0x0100) !== 0;
    let bytecode = Buffer.alloc(0), maxLocals = paramSlots(m.sig, (m.modifiers & 0x0008) !== 0);
    if (!isAbstract && !isNative && m.bcWords > 0) {
      const rawBc = b.subarray(bcStart, Math.min(bcStart + m.bcWords * 4, bcEnd));
      const conv = convertBytecode(rawBc, indexMap, m.sig.slice(m.sig.indexOf(')') + 1, m.sig.indexOf(')') + 2));
      bytecode = conv.bytecode;
      maxLocals = Math.max(maxLocals, conv.maxLocals);
    }
    methods.push({ ...m, bytecode, maxLocals });
    lines.push(`  ${m.name}${m.sig} mods=0x${m.modifiers.toString(16)} stack=${m.maxStack} locals=${maxLocals} bc=${bytecode.length}B`);
  }
  const fields = [];
  for (let i = 0; i < raw[9]; i++) {
    const fo = base + raw[6] + i * 12;
    const name = utf8At(b, selfRel(b, fo)), desc = utf8At(b, selfRel(b, fo + 4));
    if (name && desc) fields.push({ name, desc, modifiers: u32(b, fo + 8) });
    else lines.push(`  field[${i}] unresolved @+${raw[6] + i * 12}`);
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
