#include "display_task.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "app_config.h"
#include "display.h"
#include "settings.h"
#include "text_renderer.h"

#define FRAME_MS           10    // chu ky ve toi da (100 khung/giay)
#define NOTICE_FONT_BIG    3     // Spleen 5x8
#define NOTICE_FONT_SMALL  0     // Tom Thumb (neu Spleen khong vua)

struct LineState {
    const LineSettings *cfg;
    const Font *font;
    TextMetrics m;
    int     x, y;
    bool    scroll;
    int64_t next_step_ms;
};

static DisplaySettings s_cfg;          // ban sao cai dat dang hien thi
static LineState       s_line[MAX_LINES];
static uint8_t         s_hue;

static inline int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

// Tinh vi tri cac hang theo cai dat hien tai
static void layout(void)
{
    int total = 0;
    for (int i = 0; i < s_cfg.lines; i++) {
        LineState &L = s_line[i];
        L.cfg    = &s_cfg.line[i];
        L.font   = font_get(L.cfg->font);
        L.m      = text_measure(L.font, L.cfg->text, L.cfg->scale);
        L.scroll = L.m.width > PANEL_W;                  // vua man hinh -> khong chay
        if (L.scroll)                 L.x = PANEL_W;     // bat dau tu mep phai
        else if (L.cfg->align == 0)   L.x = 0;
        else if (L.cfg->align == 2)   L.x = PANEL_W - L.m.width;
        else                          L.x = (PANEL_W - L.m.width) / 2;
        L.next_step_ms = now_ms() + L.cfg->speed_ms;
        total += L.m.height;
    }
    // Chia deu khoang trong theo chieu doc
    int free_px = PANEL_H - total;
    if (s_cfg.lines == 1) {
        s_line[0].y = free_px / 2;
    } else {
        int edge = free_px > 0 ? free_px / 3 : 0;
        s_line[0].y = edge;
        s_line[1].y = PANEL_H - edge - s_line[1].m.height;
    }
}

static void draw_frame(void)
{
    display->clearScreen();
    for (int i = 0; i < s_cfg.lines; i++) {
        const LineState &L = s_line[i];
        const uint32_t c = L.cfg->color;
        // lech pha mau giua 2 hang cho dep
        Paint p = { L.cfg->effect, (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c, (uint8_t)(s_hue + i * 64) };
        draw_text(L.x, L.y, L.font, L.cfg->text, L.cfg->scale, p, L.m.top);
    }
    present();
}

// Dung thong bao tam thoi (vd dia chi IP) thanh 1 hang rieng
static void make_notice(DisplaySettings *n, const char *msg)
{
    n->lines = 1;
    LineSettings &l = n->line[0];
    strncpy(l.text, msg, LINE_TEXT_MAX);
    l.text[LINE_TEXT_MAX] = '\0';
    l.scale  = 1;
    l.color  = 0x00C8FF;
    l.effect = FX_SOLID;
    l.align  = 1;
    l.font   = NOTICE_FONT_BIG;
    if (text_measure(font_get(l.font), l.text, 1).width > PANEL_W) l.font = NOTICE_FONT_SMALL;
}

static void task(void *)
{
    uint32_t   seen_version = 0;
    bool       notice_on    = false;
    bool       dirty        = false;
    int64_t    next_hue_ms  = 0;
    TickType_t last         = xTaskGetTickCount();
    char       notice[48];

    while (true) {
        // --- Co thay doi cai dat / thong bao het han -> tinh lai ---
        const bool has_notice = settings_get_notice(notice, sizeof(notice));
        const uint32_t v = settings_version();
        if (v != seen_version || has_notice != notice_on) {
            seen_version = v;
            notice_on    = has_notice;
            settings_get(&s_cfg);
            display_set_brightness(s_cfg.brightness);
            if (has_notice) make_notice(&s_cfg, notice);
            layout();
            dirty = true;
        }

        const int64_t t = now_ms();
        bool animated = false;

        // --- Chay chu: moi hang buoc theo toc do rieng ---
        for (int i = 0; i < s_cfg.lines; i++) {
            LineState &L = s_line[i];
            if (L.cfg->effect != FX_SOLID) animated = true;
            if (!L.scroll) continue;
            while (t >= L.next_step_ms) {
                if (--L.x < -L.m.width) L.x = PANEL_W;   // het chu -> chay lai tu dau
                L.next_step_ms += L.cfg->speed_ms;
                dirty = true;
            }
        }

        // --- Cau vong / doi mau ---
        if (animated && t >= next_hue_ms) {
            s_hue += s_cfg.fx_speed;
            next_hue_ms = t + 20;
            dirty = true;
        }

        if (dirty) { draw_frame(); dirty = false; }
        vTaskDelayUntil(&last, pdMS_TO_TICKS(FRAME_MS) ? pdMS_TO_TICKS(FRAME_MS) : 1);
    }
}

void display_task_start(void)
{
    // Core 1: tach khoi WiFi (core 0) de chu chay muot
    xTaskCreatePinnedToCore(task, "display", 4096, nullptr, 5, nullptr, 1);
}
