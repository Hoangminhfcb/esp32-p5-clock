"""Sinh bitmap đồng hồ ở kích thước gốc; không kéo giãn glyph lúc hiển thị.

Chạy: python tools/gen_clock_font.py (không cần thư viện ngoài)
Font bitmap Spleen 8x16 và 6x12 có sẵn trong tools/gen_fonts/font_src.
"""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'tools/gen_fonts/font_src'
# Spleen 8x16 bitmap có sẵn (BSD-2-Clause), phóng nguyên pixel 2x.
# Chữ số có phần nét 7x10 trong ô 8x16 -> bitmap 14x20 nét đậm.
large = []
bdf_large = (SRC / 'spleen-8x16.bdf').read_text()
for digit in '0123456789':
    block = next(b for b in bdf_large.split('STARTCHAR ')[1:] if re.search(r'^ENCODING '+str(ord(digit))+r'$', b, re.M))
    source = [int(line,16) for line in block.split('BITMAP\n')[1].split('ENDCHAR')[0].splitlines()][2:12]
    rows = []
    for row in source:
        assert row & 1 == 0  # Cột trống cuối ô font gốc.
        doubled = sum(3 << (12-2*x) for x in range(7) if row & (128 >> x))
        rows.extend((doubled,doubled))
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
       '// Giờ: Spleen 8x16, phóng nguyên pixel 2x (BSD-2-Clause).',
       '// Lịch: Spleen 6x12, Frederic Cambus (BSD-2-Clause).',
       '#pragma once', '#include <stdint.h>',
       'static const uint16_t CLOCK_DIGITS[10][20] = {']
for rows in large:
    out.append('    {'+', '.join(hex(v) for v in rows)+'},')
out += ['};', 'static const char CLOCK_SMALL_CODES[] = "'+chars+'";',
        'static const uint8_t CLOCK_SMALL[][8] = {']
for rows in small:
    out.append('    {'+', '.join(hex(v) for v in rows)+'},')
out += ['};', '']
(ROOT / 'main/gfx/clock_font.h').write_text('\n'.join(out),encoding='utf-8')
print('Generated Spleen pixel 14x20 clock digits and 5x8 calendar glyphs')
