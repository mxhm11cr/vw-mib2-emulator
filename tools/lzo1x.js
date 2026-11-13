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
  const match = (n) => { for (let i = 0; i < n; i++) out.push(out[mpos + i]); mpos += n; };

  // initial literal run
  if (input[ip] > 17) {
    t = input[ip++] - 17;
    if (t >= 4) {
      lit(t);
      t = input[ip++];
      if (t < 16) {
        mpos = out.length - 1 - 0x800 - (t >> 2) - (input[ip++] << 2);
        match(3);
        t = input[ip - 2] & 3;
        if (t) lit(t);
      } else {
        ip--; // re-read as match token in main loop
      }
    } else {
      lit(t);
    }
  }

  while (ip < ipEnd) {
    t = input[ip++];
    if (t < 16) {
      if (t === 0) { while (input[ip] === 0) { t += 255; ip++; } t += input[ip++]; }
      lit(t + 3);
      t = input[ip++];
      if (t < 16) {
        mpos = out.length - 1 - 0x800 - (t >> 2) - (input[ip++] << 2);
        match(3);
        t = input[ip - 2] & 3;
        if (t) lit(t);
        continue;
      }
    }
    // match
    if (t >= 64) {
      mpos = out.length - 1 - ((t >> 2) & 7) - (input[ip++] << 3);
      t = (t >> 5) - 1;
      match(3); while (t-- > 0) out.push(out[mpos++]);
    } else if (t >= 32) {
      t &= 31;
      if (t === 0) { while (input[ip] === 0) { t += 255; ip++; } t += input[ip++]; }
      mpos = out.length - 1 - ((input[ip] >> 2) | (input[ip + 1] << 6));
      ip += 2; t += 2;
      match(3); while (t-- > 0) out.push(out[mpos++]);
    } else if (t >= 16) {
      mpos = out.length - ((t & 8) << 11);
      t &= 7;
      if (t === 0) { while (input[ip] === 0) { t += 255; ip++; } t += input[ip++]; }
      mpos -= ((input[ip] >> 2) | (input[ip + 1] << 6));
      ip += 2;
      if (mpos === out.length) break; // EOF marker
      mpos -= 0x4000; t += 2;
      match(3); while (t-- > 0) out.push(out[mpos++]);
    } else {
      mpos = out.length - 1 - (t >> 2) - (input[ip++] << 2);
      match(2);
    }
    t = input[ip - 2] & 3;
    if (t) lit(t);
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
