# Membuat GIF contoh (interlaced, transparan, disposal 2/3) untuk menguji decoder/encoder
import random, json
from PIL import Image, ImageDraw
random.seed(7)

def save(name, frames, **kw):
    frames[0].save(f"/tmp/{name}.gif", save_all=True, append_images=frames[1:], loop=0, **kw)

# 1) animasi gradasi + kotak bergerak (banyak warna, LZW table overflow)
fr = []
for i in range(12):
    im = Image.new("RGB", (97, 61))
    px = im.load()
    for y in range(61):
        for x in range(97):
            px[x, y] = ((x * 3 + i * 9) % 256, (y * 4 + i * 5) % 256, (x * y + i * 20) % 256)
    d = ImageDraw.Draw(im); d.rectangle([5 + i * 6, 10, 25 + i * 6, 40], fill=(255, 255, 0))
    fr.append(im)
save("grad", fr, duration=[30, 50, 80, 100, 120, 20, 40, 40, 40, 40, 40, 200])

# 2) transparansi + disposal 2 (restore bg) dan 3 (restore previous)
fr = []
for i in range(6):
    im = Image.new("RGBA", (50, 40), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse([i * 5, 5, i * 5 + 15, 20], fill=(255, 0, 0, 255))
    d.rectangle([10, 25 + i, 30, 30 + i], fill=(0, 200, 255, 255))
    fr.append(im)
save("trans2", fr, duration=70, disposal=2, transparency=0)
save("trans3", fr, duration=70, disposal=3, transparency=0)

# 3) random noise besar -> uji kode 12 bit penuh berulang
noise = [Image.frombytes("RGB", (120, 90), bytes(random.randrange(256) for _ in range(120*90*3))) for _ in range(3)]
save("noise", noise, duration=100)
