// Jalankan: node tests/test_gifkit.js  (butuh /tmp/*.gif dari gen_samples.py)
const fs = require('fs');
const src = fs.readFileSync(__dirname + '/../src/webui.h', 'utf8');
const code = src.split('// <gifkit>')[1].split('// </gifkit>')[0];
const kit = new Function(code + '; return {lzwDecode,scanGif,gifFrames,pickIndices,subsample,buildPalette,buildLut,quantizeFrame,lzwEncode,encodeGif};')();

// 1) dump frame hasil decoder -> JSON untuk dibandingkan dengan Pillow
const out = {};
for (const n of ['grad', 'trans2', 'trans3', 'noise']) {
  const buf = new Uint8Array(fs.readFileSync(`/tmp/${n}.gif`));
  const m = kit.scanGif(buf);
  const fr = [];
  for (const f of kit.gifFrames(buf)) fr.push({ d: f.delay, px: Buffer.from(f.rgba).toString('base64') });
  out[n] = { w: m.w, h: m.h, count: m.count, frames: fr };
}
fs.writeFileSync('/tmp/js_decoded.json', JSON.stringify(out));

// 2) LZW round trip (acak, run panjang, tabel penuh)
function rt(pix, label) {
  const enc = kit.lzwEncode(pix, 8);
  const dec = kit.lzwDecode(8, Uint8Array.from(enc), pix.length);
  let ok = dec.length === pix.length; for (let i = 0; ok && i < pix.length; i++) if (dec[i] !== pix[i]) ok = false;
  console.log('LZW', label, ok ? 'OK' : 'GAGAL', pix.length, '->', enc.length);
  if (!ok) process.exitCode = 1;
}
let r = 12345; const rnd = () => (r = (r * 1103515245 + 12345) & 0x7fffffff) >> 8;
rt(Uint8Array.from({ length: 50000 }, () => rnd() & 255), 'acak');
rt(new Uint8Array(40000), 'nol semua');
rt(Uint8Array.from({ length: 30000 }, (_, i) => (i >> 5) & 255), 'gradasi');
rt(Uint8Array.from({ length: 1 }, () => 7), '1 piksel');
rt(Uint8Array.from({ length: 2 }, () => 7), '2 piksel');
for (const n of [250, 255, 256, 257, 258, 259, 300, 500, 510, 511, 512, 513, 1000, 3838, 3839, 3840, 3841, 4000, 8000])
  rt(Uint8Array.from({ length: n }, () => rnd() & 255), 'acak n=' + n);

// 3) pipeline lengkap: decode -> quantize -> encode, simpan untuk dicek Pillow
const exp = {};
for (const n of ['grad', 'trans2', 'noise']) {
  const buf = new Uint8Array(fs.readFileSync(`/tmp/${n}.gif`));
  const m = kit.scanGif(buf);
  const frames = [];
  for (const f of kit.gifFrames(buf)) frames.push({ rgba: new Uint8ClampedArray(f.rgba), delay: f.delay });
  for (const dither of [0, 5]) {
    const pal = kit.buildPalette(frames.map(f => f.rgba), 255);
    const lut = kit.buildLut(pal);
    const idx = frames.map(f => ({ idx: kit.quantizeFrame(f.rgba, m.w, m.h, lut, dither), delay: f.delay }));
    const bytes = kit.encodeGif(m.w, m.h, idx, pal);
    fs.writeFileSync(`/tmp/enc_${n}_${dither}.gif`, bytes);
    exp[`${n}_${dither}`] = { w: m.w, h: m.h, pal: Buffer.from(pal.rgb).toString('base64'), n: pal.n,
      frames: idx.map(f => ({ d: f.delay, i: Buffer.from(f.idx).toString('base64') })) };
    console.log('encode', n, 'dither', dither, 'palet', pal.n, 'ukuran', bytes.length);
  }
}
fs.writeFileSync('/tmp/js_expected.json', JSON.stringify(exp));
