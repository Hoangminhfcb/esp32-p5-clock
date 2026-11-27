#include "tests/panel_tests.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config/app_config.h"
#include "hal/panel.h"
#include "gfx/font.h"
#include "gfx/text.h"
#include "gfx/color.h"
#include <stdio.h>

// ---------------------------------------------------------------------
//  Chọn bài test cần chạy: comment / bỏ comment từng dòng
// ---------------------------------------------------------------------
void tests_run_startup(void)
{
    test_colors();
    test_sweep_bar();
    test_static_text();
    // test_all_sizes();
    // test_vietnamese();
    // test_rainbow();
    // test_scroll_text("XIN CHÀO P5 HUB75 ESP32", 0, 14, 0x00FF00, 30);
}

// ---------------------------------------------------------------------

static void wait_ms(int ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

static void draw_centered(const Font *f, const char *t, Rgb c)
{
    const TextMetrics m = text_measure(f, t);
    text_draw_color((PANEL_W - m.width) / 2, (PANEL_H - m.height) / 2, f, t, c);
}

void test_colors(void)
{
    const Rgb colors[] = { {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255} };
    for (const auto &c : colors) {
        panel_fill(c.r, c.g, c.b);
        panel_present();
        wait_ms(1000);
    }
}

void test_sweep_bar(void)
{
    TickType_t last = xTaskGetTickCount();
    for (int x = 0; x < PANEL_W; x++) {
        panel_clear();
        for (int y = 0; y < PANEL_H; y++) panel_set_pixel(x, y, 255, 255, 255);
        panel_present();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(30));
    }
}

void test_static_text(void)
{
    panel_clear();
    text_draw_color(1, 1,  font_get(0, 13), "HELLO",       {255, 255, 0});
    text_draw_color(1, 21, font_get(1, 7),  "ESP32 HUB75", {0, 255, 255});
    panel_present();                     // phải present mới hiện (đang bật double buffer)
    wait_ms(3000);
}

void test_all_sizes(void)
{
    char label[16];
    for (int fam = 0; fam < font_family_count(); fam++) {
        for (int size = font_min_size(fam); size <= font_max_size(fam); size++) {
            snprintf(label, sizeof(label), "Ab%d", size);
            panel_clear();
            draw_centered(font_get(fam, size), label, {255, 200, 0});
            panel_present();
            wait_ms(800);
        }
    }
}

void test_vietnamese(void)
{
    const char *samples[] = { "Tiếng Việt", "ĐẮK LẮK", "Ước mơ", "Hỗ trợ" };
    for (const char *t : samples) {
        panel_clear();
        draw_centered(font_get(0, 10), t, {255, 200, 0});
        panel_present();
        wait_ms(1500);
    }
}

void test_rainbow(void)
{
    const Font *f = font_get(0, 17);
    const char *t = "RGB";
    const TextMetrics m = text_measure(f, t);
    TickType_t last = xTaskGetTickCount();
    for (int i = 0; i < 300; i++) {
        const Paint p = { FX_RAINBOW, {0, 0, 0}, (uint8_t)(i * 3) };
        panel_clear();
        text_draw((PANEL_W - m.width) / 2, (PANEL_H - m.height) / 2, f, t, p, m.top);
        panel_present();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(20));
    }
}

void test_scroll_text(const char *msg, int font, int size, uint32_t color, int frame_ms)
{
    const Font *f = font_get(font, size);
    const TextMetrics m = text_measure(f, msg);
    const int y = (PANEL_H - m.height) / 2;
    TickType_t last = xTaskGetTickCount();
    for (int x = PANEL_W; x > -m.width; x--) {
        panel_clear();
        text_draw_color(x, y, f, msg, rgb_from_hex(color));
        panel_present();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(frame_ms));
    }
}
