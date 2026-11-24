const fs = require('node:fs');
const zlib = require('node:zlib');
const { resolve } = require('node:path');

function buildCacheForMcf(mcfPath, outPath, expandAlpha) {
    console.log(`Processing ${mcfPath} -> ${outPath}...`);
    const fd = fs.openSync(mcfPath, 'r');
    const pre = Buffer.alloc(48);
    fs.readSync(fd, pre, 0, 48, 0);
    const hl = pre.readInt32LE(28);
    const dl = pre.readInt32LE(32);
    const comp = pre[40] !== 0;

    const h = Buffer.alloc(hl);
    fs.readSync(fd, h, 0, hl, 48);
    const count = h.readInt32LE(0);
    console.log(`Total images: ${count}`);

    let d = Buffer.alloc(dl);
    fs.readSync(fd, d, 0, dl, 48 + hl);
    if (comp) d = zlib.inflateSync(d);

    const items = [];
    for (let i = 0; i < count; i++) {
        const id = d.readInt32LE(i * 16 + 4);
        const off = d.readUInt32LE(i * 16 + 8);
        const len = d.readUInt32LE(i * 16 + 12);
        items.push({ id, off, len });
    }

    // Sort by ID for fast binary search
    items.sort((a, b) => a.id - b.id);

    const entrySize = 20;
    const headerTableSize = 4 + count * entrySize;
    let currentOffset = headerTableSize;

    const outEntries = Buffer.alloc(count * entrySize);
    const pixelBuffers = [];

    for (let i = 0; i < count; i++) {
        const item = items[i];
        const raw = Buffer.alloc(item.len);
        fs.readSync(fd, raw, 0, item.len, item.off);

        const r_hl = raw.readInt32LE(8);
        const r_dl = raw.readInt32LE(12);
        const r_comp = raw[20] !== 0;
        const w = raw.readUInt16LE(28);
        const hgt = raw.readUInt16LE(30);
        let fmt = raw.readUInt16LE(32);

        let px = Buffer.alloc(r_dl);
        raw.copy(px, 0, 28 + r_hl, 28 + r_hl + r_dl);
        if (r_comp) {
            px = zlib.inflateSync(px);
        }

        // Skin 1 uses white RGBA textures for its alpha masks.
        if (expandAlpha && fmt === 0x1000) {
            const rgba = Buffer.alloc(px.length * 4, 255);
            for (let pixel = 0; pixel < px.length; pixel++) {
                rgba[pixel * 4 + 3] = px[pixel];
            }
            px = rgba;
            fmt = 0x1104;
        }

        // write entry
        outEntries.writeUInt32LE(item.id, i * entrySize);
        outEntries.writeUInt16LE(w, i * entrySize + 4);
        outEntries.writeUInt16LE(hgt, i * entrySize + 6);
        outEntries.writeUInt16LE(fmt, i * entrySize + 8);
        outEntries.writeUInt16LE(0, i * entrySize + 10); // opaque = 0
        outEntries.writeUInt32LE(currentOffset, i * entrySize + 12);
        outEntries.writeUInt32LE(px.length, i * entrySize + 16);

        currentOffset += px.length;
        pixelBuffers.push(px);
    }
    fs.closeSync(fd);

    console.log(`Writing cache file ${outPath} (${currentOffset} bytes)...`);
    const outFd = fs.openSync(outPath, 'w');
    const countBuf = Buffer.alloc(4);
    countBuf.writeUInt32LE(count, 0);
    fs.writeSync(outFd, countBuf, 0, 4);
    fs.writeSync(outFd, outEntries, 0, outEntries.length);
    for (const px of pixelBuffers) {
        fs.writeSync(outFd, px, 0, px.length);
    }
    fs.closeSync(outFd);
    console.log(`Done writing ${outPath}!`);
}

const [mcfPath, outPath, option] = process.argv.slice(2);
if (!mcfPath || !outPath || resolve(mcfPath) === resolve(outPath) || (option && option !== '--expand-alpha')) {
    console.error('Usage: node tools/build-skin-cache.cjs <images.mcf> <output.bin> [--expand-alpha]');
    process.exitCode = 1;
} else {
    buildCacheForMcf(mcfPath, outPath, option === '--expand-alpha');
}
