"""Sinh bitmap đồng hồ ở kích thước gốc; không kéo giãn glyph lúc hiển thị.

Chạy: python tools/gen_clock_font.py (chỉ cần thư viện chuẩn)
Giờ dùng 7 đoạn chuẩn; chữ nhỏ dùng bitmap Spleen 6x12.
"""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'tools/gen_fonts/font_src'
# Chữ lịch pixel gốc 5x8, từ BDF Spleen 6x12 (BSD-2-Clause).
bdf = (SRC / 'spleen-6x12.bdf').read_text()
chars = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:/.- '
small = []
for char in chars:
    block = next(b for b in bdf.split('STARTCHAR ')[1:] if re.search(r'^ENCODING '+str(ord(char))+r'$', b, re.M))
    rows = [int(line,16) >> 3 for line in block.split('BITMAP\n')[1].split('ENDCHAR')[0].splitlines()]
    small.append(rows[1:9])
out = ['// Sinh bằng tools/gen_clock_font.py. Không sửa tay.',
       '// Chữ lịch: Spleen 6x12, Frederic Cambus (BSD-2-Clause).',
       '#pragma once', '#include <stdint.h>',
       'static const char CLOCK_SMALL_CODES[] = "'+chars+'";',
       'static const uint8_t CLOCK_SMALL[][8] = {']
for rows in small:
    out.append('    {'+', '.join(hex(v) for v in rows)+'},')
out += ['};', '']
(ROOT / 'main/gfx/clock_font.h').write_text('\n'.join(out),encoding='utf-8')
print('Generated Spleen 5x8 calendar glyphs; time uses standard seven segments')
