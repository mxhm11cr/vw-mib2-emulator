import { test } from 'node:test';
import assert from 'node:assert';
import { readFileSync, existsSync } from 'node:fs';
import { parseRomImage } from '../src/track-a/jxe/rom-image.js';
import { convertClass } from '../src/track-a/jxe/rom-to-class.js';

const romPath = 'firmware/analysis/p0369/hmi/rom.classes';
const haveRom = existsSync(romPath);

// Parse a standard .class file's implemented-interface names (walks the constant pool).
function interfaceNames(bytes) {
  let p = 8;
  const count = bytes.readUInt16BE(p); p += 2;
  const utf8 = [], classIdx = [];
  for (let i = 1; i < count; i++) {
    const tag = bytes[p]; p++;
    if (tag === 1) { const len = bytes.readUInt16BE(p); p += 2; utf8[i] = bytes.toString('latin1', p, p + len); p += len; }
    else if (tag === 7) { classIdx[i] = bytes.readUInt16BE(p); p += 2; }
    else if (tag === 8 || tag === 16 || tag === 19 || tag === 20) p += 2;
    else if (tag === 15) p += 3;
    else if (tag === 5 || tag === 6) { p += 8; i++; }
    else p += 4;
  }
  p += 6; // access_flags, this_class, super_class
  const n = bytes.readUInt16BE(p); p += 2;
  const names = [];
  for (let i = 0; i < n; i++) { names.push(utf8[classIdx[bytes.readUInt16BE(p)]]); p += 2; }
  return names;
}

(haveRom ? test : test.skip)('converter emits interface list (MIBMain$1 implements Runnable)', () => {
  const b = readFileSync(romPath);
  const img = parseRomImage(b);
  const inner = convertClass(b, 'de/vw/mib/MIBMain$1', img);
  assert.deepEqual(interfaceNames(inner.bytes), ['java/lang/Runnable']);
  const main = convertClass(b, 'de/vw/mib/MIBMain', img);
  assert.ok(interfaceNames(main.bytes).includes('de/vw/mib/log4mib/classifier/StartupClassifier'));
});

(haveRom ? test : test.skip)('CP guard: misidentified Ref with garbage natOff degrades to Unknown (no throw)', () => {
  const b = readFileSync(romPath);
  const img = parseRomImage(b);
  // de/vw/mib/util/Util previously threw ERR_OUT_OF_RANGE in parseCP; must now convert.
  const { bytes } = convertClass(b, 'de/vw/mib/util/Util', img);
  assert.equal(bytes.readUInt32BE(0), 0xcafebabe);
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

(haveRom ? test : test.skip)('firmware interface method refs are InterfaceMethodref (tag 11)', () => {
  const b = readFileSync(romPath);
  const img = parseRomImage(b);
  // ByteArrayCodec.decode calls ByteArrayDecoder.readHeader (a firmware interface). The ref
  // must be tagged 11, not 10, or invokeinterface throws IncompatibleClassChangeError at runtime.
  const { bytes } = convertClass(b, 'de/vw/mib/codec/ByteArrayCodec', img);
  assert.ok(cpTagCount(bytes, 11) >= 1, 'expected >=1 InterfaceMethodref CP entries');
});
