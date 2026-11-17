# TTGO GIF Player (LilyGo T-Display)

Pemutar GIF untuk LilyGo T-Display (ESP32, layar 240×135). GIF diunggah lewat web UI,
dan **diperkecil otomatis di browser** supaya pas di layar sebelum dikirim.

## Cara pakai
1. Flash firmware (lihat bawah). Saat pertama nyala TTGO membuat WiFi **TTGO-GIF** (sandi `12345678`).
2. Sambungkan HP/PC ke WiFi itu, buka `http://192.168.4.1/wifi`, pilih WiFi rumah, isi sandi, simpan.
   (Bisa juga langsung pakai mode AP tanpa router: upload GIF di `http://192.168.4.1/`.)
3. TTGO restart dan masuk ke WiFi yang sama. IP tampil di layar beberapa detik
   (atau klik tombol atas). Buka `http://<IP>` atau `http://ttgogif.local`.
4. Pilih/seret GIF di web UI → otomatis di-resize, dikompres, diunggah, lalu langsung tampil.

## Tombol
| Tombol | Aksi |
|---|---|
| Bawah (GPIO0) | Slide / GIF berikutnya |
| Atas (GPIO35) klik | Info: IP, jumlah GIF, baterai |
| Atas (GPIO35) dobel klik (< 450 ms) | Power off (deep sleep) |
| Atas (GPIO35) klik saat mati | Nyalakan |

## Opsi web UI
- **Resize otomatis**: matikan untuk mengirim GIF apa adanya (harus sudah ≤ 240×135 dan kecil).
- **Penyesuaian**: pas layar (utuh) / isi layar (potong tepi) / regangkan.
- **Maks. frame**: jika GIF punya frame lebih banyak, frame dipilih merata (durasi total dijaga).
  Bila hasil melebihi sisa penyimpanan (~2 MB), frame dikurangi otomatis.
- **Dithering**: gradasi lebih halus, ukuran file lebih besar.

## Build lewat GitHub
Push ke GitHub → tab **Actions** → job *Build firmware* → unduh artifact `ttgo-gif-player`:
- `ttgo-gif-merged.bin` — satu file, flash di alamat `0x0`
- `bootloader.bin`, `partitions.bin`, `ttgo-gif-app.bin` — terpisah (0x1000, 0x8000, 0x10000)

Flash: `esptool.py --chip esp32 --baud 460800 write_flash 0x0 ttgo-gif-merged.bin`
atau lewat web flasher esptool (alamat 0x0). Build lokal: `pio run -t upload`.

## Uji encoder GIF (opsional)
```
python3 tests/gen_samples.py && node tests/test_gifkit.js && python3 tests/verify_pillow.py
python3 tests/strict_eoi.py /tmp/enc_*.gif
```
