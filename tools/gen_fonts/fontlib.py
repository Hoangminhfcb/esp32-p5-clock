# Render font TTF thành bitmap từng ký tự (không khử răng cưa, có hinting).
#   render_ttf() trả về (glyphs, baseline):
#     glyphs  : { ký_tự: (rows, adv) }  rows = list các hàng (list 0/1), cùng chiều cao
#     baseline: số hàng nằm phía trên chân chữ
from PIL import Image, ImageDraw, ImageFont


def cap_height(path, px):
    """Chiều cao chữ 'H' (pixel) khi render ở cỡ px."""
    f = ImageFont.truetype(path, px)
    im = Image.new("1", (px * 3, px * 3), 0)
    d = ImageDraw.Draw(im)
    d.fontmode = "1"
    d.text((px, px * 2), "H", font=f, fill=1, anchor="ls")
    bb = im.getbbox()
    return bb[3] - bb[1] if bb else 0


def px_for_cap(path, cap):
    """Cỡ chữ (px) cho chiều cao chữ hoa đúng bằng cap. None nếu hinting nhảy qua."""
    for px in range(cap, cap * 3):
        h = cap_height(path, px)
        if h == cap:
            return px
        if h > cap:
            return None
    return None


def render_ttf(path, px, chars):
    from fontTools.ttLib import TTFont
    cmap = TTFont(path, lazy=True).getBestCmap()
    f = ImageFont.truetype(path, px)
    H, base = px * 3, px * 2
    out = {}
    for ch in chars:
        if ord(ch) not in cmap:
            continue
        adv = round(f.getlength(ch))
        im = Image.new("1", (adv + px * 2, H), 0)
        d = ImageDraw.Draw(im)
        d.fontmode = "1"
        d.text((px, base), ch, font=f, fill=1, anchor="ls")
        out[ch] = ([[im.getpixel((px + x, y)) for x in range(adv)] for y in range(H)], adv)
    return out, base
