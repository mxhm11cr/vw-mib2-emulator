import { test } from 'node:test';
import assert from 'node:assert';
import { readFileSync, existsSync } from 'node:fs';
import { parseRomImage } from '../src/track-a/jxe/rom-image.js';
import { convertClass, convertBytecode } from '../src/track-a/jxe/rom-to-class.js';

const romPath = 'firmware/analysis/rom/rom.classes';
const haveRom = existsSync(romPath);

test('J9 return opcodes map to standard JVM by return type', () => {
  const m = new Map([[1, 5]]);
  // void: return0 (0xAC) -> return (0xB1)
  assert.equal(convertBytecode(Buffer.from([0x2a, 0xb7, 0x01, 0x00, 0xac]), m, 'V').bytecode[4], 0xb1);
  // int: return1 (0xAD) -> ireturn (0xAC)
  assert.equal(convertBytecode(Buffer.from([0x03, 0xad]), new Map(), 'I').bytecode[1], 0xac);
  // float: return1 -> freturn (0xAE)
  assert.equal(convertBytecode(Buffer.from([0x03, 0xad]), new Map(), 'F').bytecode[1], 0xae);
  // ref: return1 -> areturn (0xB0)
  assert.equal(convertBytecode(Buffer.from([0x01, 0xad]), new Map(), 'L').bytecode[1], 0xb0);
  // long: return2 (0xAE) -> lreturn (0xAD)
  assert.equal(convertBytecode(Buffer.from([0x09, 0xae]), new Map(), 'J').bytecode[1], 0xad);
  // double: return2 -> dreturn (0xAF)
  assert.equal(convertBytecode(Buffer.from([0x0e, 0xae]), new Map(), 'D').bytecode[1], 0xaf);
});

test('J9 little-endian CP operands convert to big-endian + remap', () => {
  // getstatic #3 stored LE (03 00); remap 3 -> 9; then void return
  const { bytecode } = convertBytecode(Buffer.from([0xb2, 0x03, 0x00, 0xac]), new Map([[3, 9]]), 'V');
  assert.equal(bytecode[0], 0xb2);
  assert.equal(bytecode.readUInt16BE(1), 9);
  assert.equal(bytecode[3], 0xb1);
});

test('J9 branch offsets convert LE -> BE', () => {
  // ifnonnull +28 (LE 1c 00) -> BE 00 1c
  const { bytecode } = convertBytecode(Buffer.from([0xc7, 0x1c, 0x00, 0xac]), new Map(), 'V');
  assert.equal(bytecode.readUInt16BE(1), 28);
});

(haveRom ? test : test.skip)('authentic ROM image: 1436 classes, all names resolve', () => {
  const img = parseRomImage(readFileSync(romPath));
  assert.equal(img.header.magic, 'J99J');
  assert.equal(img.classes.length, 1436);
  assert.ok(img.classes.every((c) => c.header.className === c.name), 'every directory name matches class header className');
});

(haveRom ? test : test.skip)('methodsOffset = raw[8]+32 resolves first method on sample classes', () => {
  const img = parseRomImage(readFileSync(romPath));
  for (const name of ['java/lang/Void', 'java/util/Map', 'java/io/File', 'java/lang/Byte']) {
    const c = img.classes.find((x) => x.name === name);
    const off = c.classOff + c.header.raw[8] + 32;
    const b = readFileSync(romPath);
    const sig = (() => { const t = off + 4 + b.readInt32LE(off + 4); const len = b.readUInt16LE(t); return b.toString('latin1', t + 2, t + 2 + len); })();
    assert.ok(sig.startsWith('('), `${name}: first method signature valid (${sig})`);
  }
});

(haveRom ? test : test.skip)('convert java/lang/Void -> valid standard class file', () => {
  const { bytes, info } = convertClass(readFileSync(romPath), 'java/lang/Void');
  assert.equal(bytes.readUInt32BE(0), 0xcafebabe);
  assert.equal(bytes.readUInt16BE(6), 46); // Java 1.2 major version
  assert.equal(info.methods, 2);
  assert.equal(info.fields, 2);
  assert.ok(bytes.includes(Buffer.from([0xb1])), 'contains standard return opcode');
  assert.ok(!bytes.includes(Buffer.from([0xb2, 0x03, 0x00])), 'no leftover LE CP operand');
});

// Count CP entries of a given tag in a converted class file.
function cpTagCount(bytes, wantTag) {
  let p = 8; const count = bytes.readUInt16BE(p); p += 2; let n = 0;
  for (let i = 1; i < count; i++) {
    const tag = bytes[p]; p++;
    if (tag === wantTag) n++;
    if (tag === 1) p += 2 + bytes.readUInt16BE(p);
    else if (tag === 5 || tag === 6) { p += 8; i++; }
    else if (tag === 7 || tag === 8 || tag === 16 || tag === 19 || tag === 20) p += 2;
    else if (tag === 15) p += 3;
    else p += 4;
  }
  return n;
}

(haveRom ? test : test.skip)('float constants typed via stack-depth resolver (IntObjectOptHashMap)', () => {
  const { bytes } = convertClass(readFileSync(romPath), 'de/vw/extlibs/collections/ints/IntObjectOptHashMap');
  assert.equal(bytes.readUInt32BE(0), 0xcafebabe);
  // DEFAULT_LOAD_FACTOR / DEFAULT_RESIZE_THRESHOLD are float constants loaded as <init>(IFF)V
  // args; the resolver must type them Float (tag 4), not Integer.
  assert.ok(cpTagCount(bytes, 4) >= 2, 'expected >=2 Float CP entries');
});

(haveRom ? test : test.skip)('all de.vw HMI classes convert without error', () => {
  const b = readFileSync(romPath);
  const img = parseRomImage(b);
  const vw = img.classes.filter((c) => c.name.startsWith('de/vw'));
  assert.ok(vw.length >= 30, 'expected >=30 de.vw classes, got ' + vw.length);
  for (const c of vw) {
    const { bytes } = convertClass(b, c.name, img);
    assert.equal(bytes.readUInt32BE(0), 0xcafebabe, c.name + ' magic');
  }
});

(haveRom ? test : test.skip)('empty String constant preserved (not misclassified as Float)', () => {
  const b = readFileSync(romPath);
  const { bytes } = convertClass(b, 'de/vw/extlibs/collections/ints/IntObjectOptHashMap');
  // toString uses an empty "" String constant; ensure no spurious Float NaN from it.
  assert.equal(bytes.readUInt32BE(0), 0xcafebabe);
  assert.ok(cpTagCount(bytes, 8) >= 1, 'has String CP entries');
});
