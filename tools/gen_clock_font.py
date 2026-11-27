"""Sinh bitmap đồng hồ ở kích thước gốc; không kéo giãn glyph lúc hiển thị.

Chạy: python tools/gen_clock_font.py (cần Pillow)
Giờ lấy từ mẫu số 8 của người dùng; chữ nhỏ dùng bitmap Spleen 6x12.
"""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'tools/gen_fonts/font_src'
# Tách 7 đoạn từ chính số 8 do người dùng cung cấp (alpha là phần nét).
from PIL import Image
source=Image.open(SRC / 'pixil-digit-8.png').getchannel('A')
box=source.getbbox();glyph=source.crop(box)
assert glyph.size==(12,21)
segments=[[0]*21 for _ in range(7)]
for y in range(21):
    for x in range(12):
        if not glyph.getpixel((x,y)): continue
        if y<=2 and 2<=x<=9: segment=0
        elif 9<=y<=11 and 2<=x<=9: segment=6
        elif 18<=y<=20 and 2<=x<=9: segment=3
        elif y<=9: segment=5 if x<3 else 1
        else: segment=4 if x<3 else 2
        segments[segment][y] |= 1<<(11-x)
large=[]
for mask in (0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f):
    rows=[0]*21
    for segment in range(7):
        if mask&(1<<segment):
            rows=[a|b for a,b in zip(rows,segments[segment])]
    large.append(rows)
# Số 8 phải khớp từng pixel với ảnh gốc sau khi bỏ vùng trong suốt.
assert large[8]==[sum(1<<(11-x) for x in range(12) if glyph.getpixel((x,y))) for y in range(21)]
# Chữ lịch pixel gốc 5x8, từ BDF Spleen 6x12 (BSD-2-Clause).
bdf = (SRC / 'spleen-6x12.bdf').read_text()
chars = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:/.- '
small = []
for char in chars:
    block = next(b for b in bdf.split('STARTCHAR ')[1:] if re.search(r'^ENCODING '+str(ord(char))+r'$', b, re.M))
    rows = [int(line,16) >> 3 for line in block.split('BITMAP\n')[1].split('ENDCHAR')[0].splitlines()]
    small.append(rows[1:9])
out = ['// Sinh bằng tools/gen_clock_font.py. Không sửa tay.',
       '// Giờ: tách 7 đoạn từ pixil-digit-8.png do người dùng cung cấp.',
       '// Lịch: Spleen 6x12, Frederic Cambus (BSD-2-Clause).',
       '#pragma once', '#include <stdint.h>',
       'static const uint16_t CLOCK_DIGITS[10][21] = {']
for rows in large:
    out.append('    {'+', '.join(hex(v) for v in rows)+'},')
out += ['};', 'static const char CLOCK_SMALL_CODES[] = "'+chars+'";',
        'static const uint8_t CLOCK_SMALL[][8] = {']
for rows in small:
    out.append('    {'+', '.join(hex(v) for v in rows)+'},')
out += ['};', '']
(ROOT / 'main/gfx/clock_font.h').write_text('\n'.join(out),encoding='utf-8')
print('Generated reference-matched 12x21 digits and Spleen calendar glyphs')
