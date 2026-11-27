#include "app/screen.h"
#include "config/app_config.h"
#include "gfx/font.h"
#include "gfx/text.h"
#include "gfx/color.h"
#include "hal/panel.h"

#define HUE_STEP_MS  20       // chu kỳ đổi pha màu
#define HUE_LINE_OFS 64       // lệch pha màu giữa 2 hàng cho đẹp

struct LineState {
    const LineSettings *cfg;
    const Font *font;
    TextMetrics m;
    int     x, y;
    bool    scroll;
    int     period;          // chạy chữ: độ rộng chữ + khoảng cách 1 tab
};

static DisplaySettings s_cfg;
static LineState       s_line[MAX_LINES];
static uint8_t         s_hue;
static int64_t         s_epoch;
static int             s_canvas_w = PANEL_W;
static int             s_view_x   = 0;
static bool            s_first;          // khung đầu sau screen_load -> luôn vẽ

static int static_x(const LineState &L)
{
    switch (L.cfg->align) {
        case 0:  return 0;
        case 2:  return s_canvas_w - L.m.width;
        default: return (s_canvas_w - L.m.width) / 2;
    }
}

// Vị trí chạy chữ sau 'elapsed' ms: bắt đầu từ mép phải canvas, mỗi speed_ms
// lùi 1 px; khi bản đầu chạy hết 1 chu kỳ thì nối vòng (x trong (-period, 0]).
static int scroll_x(const LineState &L, int64_t elapsed)
{
    const int64_t steps = elapsed > 0 ? elapsed / L.cfg->speed_ms : 0;
    int64_t x = s_canvas_w - steps;
    if (x <= -L.period) x = -((-x) % L.period);
    return (int)x;
}

void screen_load(const DisplaySettings &cfg, int64_t epoch_ms, int canvas_w, int view_x)
{
    s_cfg      = cfg;
    s_epoch    = epoch_ms;
    s_canvas_w = canvas_w;
    s_view_x   = view_x;
    s_first    = true;

    int total_h = 0;
    for (int i = 0; i < s_cfg.lines; i++) {
        LineState &L   = s_line[i];
        L.cfg          = &s_cfg.line[i];
        L.font         = font_get(L.cfg->font, L.cfg->size);
        L.m            = text_measure(L.font, L.cfg->text);
        L.scroll       = L.m.width > s_canvas_w;
        L.period       = L.m.width + SCROLL_GAP_SPACES * font_glyph(L.font, ' ')->width;
        if (L.period < 1) L.period = 1;
        L.x            = L.scroll ? s_canvas_w : static_x(L);   // chạy chữ: bắt đầu từ mép phải
        total_h += L.m.height;
    }

    const int free_px = PANEL_H - total_h;
    if (s_cfg.lines == 1) {
        s_line[0].y = free_px / 2;
    } else {
        const int edge = free_px > 0 ? free_px / 3 : 0;
        s_line[0].y = edge;
        s_line[1].y = PANEL_H - edge - s_line[1].m.height;
    }
}

bool screen_tick(int64_t now_ms)
{
    bool changed = s_first, animated = false;
    s_first = false;
    const int64_t elapsed = now_ms - s_epoch;

    for (int i = 0; i < s_cfg.lines; i++) {
        LineState &L = s_line[i];
        animated |= paint_is_animated(L.cfg->effect);
        if (!L.scroll) continue;
        const int x = scroll_x(L, elapsed);
        if (x != L.x) { L.x = x; changed = true; }
    }

    if (animated) {
        const uint8_t hue = (uint8_t)((elapsed > 0 ? elapsed / HUE_STEP_MS : 0) * s_cfg.fx_speed);
        if (hue != s_hue) { s_hue = hue; changed = true; }
    }
    return changed;
}

void screen_draw(void)
{
    panel_clear();
    for (int i = 0; i < s_cfg.lines; i++) {
        const LineState &L = s_line[i];
        const Paint p = { L.cfg->effect, rgb_from_hex(L.cfg->color), (uint8_t)(s_hue + i * HUE_LINE_OFS) };
        if (!L.scroll) {
            text_draw(L.x, L.y, L.font, L.cfg->text, p, L.m.top, s_view_x);
            continue;
        }
        // Chạy chữ: vẽ lặp lại, mỗi bản cách nhau 1 tab, phủ kín chiều ngang canvas
        for (int x = L.x; x < s_canvas_w; x += L.period)
            if (x + L.period > s_view_x)                  // bỏ bản nằm hẳn bên trái panel này
                text_draw(x, L.y, L.font, L.cfg->text, p, L.m.top, s_view_x);
    }
}
