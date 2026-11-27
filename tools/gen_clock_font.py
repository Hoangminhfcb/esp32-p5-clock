"""Sinh bitmap đồng hồ ở kích thước gốc; không kéo giãn glyph lúc hiển thị.

Chạy: PYTHONPATH=build/python-packages python tools/gen_clock_font.py
Nguồn: DejaVu Sans ExtraLight và Spleen 6x12 có sẵn trong tools/gen_fonts/font_src.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import re
ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'tools/gen_fonts/font_src'
font = ImageFont.truetype(str(SRC / 'DejaVuSans-ExtraLight.ttf'), 26)
large = []
for char in '0123456789':
    im = Image.new('1', (64, 64))
    draw = ImageDraw.Draw(im)
    draw.fontmode = '1'
    draw.text((8, 0), char, font=font, fill=1)
    box = im.getbbox()
    glyph = im.crop(box)
    assert glyph.width <= 13 and glyph.height == 19
    rows = []
    left = (13 - glyph.width) // 2
    for y in range(19):
        rows.append(sum(1 << (12-x-left) for x in range(glyph.width) if glyph.getpixel((x,y))))
    # Thêm một pixel ngang vào nét, giới hạn trong ô 13 px.
    rows = [row | (row >> 1) for row in rows]
    large.append(rows)
# Chữ lịch pixel gốc 5x8, từ BDF Spleen 6x12 (BSD-2-Clause).
bdf = (SRC / 'spleen-6x12.bdf').read_text()
chars = '0123456789ACFGILNOTWY:/.- '
small = []
for char in chars:
    block = next(b for b in bdf.split('STARTCHAR ')[1:] if re.search(r'^ENCODING '+str(ord(char))+r'$', b, re.M))
    rows = [int(line,16) >> 3 for line in block.split('BITMAP\n')[1].split('ENDCHAR')[0].splitlines()]
    small.append(rows[1:9])
out = ['// Sinh bằng tools/gen_clock_font.py. Không sửa tay.',
       '// Giờ: DejaVu Sans ExtraLight, thêm nét 1 px (Bitstream Vera/DejaVu).',
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
print('Generated native 13x19 clock digits and 5x8 calendar glyphs')
