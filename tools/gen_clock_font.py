"""Sinh bitmap đồng hồ ở kích thước gốc; không kéo giãn glyph lúc hiển thị.

Chạy: python tools/gen_clock_font.py (cần Pillow)
Giờ dùng DSEG7 Classic Bold; chữ nhỏ dùng bitmap Spleen 6x12.
"""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'tools/gen_fonts/font_src'
# Font DSEG7 Classic Bold chính thức, raster nguyên tỷ lệ ở cỡ 19.
from PIL import Image,ImageDraw,ImageFont
font=ImageFont.truetype(str(SRC / 'DSEG7Classic-Bold.ttf'),19)
images=[]
for digit in '0123456789':
    im=Image.new('1',(64,64));draw=ImageDraw.Draw(im);draw.fontmode='1'
    draw.text((8,8),digit,font=font,fill=1);images.append(im)
left=min(im.getbbox()[0] for im in images)
top=min(im.getbbox()[1] for im in images)
large=[]
for im in images:
    box=im.getbbox()
    assert box[2]-left<=12 and box[3]-top<=19
    cell=im.crop((left,top,left+12,top+19))
    large.append([sum(1<<(11-x) for x in range(12) if cell.getpixel((x,y))) for y in range(19)])
# Chữ lịch pixel gốc 5x8, từ BDF Spleen 6x12 (BSD-2-Clause).
bdf = (SRC / 'spleen-6x12.bdf').read_text()
chars = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:/.- '
small = []
for char in chars:
    block = next(b for b in bdf.split('STARTCHAR ')[1:] if re.search(r'^ENCODING '+str(ord(char))+r'$', b, re.M))
    rows = [int(line,16) >> 3 for line in block.split('BITMAP\n')[1].split('ENDCHAR')[0].splitlines()]
    small.append(rows[1:9])
out = ['// Sinh bằng tools/gen_clock_font.py. Không sửa tay.',
       '// Giờ: bitmap từ DSEG7 Classic Bold, keshikan (SIL OFL 1.1).',
       '// Lịch: Spleen 6x12, Frederic Cambus (BSD-2-Clause).',
       '#pragma once', '#include <stdint.h>',
       'static const uint16_t CLOCK_DIGITS[10][19] = {']
for rows in large:
    out.append('    {'+', '.join(hex(v) for v in rows)+'},')
out += ['};', 'static const char CLOCK_SMALL_CODES[] = "'+chars+'";',
        'static const uint8_t CLOCK_SMALL[][8] = {']
for rows in small:
    out.append('    {'+', '.join(hex(v) for v in rows)+'},')
out += ['};', '']
(ROOT / 'main/gfx/clock_font.h').write_text('\n'.join(out),encoding='utf-8')
print('Generated official DSEG7 Classic Bold 12x19 digits and Spleen calendar glyphs')
