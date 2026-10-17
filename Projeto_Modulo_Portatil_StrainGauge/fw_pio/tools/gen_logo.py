# Gera src/logo.c: logo 120x60 RGB565 (splash) — ponte de Wheatstone estilizada
import numpy as np
from PIL import Image, ImageDraw, ImageFont
import matplotlib, os

ttf = os.path.join(matplotlib.get_data_path(), 'fonts', 'ttf', 'DejaVuSans-Bold.ttf')
W, H = 120, 60
im = Image.new('RGB', (W, H))
d = ImageDraw.Draw(im)
# fundo degradê azul-escuro
a = np.zeros((H, W, 3), dtype=np.uint8)
for y in range(H):
    t = y / H
    a[y, :] = (int(8+20*t), int(20+40*t), int(60+60*t))
im = Image.fromarray(a)
d = ImageDraw.Draw(im)
d.rectangle([1, 1, W-2, H-2], outline=(255, 200, 0))
# ponte de Wheatstone (losango c/ 4 braços) à esquerda
cx, cy, r = 30, 30, 16
pts = [(cx, cy-r), (cx+r, cy), (cx, cy+r), (cx-r, cy)]
d.line(pts + [pts[0]], fill=(0, 255, 255), width=2)
for p in pts:
    d.rectangle([p[0]-3, p[1]-3, p[0]+3, p[1]+3], fill=(255, 255, 255))
d.line([(cx-r-6, cy), (cx-r, cy)], fill=(255, 200, 0), width=2)
d.line([(cx+r, cy), (cx+r+6, cy)], fill=(255, 200, 0), width=2)
# textos
f1 = ImageFont.truetype(ttf, 22)
f2 = ImageFont.truetype(ttf, 9)
d.text((56, 12), 'SG-6', fill=(255, 255, 255), font=f1)
d.text((56, 38), 'STRAIN DAQ', fill=(0, 255, 255), font=f2)
d.text((56, 48), '24-bit  6ch', fill=(180, 180, 180), font=f2)

px = np.array(im)
with open(os.path.join(os.path.dirname(__file__), '..', 'src', 'logo.c'), 'w') as f:
    f.write('/* gerado por tools/gen_logo.py */\n#include <stdint.h>\n')
    f.write('const uint16_t logo_rgb565[120*60] = {\n')
    line = []
    for y in range(H):
        for x in range(W):
            r, g, b = px[y, x]
            v = ((int(r) & 0xF8) << 8) | ((int(g) & 0xFC) << 3) | (int(b) >> 3)
            line.append(f'0x{v:04X}')
        f.write(','.join(line) + ',\n')
        line = []
    f.write('};\n')
print('logo.c OK')
