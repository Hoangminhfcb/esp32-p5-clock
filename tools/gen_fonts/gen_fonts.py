# =====================================================================
#  gen_fonts.py — Sinh font bitmap cho panel -> main/gfx/fonts/
#
#    python tools/gen_fonts/gen_fonts.py            (cần: pip install pillow fonttools)
#    python tools/gen_fonts/gen_fonts.py --preview  (thêm ảnh preview.png)
#
#  Mỗi KIỂU CHỮ (family) được render ở mọi cỡ từ SIZE_MIN..SIZE_MAX.
#  "Cỡ" = chiều cao chữ IN HOA tính bằng pixel -> mỗi bước tăng đúng 1 px.
#  Tiếng Việt: cỡ nhỏ tự ghép dấu (vietnamese.py) cho rõ; cỡ lớn dùng dấu của font.
#  Thêm kiểu chữ: thêm 1 dòng vào FAMILIES — xem tools/gen_fonts/README.md
# =====================================================================
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from fontlib import px_for_cap, render_ttf      # noqa: E402
from vietnamese import VN_CHARS, Composer       # noqa: E402

S = os.path.join(HERE, "font_src") + "/"
O = os.path.join(HERE, "..", "..", "main", "gfx", "fonts") + "/"

ASCII = [chr(c) for c in range(0x20, 0x7F)]
assert len(VN_CHARS) == 134

# Cỡ chữ (chiều cao chữ hoa, px)
SIZE_MIN, SIZE_MAX = 6, 20
NATIVE_VN_FROM = 12          # từ cỡ này trở lên dùng dấu tiếng Việt có sẵn của font

DEJAVU = "Bitstream Vera / DejaVu (tự do, xem font_src/LICENSE-DejaVu.txt)"
# Mỗi kiểu chữ: (id, tên hiển thị, nguồn theo cỡ, nét mảnh?, license)
#   nguồn theo cỡ = [(từ cỡ, file TTF), ...] — cho phép dùng file khác nhau ở cỡ nhỏ / lớn.
#   Vd "Mỏng": DejaVu Sans thường giữ nét 1px tới cỡ 12; từ 13 nét bị dày lên
#   nên chuyển sang DejaVu Sans ExtraLight để vẫn mảnh.
# THỨ TỰ = id kiểu chữ, chỉ thêm vào CUỐI
FAMILIES = [
    ("bold", "Đậm",  [(6, "DejaVuSans-Bold.ttf")],                                   False, DEJAVU),
    ("thin", "Mỏng", [(6, "DejaVuSans.ttf"), (13, "DejaVuSans-ExtraLight.ttf")],     True,  DEJAVU),
]

PAD_TOP, PAD_BOT = 8, 2


def pad(glyphs):
    return {ch: ([[0] * adv for _ in range(PAD_TOP)] + [r[:] for r in rows] + [[0] * adv for _ in range(PAD_BOT)], adv)
            for ch, (rows, adv) in glyphs.items()}


def ttf_for(sources, cap):
    path = sources[0][1]
    for from_cap, f in sources:
        if cap >= from_cap:
            path = f
    return S + path


def build(path, px, cap, thin):
    g = pad(render_ttf(path, px, ASCII + VN_CHARS)[0])
    comp = Composer({c: g[c] for c in ASCII}, max_cls=1 if thin else 2)
    out = {c: g[c] for c in ASCII}
    for ch in VN_CHARS:
        out[ch] = g[ch] if (cap >= NATIVE_VN_FROM and ch in g) else comp.compose(ch)
    return out


def crop(glyphs):
    """Cắt hàng trống chung; trả về (H, cols theo ký tự, ascii_top, ascii_bottom)."""
    H0 = len(next(iter(glyphs.values()))[0])
    used = [any(r[y][x] for r, a in glyphs.values() for x in range(a)) for y in range(H0)]
    t = used.index(True)
    b = H0 - used[::-1].index(True)
    H = b - t
    cols = {}
    for ch, (rows, adv) in glyphs.items():
        cs = []
        for x in range(adv):
            v = 0
            for y in range(t, b):
                if rows[y][x]:
                    v |= 1 << (y - t)
            cs.append(v)
        cols[ch] = cs
    m = 0
    for c in ASCII:
        for v in cols[c]:
            m |= v
    return H, cols, (m & -m).bit_length() - 1, m.bit_length() - 1


