# -*- coding: utf-8 -*-
"""Converte um PNG (logo) para:
  1) header C com array RGB565 (para Flash, logo pequeno <= ~14 KB), e
  2) BMP 24 bits sem compressao (para carregar do SD no splash).
Uso:  python logo_para_rgb565.py logo.png [largura_max]
Requer Pillow:  pip install pillow
Sem Pillow: se a entrada ja for BMP 24-bit, copia/renomeia para logo_splash.bmp.
"""
import sys, struct, os

def bmp24_save(w, h, px, path):
    # px: lista de (r,g,b) linha de cima p/ baixo -> BMP eh de baixo p/ cima
    row_size = (w * 3 + 3) & ~3
    pix_size = row_size * h
    with open(path, 'wb') as f:
        f.write(b'BM')
        f.write(struct.pack('<IHHI', 54 + pix_size, 0, 0, 54))
        f.write(struct.pack('<IiiHHIIiiII', 40, w, h, 1, 24, 0, pix_size, 2835, 2835, 0, 0))
        for y in range(h - 1, -1, -1):
            row = bytearray()
            for x in range(w):
                r, g, b = px[y * w + x]
                row += bytes((b, g, r))
            row += b'\x00' * (row_size - len(row))
            f.write(row)

def main():
    if len(sys.argv) < 2:
        print(__doc__); return
    src = sys.argv[1]
    maxw = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    out_c = os.path.splitext(os.path.basename(src))[0] + '_rgb565.h'
    out_bmp = 'logo_splash.bmp'
    try:
        from PIL import Image
        im = Image.open(src).convert('RGB')
        if maxw and im.width > maxw:
            im = im.resize((maxw, int(im.height * maxw / im.width)))
        w, h = im.size
        px = list(im.getdata())
    except ImportError:
        if src.lower().endswith('.bmp'):
            import shutil
            shutil.copy(src, out_bmp)
            print('Pillow nao instalado: BMP copiado como', out_bmp)
            print('Para header C, instale Pillow: pip install pillow')
            return
        raise
    # BMP p/ SD
    bmp24_save(w, h, px, out_bmp)
    # header C RGB565 (big-endian por pixel, ordem padrao ILI9341)
    with open(out_c, 'w') as f:
        f.write('// gerado por logo_para_rgb565.py\n')
        f.write(f'#define LOGO_W {w}\n#define LOGO_H {h}\n')
        f.write(f'const uint16_t logo_rgb565[{w*h}] = {{\n')
        n = 0
        for (r, g, b) in px:
            v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            f.write(f'0x{v:04X},')
            n += 1
            if n % 12 == 0:
                f.write('\n')
        f.write('};\n')
    kb = w * h * 2 / 1024
    print(f'OK: {w}x{h} px  ->  {out_c} ({kb:.1f} KB em Flash) + {out_bmp} (SD)')
    if kb > 14:
        print('AVISO: >14 KB nao cabe confortavel no Flash do C8T6; use o BMP do SD no splash.')

if __name__ == '__main__':
    main()
