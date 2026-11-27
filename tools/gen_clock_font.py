"""Sinh bitmap đồng hồ ở kích thước gốc; không kéo giãn glyph lúc hiển thị.

Chạy: python tools/gen_clock_font.py (cần Pillow)
Silkscreen Bold và bitmap Spleen 6x12 có sẵn trong tools/gen_fonts/font_src.
"""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'tools/gen_fonts/font_src'
# Silkscreen Bold có sẵn (SIL OFL 1.1), raster trực tiếp ở cỡ 20.
# Ô số 13x13, giữ cạnh pixel sắc, không kéo giãn.
from PIL import Image, ImageDraw, ImageFont
font = ImageFont.truetype(str(SRC / 'Silkscreen-Bold.ttf'),20)
large = []
images=[]
for digit in '0123456789':
    im=Image.new('1',(64,64));draw=ImageDraw.Draw(im);draw.fontmode='1'
    draw.text((4,0),digit,font=font,fill=1);images.append(im)
top=min(im.getbbox()[1] for im in images)
for im in images:
    box=im.getbbox();glyph=im.crop(box)
    assert glyph.width<=13 and box[3]-top<=13
    cell=Image.new('1',(13,13));cell.paste(glyph,((13-glyph.width)//2,box[1]-top))
    large.append([sum(1<<(12-x) for x in range(13) if cell.getpixel((x,y))) for y in range(13)])
# Chữ lịch pixel gốc 5x8, từ BDF Spleen 6x12 (BSD-2-Clause).
bdf = (SRC / 'spleen-6x12.bdf').read_text()
chars = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:/.- '
small = []
for char in chars:
    block = next(b for b in bdf.split('STARTCHAR ')[1:] if re.search(r'^ENCODING '+str(ord(char))+r'$', b, re.M))
    rows = [int(line,16) >> 3 for line in block.split('BITMAP\n')[1].split('ENDCHAR')[0].splitlines()]
    small.append(rows[1:9])
out = ['// Sinh bằng tools/gen_clock_font.py. Không sửa tay.',
       '// Giờ: Silkscreen Bold, Jason Kottke (SIL OFL 1.1).',
       '// Lịch: Spleen 6x12, Frederic Cambus (BSD-2-Clause).',
       '#pragma once', '#include <stdint.h>',
       'static const uint16_t CLOCK_DIGITS[10][13] = {']
for rows in large:
    out.append('    {'+', '.join(hex(v) for v in rows)+'},')
out += ['};', 'static const char CLOCK_SMALL_CODES[] = "'+chars+'";',
        'static const uint8_t CLOCK_SMALL[][8] = {']
for rows in small:
    out.append('    {'+', '.join(hex(v) for v in rows)+'},')
out += ['};', '']
(ROOT / 'main/gfx/clock_font.h').write_text('\n'.join(out),encoding='utf-8')
print('Generated Silkscreen Bold pixel 13x13 clock digits and 5x8 calendar glyphs')
