#pragma once
#include <pgmspace.h>

// ---------------------------------------------------------------------------
//  Halaman utama: upload GIF + resize otomatis di browser + daftar GIF di TTGO
// ---------------------------------------------------------------------------
static const char WEBUI_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="id"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TTGO GIF</title>
<style>
:root{--bg:#e8edf1;--ink:#121c26;--mute:#5b6b7a;--panel:#fff;--line:#c9d3dc;--acc:#1f4bff;--ok:#12805c;--bad:#c0352b}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
main{max-width:640px;margin:0 auto;padding:16px 14px 60px}
header{display:flex;justify-content:space-between;align-items:flex-end;gap:10px;margin:6px 0 16px}
h1{margin:0;font-size:28px;letter-spacing:-.02em;line-height:1}
h1 small{display:block;font-size:13px;font-weight:500;color:var(--mute);letter-spacing:0;margin-top:6px}
h2{font-size:15px;margin:26px 0 10px}
#net{font-size:12px;color:var(--mute);text-align:right;line-height:1.35}
#net a{color:var(--acc)}
.card{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:14px}
.drop{display:block;border:2px dashed #93a4b4;border-radius:10px;padding:26px 12px;text-align:center;cursor:pointer;background:#f6f9fb;font-weight:600}
.drop.on{border-color:var(--acc);background:#eaf0ff}
.drop span{display:block;font-weight:400;font-size:13px;color:var(--mute);margin-top:4px}
.opts{display:grid;grid-template-columns:1fr 1fr;gap:10px 14px;margin-top:14px}
.opts label{display:flex;flex-direction:column;gap:4px;font-size:13px;color:var(--mute)}
.opts label.chk{flex-direction:row;align-items:center;gap:8px;color:var(--ink)}
select,input[type=number],input[type=text],input[type=password]{font:inherit;padding:8px;border:1px solid var(--line);border-radius:6px;background:#fff;color:var(--ink);width:100%}
button{font:inherit;font-weight:600;padding:8px 12px;border:0;border-radius:6px;background:var(--acc);color:#fff;cursor:pointer}
button.alt{background:#e3e9ef;color:var(--ink)}
button.del{background:#fbe6e4;color:var(--bad)}
.scr{background:#000;border:3px solid #26313c;border-radius:6px;aspect-ratio:240/135;overflow:hidden;display:flex;align-items:center;justify-content:center}
.scr img{max-width:100%;max-height:100%;image-rendering:auto}
.row{display:grid;grid-template-columns:132px 1fr;gap:12px;align-items:center;padding:10px 0;border-top:1px solid var(--line)}
.row:first-child{border-top:0}
.meta b{display:block;word-break:break-all}
.meta .st{display:block;font-size:13px;color:var(--mute);margin:2px 0 6px}
.meta .st.ok{color:var(--ok)}.meta .st.bad{color:var(--bad)}
.bar{height:6px;background:#dde4ea;border-radius:3px;overflow:hidden}
.bar i{display:block;height:100%;width:0;background:var(--acc);transition:width .15s}
.acts{display:flex;gap:8px;flex-wrap:wrap}
.store{margin:8px 0 0;font-size:13px;color:var(--mute)}
.empty{color:var(--mute);padding:6px 0}
.cur{outline:2px solid var(--acc);outline-offset:2px}
@media(max-width:420px){.row{grid-template-columns:104px 1fr}.opts{grid-template-columns:1fr}}
</style></head><body><main>
<header>
  <h1>TTGO GIF<small>Layar 240 &times; 135</small></h1>
  <div id="net">memuat&hellip;</div>
</header>

<div class="card">
  <label class="drop" id="drop"><input type="file" id="file" accept="image/gif" multiple hidden>
    Pilih GIF atau seret ke sini<span>GIF otomatis diperkecil agar pas di layar sebelum dikirim</span></label>
  <div class="opts">
    <label class="chk"><input type="checkbox" id="oResize" checked> Resize otomatis</label>
    <label class="chk"><input type="checkbox" id="oUp" checked> Perbesar GIF kecil</label>
    <label>Penyesuaian
      <select id="oMode"><option value="fit">Pas layar (utuh)</option><option value="fill">Isi layar (potong tepi)</option><option value="stretch">Regangkan</option></select></label>
    <label>Maks. frame
      <input type="number" id="oMax" min="4" max="300" value="60"></label>
    <label class="chk"><input type="checkbox" id="oDither"> Dithering (gradasi lebih halus, file lebih besar)</label>
  </div>
  <div id="queue"></div>
</div>

<h2>GIF di perangkat</h2>
<div class="card">
  <div id="lib"><div class="empty">memuat&hellip;</div></div>
  <div class="bar" style="margin-top:12px"><i id="sbar"></i></div>
  <div class="store" id="stext"></div>
</div>
</main>
<script>
// <gifkit>
function lzwDecode(minCode, data, npix) {
  const out = new Uint8Array(npix);
  const clear = 1 << minCode, eoi = clear + 1;
  let size = minCode + 1, mask = (1 << size) - 1, next = eoi + 1;
  const prefix = new Uint16Array(4096), suffix = new Uint8Array(4096), stack = new Uint8Array(4097);
  for (let i = 0; i < clear; i++) suffix[i] = i;
  let bits = 0, acc = 0, pos = 0, op = 0, old = -1, first = 0;
  while (op < npix) {
    while (bits < size) {
      if (pos >= data.length) return out;
      acc |= data[pos++] << bits; bits += 8;
    }
    let code = acc & mask; acc >>>= size; bits -= size;
    if (code === clear) { size = minCode + 1; mask = (1 << size) - 1; next = eoi + 1; old = -1; continue; }
    if (code === eoi) break;
    if (old === -1) { out[op++] = suffix[code]; old = code; first = code; continue; }
    const inCode = code; let sp = 0;
    if (code >= next) { stack[sp++] = first; code = old; }
    while (code >= clear) { stack[sp++] = suffix[code]; code = prefix[code]; }
    first = suffix[code];
    stack[sp++] = first;
    while (sp > 0 && op < npix) out[op++] = stack[--sp];
    if (next < 4096) {
      prefix[next] = old; suffix[next] = first; next++;
      if (next > mask && size < 12) { size++; mask = (1 << size) - 1; }
    }
    old = inCode;
  }
  return out;
}

function scanGif(buf) {
  if (buf.length < 13) throw new Error('File terlalu kecil');
  const sig = String.fromCharCode(buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
  if (sig !== 'GIF87a' && sig !== 'GIF89a') throw new Error('Bukan file GIF');
  const w = buf[6] | (buf[7] << 8), h = buf[8] | (buf[9] << 8);
  let p = 10; const fl = buf[p]; p += 3;
  if (fl & 0x80) p += (2 << (fl & 7)) * 3;
  let count = 0;
  const skip = () => { while (p < buf.length) { const n = buf[p++]; if (!n) break; p += n; } };
  while (p < buf.length) {
    const b = buf[p++];
    if (b === 0x3B) break;
    if (b === 0x21) { p++; skip(); }
    else if (b === 0x2C) {
      p += 8; const f = buf[p++];
      if (f & 0x80) p += (2 << (f & 7)) * 3;
      p++; skip(); count++;
    } else break;
  }
  return { w, h, count };
}

// Generator: setiap frame sudah digabung (composited) ke kanvas penuh.
// rgba yang di-yield dipakai ulang, jadi harus langsung dikonsumsi.
function* gifFrames(buf) {
  const m = scanGif(buf), W = m.w, H = m.h;
  if (!W || !H || W * H > 16e6) throw new Error('Ukuran GIF tidak valid');
  let p = 10; const fl = buf[p]; p += 3;
  let gct = null;
  if (fl & 0x80) { const n = (2 << (fl & 7)) * 3; gct = buf.subarray(p, p + n); p += n; }
  const canvas = new Uint8ClampedArray(W * H * 4);
  let gce = null, prev = null, idx = 0;
  const skip = () => { while (p < buf.length) { const n = buf[p++]; if (!n) break; p += n; } };
  while (p < buf.length) {
    const b = buf[p++];
    if (b === 0x3B) break;
    if (b === 0x21) {
      const label = buf[p++];
      if (label === 0xF9 && buf[p] >= 4) {
        const pk = buf[p + 1], dl = buf[p + 2] | (buf[p + 3] << 8), ti = buf[p + 4];
        gce = { disposal: (pk >> 2) & 7, trans: (pk & 1) ? ti : -1, delay: dl * 10 };
      }
      skip();
      continue;
    }
    if (b !== 0x2C) break;
    const x = buf[p] | (buf[p + 1] << 8), y = buf[p + 2] | (buf[p + 3] << 8);
    const w = buf[p + 4] | (buf[p + 5] << 8), h = buf[p + 6] | (buf[p + 7] << 8);
    p += 8;
    const f = buf[p++]; let ct = gct;
    if (f & 0x80) { const n = (2 << (f & 7)) * 3; ct = buf.subarray(p, p + n); p += n; }
    const minCode = buf[p++];
    const chunks = []; let total = 0;
    while (p < buf.length) { const n = buf[p++]; if (!n) break; chunks.push(buf.subarray(p, p + n)); p += n; total += n; }
    const data = new Uint8Array(total); let o = 0;
    for (const c of chunks) { data.set(c, o); o += c.length; }
    const pix = lzwDecode(minCode, data, w * h);
    if (prev) {
      if (prev.disposal === 2) {
        for (let yy = prev.y; yy < prev.y + prev.h && yy < H; yy++)
          for (let xx = prev.x; xx < prev.x + prev.w && xx < W; xx++) {
            const q = (yy * W + xx) * 4; canvas[q] = canvas[q + 1] = canvas[q + 2] = canvas[q + 3] = 0;
          }
      } else if (prev.disposal === 3 && prev.saved) canvas.set(prev.saved);
    }
    const g = gce || { disposal: 0, trans: -1, delay: 100 };
    const saved = g.disposal === 3 ? canvas.slice() : null;
    if (ct) {
      const rowOf = new Int32Array(h);
      if (f & 0x40) {
        let r = 0;
        for (const [s0, st] of [[0, 8], [4, 8], [2, 4], [1, 2]]) for (let yy = s0; yy < h; yy += st) rowOf[r++] = yy;
      } else for (let r = 0; r < h; r++) rowOf[r] = r;
      for (let r = 0; r < h; r++) {
        const dy = y + rowOf[r]; if (dy >= H) continue;
        for (let c = 0; c < w; c++) {
          const dx = x + c; if (dx >= W) break;
          const v = pix[r * w + c];
          if (v === g.trans) continue;
          const t = v * 3; if (t + 2 >= ct.length) continue;
          const q = (dy * W + dx) * 4;
          canvas[q] = ct[t]; canvas[q + 1] = ct[t + 1]; canvas[q + 2] = ct[t + 2]; canvas[q + 3] = 255;
        }
      }
    }
    let d = g.delay; if (d <= 10) d = 100;
    yield { i: idx++, rgba: canvas, delay: d };
    prev = { disposal: g.disposal, x, y, w, h, saved };
    gce = null;
  }
}

function pickIndices(n, m) {
  const r = [];
  if (n <= m) { for (let i = 0; i < n; i++) r.push(i); return r; }
  for (let k = 0; k < m; k++) r.push(Math.floor(k * n / m));
  return r;
}

// frames: [{rgba, delay}] -> subset n frame, durasi total dipertahankan
function subsample(fr, n) {
  const sel = pickIndices(fr.length, n), out = [];
  for (let k = 0; k < sel.length; k++) {
    const end = k + 1 < sel.length ? sel[k + 1] : fr.length;
    let d = 0; for (let j = sel[k]; j < end; j++) d += fr[j].delay;
    out.push({ rgba: fr[sel[k]].rgba, delay: d });
  }
  return out;
}

// Median cut pada histogram RGB555. Mengembalikan {rgb: Uint8Array(768), n}
function buildPalette(rgbaList, maxColors) {
  const hist = new Uint32Array(32768);
  for (const f of rgbaList)
    for (let i = 0; i < f.length; i += 4) hist[((f[i] >> 3) << 10) | ((f[i + 1] >> 3) << 5) | (f[i + 2] >> 3)]++;
  const cols = [];
  for (let k = 0; k < 32768; k++) if (hist[k]) cols.push(k);
  const rgb = new Uint8Array(768);
  const put = (n, r5, g5, b5) => {
    rgb[n * 3] = Math.round(r5 * 255 / 31); rgb[n * 3 + 1] = Math.round(g5 * 255 / 31); rgb[n * 3 + 2] = Math.round(b5 * 255 / 31);
  };
  if (cols.length <= maxColors) {
    cols.forEach((k, n) => put(n, k >> 10, (k >> 5) & 31, k & 31));
    return { rgb, n: Math.max(cols.length, 1) };
  }
  const chan = (k, c) => c === 0 ? k >> 10 : c === 1 ? (k >> 5) & 31 : k & 31;
  const mk = (c) => {
    let pop = 0; const mn = [31, 31, 31], mx = [0, 0, 0];
    for (const k of c) {
      pop += hist[k];
      for (let ch = 0; ch < 3; ch++) { const v = chan(k, ch); if (v < mn[ch]) mn[ch] = v; if (v > mx[ch]) mx[ch] = v; }
    }
    let ch = 0, rg = mx[0] - mn[0];
    for (let i = 1; i < 3; i++) if (mx[i] - mn[i] > rg) { rg = mx[i] - mn[i]; ch = i; }
    return { c, pop, ch, score: pop * (rg + 1) };
  };
  let boxes = [mk(cols)];
  while (boxes.length < maxColors) {
    let bi = -1, best = -1;
    for (let i = 0; i < boxes.length; i++) if (boxes[i].c.length > 1 && boxes[i].score > best) { best = boxes[i].score; bi = i; }
    if (bi < 0) break;
    const b = boxes[bi], ch = b.ch;
    b.c.sort((a, c) => chan(a, ch) - chan(c, ch));
    let acc = 0, cut = 1;
    for (let i = 0; i < b.c.length - 1; i++) { acc += hist[b.c[i]]; cut = i + 1; if (acc * 2 >= b.pop) break; }
    boxes.splice(bi, 1, mk(b.c.slice(0, cut)), mk(b.c.slice(cut)));
  }
  boxes.forEach((b, n) => {
    let r = 0, g = 0, bl = 0, pop = 0;
    for (const k of b.c) { const w = hist[k]; r += (k >> 10) * w; g += ((k >> 5) & 31) * w; bl += (k & 31) * w; pop += w; }
    put(n, r / pop, g / pop, bl / pop);
  });
  return { rgb, n: boxes.length };
}

function buildLut(pal) {
  const lut = new Uint8Array(32768);
  for (let k = 0; k < 32768; k++) {
    const r = ((k >> 10) << 3) | ((k >> 12) & 7), g = (((k >> 5) & 31) << 3) | (((k >> 7) & 7)), b = ((k & 31) << 3) | ((k >> 2) & 7);
    let best = 0, bd = 1e9;
    for (let i = 0; i < pal.n; i++) {
      const dr = r - pal.rgb[i * 3], dg = g - pal.rgb[i * 3 + 1], db = b - pal.rgb[i * 3 + 2];
      const d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
      if (d < bd) { bd = d; best = i; }
    }
    lut[k] = best;
  }
  return lut;
}

const BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5];
// Dither ordered (posisi-deterministik) supaya piksel statis tetap identik antar-frame
function quantizeFrame(rgba, w, h, lut, dither) {
  const out = new Uint8Array(w * h);
  let i = 0, o = 0;
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++, i += 4, o++) {
    let r = rgba[i], g = rgba[i + 1], b = rgba[i + 2];
    if (dither) {
      const t = (BAYER[((y & 3) << 2) | (x & 3)] - 7.5) * dither;
      r += t; g += t; b += t;
      r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b;
    }
    out[o] = lut[((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)];
  }
  return out;
}

function lzwEncode(pix, minCode) {
  const clear = 1 << minCode, eoi = clear + 1;
  const out = [];
  let cur = 0, shift = 0, size = minCode + 1, next = eoi + 1;
  let dict = new Map();
  const emit = (c) => {
    cur |= c << shift; shift += size;
    while (shift >= 8) { out.push(cur & 255); cur >>>= 8; shift -= 8; }
  };
  emit(clear);
  let ib = pix[0];
  for (let i = 1; i < pix.length; i++) {
    const k = pix[i], key = (ib << 8) | k, c = dict.get(key);
    if (c === undefined) {
      emit(ib);
      if (next === 4096) { emit(clear); size = minCode + 1; next = eoi + 1; dict = new Map(); }
      else { if (next >= (1 << size)) size++; dict.set(key, next++); }
      ib = k;
    } else ib = c;
  }
  emit(ib);
  if (next < 4096 && next >= (1 << size)) size++;
  emit(eoi);
  if (shift > 0) out.push(cur & 255);
  return out;
}

// frames: [{idx: Uint8Array(w*h), delay}] ; hanya bagian yang berubah yang disimpan
function encodeGif(w, h, frames, pal) {
  const TR = 255, list = [];
  let prev = null;
  for (const f of frames) {
    if (!prev) {
      list.push({ x: 0, y: 0, w, h, data: f.idx, t: false, delay: f.delay });
    } else {
      let x0 = w, y0 = h, x1 = -1, y1 = -1;
      for (let y = 0; y < h; y++) for (let x = 0; x < w; x++)
        if (f.idx[y * w + x] !== prev[y * w + x]) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
      if (x1 < 0) { list[list.length - 1].delay += f.delay; continue; }
      const fw = x1 - x0 + 1, fh = y1 - y0 + 1, data = new Uint8Array(fw * fh);
      for (let y = 0; y < fh; y++) for (let x = 0; x < fw; x++) {
        const a = f.idx[(y0 + y) * w + x0 + x];
        data[y * fw + x] = a === prev[(y0 + y) * w + x0 + x] ? TR : a;
      }
      list.push({ x: x0, y: y0, w: fw, h: fh, data, t: true, delay: f.delay });
    }
    prev = f.idx;
  }
  const b = [];
  const u16 = (v) => { b.push(v & 255, (v >> 8) & 255); };
  for (const ch of 'GIF89a') b.push(ch.charCodeAt(0));
  u16(w); u16(h); b.push(0xF7, 0, 0);
  for (let i = 0; i < 768; i++) b.push(pal.rgb[i]);
  b.push(0x21, 0xFF, 0x0B); for (const ch of 'NETSCAPE2.0') b.push(ch.charCodeAt(0));
  b.push(3, 1, 0, 0, 0);
  for (const f of list) {
    const cs = Math.min(65535, Math.max(2, Math.round(f.delay / 10)));
    b.push(0x21, 0xF9, 4, (1 << 2) | (f.t ? 1 : 0)); u16(cs); b.push(TR, 0);
    b.push(0x2C); u16(f.x); u16(f.y); u16(f.w); u16(f.h); b.push(0);
    b.push(8);
    const lz = lzwEncode(f.data, 8);
    for (let i = 0; i < lz.length; i += 255) {
      const n = Math.min(255, lz.length - i); b.push(n);
      for (let j = 0; j < n; j++) b.push(lz[i + j]);
    }
    b.push(0);
  }
  b.push(0x3B);
  return Uint8Array.from(b);
}
// </gifkit>

(function () {
'use strict';
const SW = 240, SH = 135;
const $ = (s) => document.querySelector(s);
const fmt = (n) => n < 1024 ? n + ' B' : n < 1048576 ? (n / 1024).toFixed(1) + ' KB' : (n / 1048576).toFixed(2) + ' MB';
const tick = () => new Promise((r) => setTimeout(r, 0));
const LS = {
  get(k, d) { try { const v = localStorage.getItem('gp.' + k); return v === null ? d : JSON.parse(v); } catch (e) { return d; } },
  set(k, v) { try { localStorage.setItem('gp.' + k, JSON.stringify(v)); } catch (e) {} }
};
const opt = { resize: LS.get('resize', true), up: LS.get('up', true), mode: LS.get('mode', 'fit'), max: LS.get('max', 60), dither: LS.get('dither', false) };
$('#oResize').checked = opt.resize; $('#oUp').checked = opt.up; $('#oMode').value = opt.mode; $('#oMax').value = opt.max; $('#oDither').checked = opt.dither;
$('#oResize').onchange = (e) => { opt.resize = e.target.checked; LS.set('resize', opt.resize); };
$('#oUp').onchange = (e) => { opt.up = e.target.checked; LS.set('up', opt.up); };
$('#oMode').onchange = (e) => { opt.mode = e.target.value; LS.set('mode', opt.mode); };
$('#oMax').onchange = (e) => { opt.max = Math.max(4, Math.min(300, +e.target.value || 60)); e.target.value = opt.max; LS.set('max', opt.max); };
$('#oDither').onchange = (e) => { opt.dither = e.target.checked; LS.set('dither', opt.dither); };

let info = null;
async function loadInfo() {
  const r = await fetch('/api/info', { cache: 'no-store' });
  if (!r.ok) throw new Error('Perangkat tidak merespons');
  info = await r.json();
  drawInfo();
}

function drawInfo() {
  const n = $('#net'); n.textContent = '';
  n.append((info.ap ? 'Mode AP: ' : 'WiFi: ') + info.ssid, document.createElement('br'), info.ip + ' · ');
  const a = document.createElement('a'); a.href = '/wifi'; a.textContent = 'ganti WiFi'; n.append(a);
  const used = info.used, total = info.total;
  $('#sbar').style.width = Math.min(100, used * 100 / total) + '%';
  $('#stext').textContent = 'Terpakai ' + fmt(used) + ' dari ' + fmt(total) + ' · ' + info.files.length + ' GIF' +
    (info.bat > 2800 ? ' · baterai ' + (info.bat / 1000).toFixed(2) + ' V' : '');
  const lib = $('#lib'); lib.textContent = '';
  if (!info.files.length) { const d = document.createElement('div'); d.className = 'empty'; d.textContent = 'Belum ada GIF. Pilih file di atas.'; lib.append(d); return; }
  info.files.forEach((f, i) => {
    const row = document.createElement('div'); row.className = 'row';
    row.innerHTML = '<div class="scr' + (i === info.cur ? ' cur' : '') + '"><img loading="lazy" alt=""></div><div class="meta"><b></b><span class="st"></span><div class="acts"><button class="alt">Putar</button><button class="del">Hapus</button></div></div>';
    row.querySelector('img').src = '/api/file?name=' + encodeURIComponent(f.n) + '&t=' + f.s;
    row.querySelector('b').textContent = f.n;
    row.querySelector('.st').textContent = fmt(f.s) + (i === info.cur ? ' · sedang tampil' : '');
    row.querySelector('.alt').onclick = async () => { await fetch('/api/show?name=' + encodeURIComponent(f.n), { method: 'POST' }); setTimeout(loadInfo, 700); };
    row.querySelector('.del').onclick = async () => {
      if (!confirm('Hapus ' + f.n + '?')) return;
      await fetch('/api/delete?name=' + encodeURIComponent(f.n), { method: 'POST' }); setTimeout(loadInfo, 500);
    };
    lib.append(row);
  });
}

function safeName(n) {
  let b = n.replace(/\.[^.]*$/, '').toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_+|_+$/g, '').slice(0, 20);
  return (b || 'gif') + '.gif';
}

function target(w, h) {
  if (opt.mode === 'stretch') return { dw: SW, dh: SH, dx: 0, dy: 0, sw: SW, sh: SH, s: Math.min(SW / w, SH / h) };
  if (opt.mode === 'fill') {
    const s = Math.max(SW / w, SH / h), sw = w * s, sh = h * s;
    return { dw: SW, dh: SH, dx: (SW - sw) / 2, dy: (SH - sh) / 2, sw, sh, s };
  }
  const s = Math.min(SW / w, SH / h, opt.up ? Infinity : 1);
  const dw = Math.max(1, Math.round(w * s)), dh = Math.max(1, Math.round(h * s));
  return { dw, dh, dx: 0, dy: 0, sw: dw, sh: dh, s };
}

async function prepare(file, budget, job) {
  if (budget < 12288) throw new Error('Penyimpanan penuh, hapus GIF lain dulu');
  const buf = new Uint8Array(await file.arrayBuffer());
  const m = scanGif(buf);
  if (!m.count) throw new Error('GIF tidak berisi frame');
  const T = target(m.w, m.h);
  const sel = pickIndices(m.count, opt.max);
  const src = document.createElement('canvas'); src.width = m.w; src.height = m.h;
  const sx = src.getContext('2d');
  const dst = document.createElement('canvas'); dst.width = T.dw; dst.height = T.dh;
  const dx = dst.getContext('2d', { willReadFrequently: true });
  const smooth = !(T.s >= 1 && Number.isInteger(T.s));
  let frames = [], sp = 0;
  for (const f of gifFrames(buf)) {
    if (sp < sel.length && f.i === sel[sp]) {
      sx.putImageData(new ImageData(f.rgba, m.w, m.h), 0, 0);
      dx.fillStyle = '#000'; dx.fillRect(0, 0, T.dw, T.dh);
      dx.imageSmoothingEnabled = smooth; dx.imageSmoothingQuality = 'high';
      dx.drawImage(src, T.dx, T.dy, T.sw, T.sh);
      frames.push({ rgba: dx.getImageData(0, 0, T.dw, T.dh).data, delay: f.delay });
      sp++;
    } else if (frames.length) frames[frames.length - 1].delay += f.delay;
    if (f.i % 3 === 0) { job.set('Memperkecil frame ' + (f.i + 1) + '/' + m.count + '…'); job.prog((f.i + 1) / m.count); await tick(); }
  }
  job.set('Menyusun palet warna & kompresi…'); await tick();
  const total0 = frames.length;
  let bytes;
  for (;;) {
    const pal = buildPalette(frames.map((f) => f.rgba), 255);
    const lut = buildLut(pal);
    const idx = frames.map((f) => ({ idx: quantizeFrame(f.rgba, T.dw, T.dh, lut, opt.dither ? 5 : 0), delay: f.delay }));
    bytes = encodeGif(T.dw, T.dh, idx, pal);
    if (bytes.length <= budget || frames.length <= 4) break;
    const n = Math.max(4, Math.min(frames.length - 1, Math.floor(frames.length * budget / bytes.length * 0.9)));
    frames = subsample(frames, n);
    job.set('Terlalu besar, mengurangi frame ke ' + n + '…'); await tick();
  }
  if (bytes.length > budget) throw new Error('Hasil ' + fmt(bytes.length) + ' melebihi sisa penyimpanan');
  return { blob: new Blob([bytes], { type: 'image/gif' }), note: m.w + '×' + m.h + ' → ' + T.dw + '×' + T.dh + ' · ' + frames.length + (frames.length < m.count ? '/' + m.count : '') + ' frame · ' + fmt(file.size) + ' → ' + fmt(bytes.length), cut: frames.length < total0 };
}

function send(name, blob, onp) {
  return new Promise((res, rej) => {
    const x = new XMLHttpRequest();
    x.open('POST', '/api/upload');
    x.upload.onprogress = (e) => { if (e.lengthComputable) onp(e.loaded / e.total); };
    x.onload = () => {
      if (x.status === 200) return res();
      let m = 'Upload gagal (' + x.status + ')'; try { m = JSON.parse(x.responseText).err || m; } catch (e) {}
      rej(new Error(m));
    };
    x.onerror = () => rej(new Error('Koneksi ke perangkat terputus'));
    const fd = new FormData(); fd.append('gif', blob, name); x.send(fd);
  });
}

function makeJob(file) {
  const el = document.createElement('div'); el.className = 'row';
  el.innerHTML = '<div class="scr"><img alt=""></div><div class="meta"><b></b><span class="st">Menunggu…</span><div class="bar"><i></i></div></div>';
  el.querySelector('b').textContent = file.name;
  $('#queue').prepend(el);
  const st = el.querySelector('.st'), bar = el.querySelector('.bar i'), img = el.querySelector('img');
  return {
    file,
    set(t, c) { st.textContent = t; st.className = 'st' + (c ? ' ' + c : ''); },
    prog(p) { bar.style.width = Math.round(p * 100) + '%'; },
    img(u) { img.src = u; }
  };
}

async function runJob(j) {
  const f = j.file, name = safeName(f.name);
  await loadInfo();
  const same = info.files.find((x) => x.n === name);
  const budget = info.total - info.used + (same ? same.s : 0) - 20480;
  let blob, note;
  if (opt.resize) { j.set('Membaca GIF…'); const r = await prepare(f, budget, j); blob = r.blob; note = r.note; j.img(URL.createObjectURL(blob)); }
  else {
    if (f.size > budget) throw new Error('Penyimpanan tidak cukup (sisa ' + fmt(Math.max(0, budget)) + ')');
    blob = f; note = fmt(f.size) + ' dikirim apa adanya'; j.img(URL.createObjectURL(f));
  }
  j.set('Mengunggah ' + fmt(blob.size) + '…'); j.prog(0);
  await send(name, blob, (p) => j.prog(p));
  j.prog(1); j.set('Selesai · ' + note, 'ok');
}

const queue = []; let busy = false;
async function pump() {
  if (busy) return; busy = true;
  while (queue.length) {
    const j = queue.shift();
    try { await runJob(j); } catch (e) { j.set('Gagal: ' + e.message, 'bad'); }
  }
  busy = false;
  try { await loadInfo(); } catch (e) {}
}
function addFiles(list) {
  for (const f of list) {
    if (!/\.gif$/i.test(f.name) && f.type !== 'image/gif') { const j = makeJob(f); j.set('Bukan file GIF', 'bad'); continue; }
    queue.push(makeJob(f));
  }
  pump();
}
const drop = $('#drop');
$('#file').onchange = (e) => { addFiles(e.target.files); e.target.value = ''; };
['dragenter', 'dragover'].forEach((t) => drop.addEventListener(t, (e) => { e.preventDefault(); drop.classList.add('on'); }));
['dragleave', 'drop'].forEach((t) => drop.addEventListener(t, (e) => { e.preventDefault(); drop.classList.remove('on'); }));
drop.addEventListener('drop', (e) => addFiles(e.dataTransfer.files));
loadInfo().catch((e) => { $('#net').textContent = e.message; });
})();
</script></body></html>
)HTML";

// ---------------------------------------------------------------------------
//  Halaman pengaturan WiFi
// ---------------------------------------------------------------------------
static const char WIFI_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="id"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WiFi · TTGO GIF</title>
<style>
body{margin:0;background:#e8edf1;color:#121c26;font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
main{max-width:480px;margin:0 auto;padding:18px 14px}
h1{font-size:24px;margin:6px 0 4px;letter-spacing:-.02em}
p{color:#5b6b7a;margin:4px 0 14px}
.card{background:#fff;border:1px solid #c9d3dc;border-radius:10px;padding:14px;margin-bottom:14px}
label{display:block;font-size:13px;color:#5b6b7a;margin:10px 0 4px}
input{font:inherit;padding:9px;border:1px solid #c9d3dc;border-radius:6px;width:100%;box-sizing:border-box}
button{font:inherit;font-weight:600;padding:9px 14px;border:0;border-radius:6px;background:#1f4bff;color:#fff;margin-top:12px;cursor:pointer}
button.alt{background:#e3e9ef;color:#121c26}
button.del{background:#fbe6e4;color:#c0352b}
.net{display:flex;justify-content:space-between;width:100%;background:#f1f5f8;color:#121c26;margin:6px 0 0;font-weight:500}
a{color:#1f4bff}
#msg{font-weight:600;margin-top:12px}
</style></head><body><main>
<h1>Pengaturan WiFi</h1>
<p>Hubungkan TTGO ke WiFi yang sama dengan HP/PC kamu. Alamat IP akan tampil di layar TTGO (klik tombol atas).</p>
<div class="card">
  <button class="alt" id="scan" style="margin-top:0">Cari jaringan</button>
  <div id="nets"></div>
  <label for="ssid">Nama WiFi (SSID)</label><input id="ssid" autocomplete="off" autocapitalize="off">
  <label for="pass">Kata sandi</label><input id="pass" type="password" autocomplete="off">
  <button id="save">Simpan &amp; sambungkan</button>
  <div id="msg"></div>
</div>
<div class="card">
  <button class="del" id="reset" style="margin-top:0">Lupakan WiFi tersimpan</button>
  <p style="margin-bottom:0">TTGO restart ke mode AP (<b>TTGO-GIF</b>, sandi <b>12345678</b>, alamat 192.168.4.1).</p>
</div>
<a href="/">&larr; Kembali ke GIF</a>
<script>
const $=s=>document.querySelector(s);
$('#scan').onclick=async()=>{
  const b=$('#scan');b.textContent='Mencari…';
  try{
    const l=await (await fetch('/api/wifi/scan')).json();
    const box=$('#nets');box.textContent='';
    l.forEach(n=>{const x=document.createElement('button');x.className='alt net';x.textContent='';
      const a=document.createElement('span');a.textContent=n.s;const c=document.createElement('span');c.textContent=n.r+' dBm'+(n.o?'':' 🔒');
      x.append(a,c);x.onclick=()=>{$('#ssid').value=n.s;$('#pass').focus()};box.append(x)});
    if(!l.length)box.textContent='Tidak ada jaringan ditemukan.';
  }catch(e){$('#nets').textContent='Gagal mencari jaringan.'}
  b.textContent='Cari jaringan';
};
$('#save').onclick=async()=>{
  const s=$('#ssid').value.trim();if(!s){$('#msg').textContent='Isi nama WiFi dulu.';return}
  await fetch('/api/wifi/save',{method:'POST',body:new URLSearchParams({ssid:s,pass:$('#pass').value})});
  $('#msg').textContent='Tersimpan. TTGO restart; sambungkan HP/PC ke WiFi yang sama lalu buka IP di layar TTGO.';
};
$('#reset').onclick=async()=>{
  if(!confirm('Lupakan WiFi tersimpan dan restart?'))return;
  await fetch('/api/wifi/reset',{method:'POST'});
  $('#msg').textContent='WiFi dihapus. TTGO restart ke mode AP.';
};
</script></main></body></html>
)HTML";
