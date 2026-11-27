#include "text_renderer.h"
#include "display.h"
#include "app_config.h"

// ---------------------------------------------------------------------
//  UTF-8 + tim ky tu trong font
// ---------------------------------------------------------------------
uint32_t utf8_next(const char **ps)
{
    const unsigned char *s = (const unsigned char *)*ps;
    uint32_t cp;
    int extra;
    if (s[0] < 0x80)              { cp = s[0];        extra = 0; }
    else if ((s[0] & 0xE0) == 0xC0) { cp = s[0] & 0x1F; extra = 1; }
    else if ((s[0] & 0xF0) == 0xE0) { cp = s[0] & 0x0F; extra = 2; }
    else if ((s[0] & 0xF8) == 0xF0) { cp = s[0] & 0x07; extra = 3; }
    else { *ps += 1; return '?'; }
    for (int i = 1; i <= extra; i++) {
        if ((s[i] & 0xC0) != 0x80) { *ps += i; return '?'; }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *ps += 1 + extra;
    return cp;
}

static inline bool is_combining(uint32_t cp) { return cp >= 0x0300 && cp <= 0x036F; }

static const FontGlyph *glyph_of(const Font *f, uint32_t cp)
{
    if (cp >= FONT_ASCII_FIRST && cp <= FONT_ASCII_LAST)
        return &f->glyphs[cp - FONT_ASCII_FIRST];
    int lo = 0, hi = FONT_EXT_COUNT - 1;                 // tim nhi phan
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (FONT_EXT_CODES[mid] == cp) return &f->glyphs[FONT_ASCII_COUNT + mid];
        if (FONT_EXT_CODES[mid] < cp) lo = mid + 1; else hi = mid - 1;
    }
    return &f->glyphs['?' - FONT_ASCII_FIRST];
}

// ---------------------------------------------------------------------
//  Do kich thuoc
// ---------------------------------------------------------------------
TextMetrics text_measure(const Font *f, const char *s, int scale)
{
    TextMetrics m = {};
    uint32_t ink = 0;
    const FontGlyph *last = nullptr;
    while (*s) {
        uint32_t cp = utf8_next(&s);
        if (is_combining(cp)) continue;
        last = glyph_of(f, cp);
        m.width += last->width;
        for (int i = 0; i < last->width; i++) ink |= f->cols[last->offset + i];
    }
    if (last)        // bo cot trong o cuoi -> can giua chinh xac
        for (int i = last->width - 1; i >= 0 && f->cols[last->offset + i] == 0; i--) m.width--;

    // Chu khong dau luon dung khung ASCII (dong chu khong nhay len xuong);
    // co dau tieng Viet thi mo rong them
    m.top = f->ascii_top;
    m.bottom = f->ascii_bottom;
    if (ink) {
        int t = __builtin_ctz(ink), b = 31 - __builtin_clz(ink);
        if (t < m.top) m.top = t;
        if (b > m.bottom) m.bottom = b;
    }
    m.width *= scale;
    m.height = (m.bottom - m.top + 1) * scale;
    return m;
}

// ---------------------------------------------------------------------
//  Mau
// ---------------------------------------------------------------------
void hue_to_rgb(uint8_t hue, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t region = hue / 43, rem = (hue - region * 43) * 6;
    uint8_t up = rem, down = 255 - rem;
    switch (region) {
        case 0:  *r = 255;  *g = up;   *b = 0;    break;
        case 1:  *r = down; *g = 255;  *b = 0;    break;
        case 2:  *r = 0;    *g = 255;  *b = up;   break;
        case 3:  *r = 0;    *g = down; *b = 255;  break;
        case 4:  *r = up;   *g = 0;    *b = 255;  break;
        default: *r = 255;  *g = 0;    *b = down; break;
    }
}

// ---------------------------------------------------------------------
//  Ve
// ---------------------------------------------------------------------
void draw_text(int x, int y, const Font *f, const char *s, int scale, const Paint &p, int top_row)
{
    uint8_t r = p.r, g = p.g, b = p.b;
    if (p.effect == FX_CYCLE) hue_to_rgb(p.hue, &r, &g, &b);

    while (*s && x < PANEL_W) {
        uint32_t cp = utf8_next(&s);
        if (is_combining(cp)) continue;
        const FontGlyph *gl = glyph_of(f, cp);
        if (x + gl->width * scale <= 0) { x += gl->width * scale; continue; }   // ngoai man hinh trai

        for (int col = 0; col < gl->width; col++) {
            const int px = x + col * scale;
            if (px >= PANEL_W) break;
            uint32_t bits = f->cols[gl->offset + col] >> top_row;
            if (px + scale <= 0 || bits == 0) continue;
            if (p.effect == FX_RAINBOW) {                  // mau theo vi tri cot tren man hinh
                int sx = px < 0 ? 0 : px;
                hue_to_rgb((uint8_t)(p.hue + sx * 256 / PANEL_W), &r, &g, &b);
            }
            for (int row = 0; bits; row++, bits >>= 1) {
                if (!(bits & 1)) continue;
                const int py = y + row * scale;
                for (int dx = 0; dx < scale; dx++)
                    for (int dy = 0; dy < scale; dy++)
                        display->drawPixelRGB888(px + dx, py + dy, r, g, b);
            }
        }
        x += gl->width * scale;
    }
}

void draw_text_color(int x, int y, const Font *f, const char *s, uint8_t r, uint8_t g, uint8_t b, int scale)
{
    Paint p = { FX_SOLID, r, g, b, 0 };
    TextMetrics m = text_measure(f, s, scale);
    draw_text(x, y, f, s, scale, p, m.top);
}
