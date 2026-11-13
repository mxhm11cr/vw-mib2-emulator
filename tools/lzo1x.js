// LZO1X decompression — faithful translation of the minilzo lzo1x_decompress algorithm.
// Used for QNX IFS images compressed with STARTUP_HDR_FLAGS1_COMPRESS_LZO (flags1=0x8).
// Block framing (2-byte BE length prefix) is handled by the caller (see ifs-extract.js).
// Reference: QNX dumpifs.c uses lzo1x_decompress; algorithm is the public minilzo LZO1X.

export function lzo1xDecompress(input) {
  const out = [];
  let ip = 0;
  const ipEnd = input.length;
  let t = 0;
  let mpos = 0;
  const lit = (n) => { for (let i = 0; i < n; i++) out.push(input[ip++]); };
  const mc = (n) => { for (let i = 0; i < n; i++) out.push(out[mpos++]); };
  // minilzo zero-length extension: t += 255 per zero byte, then base + next byte.
  // base differs by context: literal run = 15, M3 = 31, M4 = 7.
  const extend = (base) => { while (input[ip] === 0) { t += 255; ip++; } t += base + input[ip++]; };
  // copy_match: 2 bytes then t more (t >= 1), overlap-safe.
  const copyMatch = () => { mc(2); while (t-- > 0) out.push(out[mpos++]); };

  // Faithful minilzo lzo1x_decompress control flow (init/main/first_literal_run/match/
  // match_done/match_next). Post-match trailing literals route to `match` (M1-capable).
  let state = 'init';
  while (ip < ipEnd) {
    if (state === 'init') {
      if (input[ip] > 17) {
        t = input[ip++] - 17;
        if (t < 4) { state = 'match_next'; continue; }
        lit(t);
        state = 'first_literal_run'; continue;
      }
      state = 'main'; continue;
    }
    if (state === 'main') {
      t = input[ip++];
      if (t >= 16) { state = 'match'; continue; }
      if (t === 0) extend(15);
      lit(t + 3);
      state = 'first_literal_run'; continue;
    }
    if (state === 'first_literal_run') {
      t = input[ip++];
      if (t >= 16) { state = 'match'; continue; }
      mpos = out.length - 1 - 0x800 - (t >> 2) - (input[ip++] << 2);
      mc(3);
      state = 'match_done'; continue;
    }
    if (state === 'match') {
      if (t >= 64) {
        mpos = out.length - 1 - ((t >> 2) & 7) - (input[ip++] << 3);
        t = (t >> 5) - 1;
        copyMatch();
      } else if (t >= 32) {
        t &= 31; if (t === 0) extend(31);
        mpos = out.length - 1 - ((input[ip] >> 2) + (input[ip + 1] << 6)); ip += 2;
        copyMatch();
      } else if (t >= 16) {
        mpos = out.length - ((t & 8) << 11);
        t &= 7; if (t === 0) extend(7);
        mpos -= ((input[ip] >> 2) + (input[ip + 1] << 6)); ip += 2;
        if (mpos === out.length) break; // EOF marker
        mpos -= 0x4000;
        copyMatch();
      } else {
        mpos = out.length - 1 - (t >> 2) - (input[ip++] << 2);
        mc(2);
      }
      state = 'match_done'; continue;
    }
    if (state === 'match_done') {
      t = input[ip - 2] & 3;
      if (t === 0) { state = 'main'; continue; }
      state = 'match_next'; continue;
    }
    if (state === 'match_next') {
      lit(t);
      t = input[ip++];
      state = 'match'; continue;
    }
  }
  return Buffer.from(out);
}

// Decompress a QNX LZO block stream: sequence of [u16 BE length][lzo1x block], length 0 terminates.
export function decompressLzoBlocks(buf, start) {
  const chunks = [];
  let p = start;
  while (p + 2 <= buf.length) {
    const len = (buf[p] << 8) | buf[p + 1];
    p += 2;
    if (len === 0) break;
    if (p + len > buf.length) throw new Error(`LZO block overruns buffer at 0x${p.toString(16)} len=${len}`);
    chunks.push(lzo1xDecompress(buf.subarray(p, p + len)));
    p += len;
  }
  return Buffer.concat(chunks);
}