def write_font(sym, title, lic, H, cols, at, ab):
    allc, gl = [], []
    for ch in ASCII + VN_CHARS:
        gl.append((len(allc), len(cols[ch]), ch))
        allc += cols[ch]
    L = [f"// {title}  |  cao {H} px (chữ ASCII: hàng {at}..{ab})  |  License: {lic}",
         "// Tự động sinh bởi tools/gen_fonts/gen_fonts.py — KHÔNG sửa tay.",
         "#pragma once", '#include "gfx/font.h"', "",
         f"static const uint32_t {sym}_cols[] = {{"]
    for i in range(0, len(allc), 10):
        L.append("    " + ", ".join(f"0x{v:08X}" for v in allc[i:i + 10]) + ",")
    L += ["};", "", f"static const FontGlyph {sym}_glyphs[] = {{"]
    for i in range(0, len(gl), 8):
        L.append("    " + " ".join(f"{{{o},{w}}}," for o, w, _ in gl[i:i + 8]))
    L += ["};", "", f"static const Font FONT_{sym.upper()} = {{ {H}, {at}, {ab}, {sym}_glyphs, {sym}_cols }};", ""]
    return "\n".join(L)


def write_registry(reg):
    inc = "\n".join(f'#include "font_{fid}.h"' for fid, *_ in reg)
    tables, fams = [], []
    for fid, name, sizes in reg:
        entries = ",\n".join(f"    &FONT_{fid.upper()}_{s}" for s in sizes)
        tables.append(f"static const Font *const {fid.upper()}_SIZES[] = {{\n{entries},\n}};")
        fams.append(f'    {{ "{name}", {sizes[0]}, {len(sizes)}, {fid.upper()}_SIZES }},')
    cs = [f"0x{ord(c):04X}" for c in VN_CHARS]
    codes = ",\n    ".join(", ".join(cs[i:i + 12]) for i in range(0, len(cs), 12))
    open(O + "fonts.cpp", "w", encoding="utf-8").write(f'''// Danh sách kiểu chữ. File này tự sinh bởi tools/gen_fonts/gen_fonts.py — KHÔNG sửa tay.
#include "gfx/font.h"
{inc}

// Mã Unicode của các ký tự mở rộng (sau ASCII), sắp xếp tăng dần — dùng chung mọi font
const uint16_t FONT_EXT_CODES[FONT_EXT_COUNT] = {{
    {codes}
}};

{chr(10).join(tables)}

static const FontFamily FAMILIES[] = {{
{chr(10).join(fams)}
}};

int font_family_count(void) {{ return sizeof(FAMILIES) / sizeof(FAMILIES[0]); }}

const FontFamily *font_family(int id)
{{
    if (id < 0 || id >= font_family_count()) id = 0;
    return &FAMILIES[id];
}}
''')


def preview(rows, path):
    from PIL import Image, ImageDraw
    sample = "Xin chào ĐẮK LẮK - Ước mơ 2026"
    sc = 3
    ims = []
    for label, H, cols in rows:
        w = sum(len(cols[c]) for c in sample)
        im = Image.new("RGB", (w * sc + 40, H * sc + 6), (18, 18, 18))
        d = ImageDraw.Draw(im)
        d.text((2, 2), label, fill=(140, 140, 140))
        x = 0
        for c in sample:
            for cx, v in enumerate(cols[c]):
                for y in range(H):
                    if v >> y & 1:
                        X = 36 + (x + cx) * sc
                        d.rectangle([X, 3 + y * sc, X + sc - 2, 3 + y * sc + sc - 2], fill=(255, 200, 0))
            x += len(cols[c])
        ims.append(im)
    out = Image.new("RGB", (max(i.width for i in ims), sum(i.height + 4 for i in ims)))
    y = 0
    for i in ims:
        out.paste(i, (0, y))
        y += i.height + 4
    out.save(path)


if __name__ == "__main__":
    for f in os.listdir(O):                            # xóa font cũ
        if f.startswith("font_") and f.endswith(".h"):
            os.remove(O + f)
    reg, prev, total = [], [], 0
    for fid, name, sources, thin, lic in FAMILIES:
        parts, sizes = [], []
        for cap in range(SIZE_MIN, SIZE_MAX + 1):
            ttf = ttf_for(sources, cap)
            px = px_for_cap(ttf, cap)
            if px is None:
                sys.exit(f"[{fid}] không có cỡ px nào cho chữ hoa cao {cap}px — đổi SIZE_MIN/MAX")
            H, cols, at, ab = crop(build(ttf, px, cap, thin))
            if H > 32:
                print(f"[{fid}] cỡ {cap}: cao {H}px > 32 -> dừng ở cỡ {cap - 1}")
                break
            parts.append(write_font(f"{fid}_{cap}", f"{name} cỡ {cap} ({os.path.basename(ttf)} {px}px)", lic, H, cols, at, ab))
            sizes.append(cap)
            prev.append((f"{fid} {cap}", H, cols))
            total += sum(len(v) for v in cols.values()) * 4 + len(cols) * 4
            print(f"{fid:7s} cỡ {cap:2d}: px={px:2d} H={H:2d} ascii={at}..{ab}")
        open(O + f"font_{fid}.h", "w", encoding="utf-8").write("\n".join(parts))
        reg.append((fid, name, sizes))
    write_registry(reg)
    print(f"Tổng dữ liệu font ~ {total // 1024} KB flash")
    if "--preview" in sys.argv:
        preview(prev, os.path.join(HERE, "preview.png"))
