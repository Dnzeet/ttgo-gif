# Decoder LZW ketat: pastikan kode EOI terbaca pada ukuran bit yang benar di akhir setiap frame
import sys
def lzw_strict(data, min_code, npix):
    clear = 1 << min_code; eoi = clear + 1
    size = min_code + 1; nxt = eoi + 1
    table = {i: bytes([i]) for i in range(clear)}
    bits = 0; acc = 0; pos = 0; out = bytearray(); old = None
    while True:
        while bits < size:
            if pos >= len(data): return out, False
            acc |= data[pos] << bits; pos += 1; bits += 8
        code = acc & ((1 << size) - 1); acc >>= size; bits -= size
        if code == clear:
            table = {i: bytes([i]) for i in range(clear)}; size = min_code + 1; nxt = eoi + 1; old = None; continue
        if code == eoi: return out, True
        if old is None:
            out += table[code]; old = code; continue
        if code in table: entry = table[code]
        elif code == nxt: entry = table[old] + table[old][:1]
        else: raise Exception('kode tak valid %d (next %d)' % (code, nxt))
        out += entry
        if nxt < 4096:
            table[nxt] = table[old] + entry[:1]; nxt += 1
            if nxt == (1 << size) and size < 12: size += 1
        old = code

def frames(b):
    p = 13; fl = b[10]
    if fl & 0x80: p += 3 * (2 << (fl & 7))
    while b[p] != 0x3B:
        if b[p] == 0x21:
            p += 2
            while b[p]: p += b[p] + 1
            p += 1
        elif b[p] == 0x2C:
            w = b[p+5] | b[p+6] << 8; h = b[p+7] | b[p+8] << 8; f = b[p+9]; p += 10
            if f & 0x80: p += 3 * (2 << (f & 7))
            mc = b[p]; p += 1; data = bytearray()
            while b[p]: data += b[p+1:p+1+b[p]]; p += b[p] + 1
            p += 1
            yield mc, bytes(data), w * h
bad = 0; total = 0
for name in sys.argv[1:]:
    b = open(name, 'rb').read()
    for mc, data, npix in frames(b):
        out, eoi_ok = lzw_strict(data, mc, npix)
        total += 1
        if len(out) != npix or not eoi_ok:
            bad += 1; print('MASALAH', name, len(out), npix, eoi_ok)
print('frame diperiksa:', total, 'bermasalah:', bad)
