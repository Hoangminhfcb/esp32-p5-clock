"""Sinh bitmap đồng hồ ở kích thước gốc; không kéo giãn glyph lúc hiển thị.

Chạy: PYTHONPATH=build/python-packages python tools/gen_clock_font.py
Số rounded tự thiết kế; chữ nhỏ từ Spleen 6x12 trong tools/gen_fonts/font_src.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import re
ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'tools/gen_fonts/font_src'
# Số rounded tự thiết kế trong ô 14x19, nét 3 px và đầu nét tròn.
# Dựng đường cong ở độ phân giải 4x rồi lấy mẫu về đúng lưới LED.
SCALE = 4
large = []
def curve(p0,p1,p2,p3):
    return [tuple((1-t)**3*p0[i]+3*(1-t)**2*t*p1[i]+3*(1-t)*t*t*p2[i]+t**3*p3[i] for i in (0,1)) for t in [j/32 for j in range(33)]]
def stroke(draw, points):
    points=[(round(x*SCALE),round(y*SCALE)) for x,y in points]
    draw.line(points,fill=255,width=3*SCALE,joint='curve')
    radius=1.5*SCALE
    for x,y in (points[0],points[-1]):
        draw.ellipse((x-radius,y-radius,x+radius,y+radius),fill=255)
for digit in range(10):
    im=Image.new('L',(14*SCALE,19*SCALE))
    d=ImageDraw.Draw(im)
    if digit==0:
        d.ellipse((1*SCALE,0,12*SCALE,18*SCALE),outline=255,width=3*SCALE)
    elif digit==1:
        stroke(d,[(3,4),(7,1.5),(7,16.5)])
        stroke(d,[(3,16.5),(11,16.5)])
    elif digit==2:
        stroke(d,curve((2.5,4),(3,-0.5),(11.5,-0.5),(11,5))+curve((11,5),(11,8),(4,11),(2,16.5))+[(11,16.5)])
    elif digit==3:
        stroke(d,curve((2.5,2.5),(10,-1),(14,7),(6,8.5))+curve((6,8.5),(14,8),(13,20),(2.5,16)))
    elif digit==4:
        stroke(d,[(9,1.5),(2,11),(11,11)])
        stroke(d,[(9,1.5),(9,16.5)])
    elif digit==5:
        stroke(d,[(10.5,1.5),(3,1.5),(2.5,8)]+curve((2.5,8),(14,4),(15,21),(2,16)))
    elif digit==6:
        stroke(d,curve((10,2),(2,-0.5),(0,17),(6.5,17))+curve((6.5,17),(14,18),(14,6),(6.5,8))+curve((6.5,8),(3,8),(2,10),(2,12)))
    elif digit==7:
        stroke(d,[(2,1.5),(11,1.5),(5,16.5)])
    elif digit==8:
        d.ellipse((1.5*SCALE,0,11.5*SCALE,9.5*SCALE),outline=255,width=3*SCALE)
        d.ellipse((1*SCALE,7.5*SCALE,12*SCALE,18*SCALE),outline=255,width=3*SCALE)
    else:
        stroke(d,curve((3,16),(11,19),(14,1),(7,1.5))+curve((7,1.5),(-1,0.5),(-1,13),(7,10))+curve((7,10),(10,10),(11,8),(11,6)))
    glyph=im.resize((14,19),Image.Resampling.LANCZOS)
    rows=[sum(1<<(13-x) for x in range(14) if glyph.getpixel((x,y))>=128) for y in range(19)]
    large.append(rows)
# Chữ lịch pixel gốc 5x8, từ BDF Spleen 6x12 (BSD-2-Clause).
bdf = (SRC / 'spleen-6x12.bdf').read_text()
chars = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:/.- '
small = []
for char in chars:
    block = next(b for b in bdf.split('STARTCHAR ')[1:] if re.search(r'^ENCODING '+str(ord(char))+r'$', b, re.M))
    rows = [int(line,16) >> 3 for line in block.split('BITMAP\n')[1].split('ENDCHAR')[0].splitlines()]
    small.append(rows[1:9])
out = ['// Sinh bằng tools/gen_clock_font.py. Không sửa tay.',
       '// Giờ: rounded tự thiết kế, nét tròn 3 px.',
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
print('Generated rounded 14x19 clock digits and 5x8 calendar glyphs')
