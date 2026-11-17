# Bandingkan hasil JS dengan Pillow (decoder independen)
import json, base64
from PIL import Image
import numpy as np

dec = json.load(open('/tmp/js_decoded.json'))
bad = 0
for n, d in dec.items():
    im = Image.open(f'/tmp/{n}.gif')
    assert im.n_frames == d['count'], (n, im.n_frames, d['count'])
    for i in range(im.n_frames):
        im.seek(i)
        ref = np.array(im.convert('RGBA'))
        js = np.frombuffer(base64.b64decode(d['frames'][i]['px']), dtype=np.uint8).reshape(d['h'], d['w'], 4)
        # bandingkan: alpha sama; RGB sama di piksel buram
        a_ref = ref[..., 3] > 0; a_js = js[..., 3] > 0
        mism_alpha = int((a_ref != a_js).sum())
        mism_rgb = int((np.abs(ref[..., :3].astype(int) - js[..., :3].astype(int)).sum(-1) * (a_ref & a_js) > 0).sum())
        if mism_alpha or mism_rgb:
            bad += 1; print('BEDA decode', n, 'frame', i, 'alpha', mism_alpha, 'rgb', mism_rgb)
    print('decode', n, 'frame', im.n_frames, 'selesai')

exp = json.load(open('/tmp/js_expected.json'))
for k, e in exp.items():
    n, dith = k.rsplit('_', 1)
    im = Image.open(f'/tmp/enc_{k}.gif')
    pal = np.frombuffer(base64.b64decode(e['pal']), dtype=np.uint8).reshape(256, 3)
    frames = e['frames']
    assert im.n_frames == len(frames), (k, im.n_frames, len(frames))
    for i, f in enumerate(frames):
        im.seek(i)
        got = np.array(im.convert('RGB'))
        idx = np.frombuffer(base64.b64decode(f['i']), dtype=np.uint8).reshape(e['h'], e['w'])
        want = pal[idx]
        diff = int((got != want).any(-1).sum())
        if diff:
            bad += 1; print('BEDA encode', k, 'frame', i, diff)
        if im.info.get('duration') != f['d'] and abs(im.info.get('duration', 0) - f['d']) > 10:
            print('delay beda', k, i, im.info.get('duration'), f['d'])
    print('encode', k, 'OK' if not bad else 'ada beda', 'frame', len(frames), 'loop', im.info.get('loop'))
print('TOTAL BEDA:', bad)
