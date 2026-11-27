#include "gfx/text.h"
#include "gfx/utf8.h"
#include "hal/panel.h"
#include "config/app_config.h"

TextMetrics text_measure(const Font *f, const char *s)
{
    TextMetrics m = {};
    uint32_t ink = 0;                       // OR tất cả các cột -> biết hàng nào có mực
    const FontGlyph *last = nullptr;

    while (*s) {
        const uint32_t cp = utf8_next(&s);
        if (utf8_is_combining(cp)) continue;
        last = font_glyph(f, cp);
        m.width += last->width;
        for (int i = 0; i < last->width; i++) ink |= f->cols[last->offset + i];
    }
    // bỏ cột trống ở cuối ký tự cuối -> căn giữa chính xác
    if (last)
        for (int i = last->width - 1; i >= 0 && f->cols[last->offset + i] == 0; i--) m.width--;

    // Chữ không dấu luôn dùng khung ASCII (dòng chữ không nhảy lên xuống);
    // có dấu tiếng Việt thì mở rộng thêm
    m.top    = f->ascii_top;
    m.bottom = f->ascii_bottom;
    if (ink) {
        const int t = __builtin_ctz(ink), b = 31 - __builtin_clz(ink);
        if (t < m.top)    m.top = t;
        if (b > m.bottom) m.bottom = b;
    }
    m.height = m.bottom - m.top + 1;
    return m;
}

void text_draw(int x, int y, const Font *f, const char *s, const Paint &paint, int top_row, int view_x)
{
    const int left = view_x, right = view_x + PANEL_W;     // vùng hiện trên panel này (tọa độ ảo)
    while (*s && x < right) {
        const uint32_t cp = utf8_next(&s);
        if (utf8_is_combining(cp)) continue;
        const FontGlyph *gl = font_glyph(f, cp);
        if (x + gl->width <= left) { x += gl->width; continue; }  // còn ngoài mép trái

        for (int col = 0; col < gl->width; col++) {
            const int px = x + col;
            if (px >= right) break;
            uint32_t bits = f->cols[gl->offset + col] >> top_row;
            if (bits == 0 || px < left) continue;
            const Rgb c = paint_color_at(paint, px);
            for (int row = 0; bits; row++, bits >>= 1)
                if (bits & 1) panel_set_pixel(px - view_x, y + row, c.r, c.g, c.b);
        }
        x += gl->width;
    }
}

void text_draw_color(int x, int y, const Font *f, const char *s, Rgb color)
{
    const Paint p = { FX_SOLID, color, 0 };
    text_draw(x, y, f, s, p, text_measure(f, s).top);
}
