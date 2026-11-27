#include "tests.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_config.h"
#include "display.h"
#include "text_renderer.h"

// ---------------------------------------------------------------------
//  Chon bai test can chay: comment / bo comment tung dong
// ---------------------------------------------------------------------
void run_startup_tests(void)
{
    test_colors();
    test_sweep_bar();
    test_static_text();
    // test_all_fonts();
    // test_vietnamese();
    // test_rainbow();
    // test_scroll_text("XIN CHAO P5 HUB75 ESP32", 1, 2, 0, 255, 0, 30);
}

// ---------------------------------------------------------------------

void test_colors(void)
{
    const uint8_t colors[][3] = {{255,0,0}, {0,255,0}, {0,0,255}, {255,255,255}};
    for (auto &c : colors) {
        display->fillScreenRGB888(c[0], c[1], c[2]);
        present();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void test_sweep_bar(void)
{
    TickType_t last = xTaskGetTickCount();
    for (int x = 0; x < PANEL_W; x++) {
        display->clearScreen();
        for (int y = 0; y < PANEL_H; y++)
            display->drawPixelRGB888(x, y, 255, 255, 255);
        present();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(30));
    }
}

void test_static_text(void)
{
    display->clearScreen();
    draw_text_color(2, 4,  font_get(1), "HELLO",       255, 255, 0, 2);
    draw_text_color(2, 22, font_get(1), "ESP32 HUB75", 0, 255, 255, 1);
    present();                                   // phai flip moi hien (dang bat double buffer)
    vTaskDelay(pdMS_TO_TICKS(3000));
}

void test_all_fonts(void)
{
    for (int i = 0; i < font_count(); i++) {
        const Font *f = font_get(i);
        display->clearScreen();
        draw_text_color(0, 0, f, f->name, 255, 255, 255, 1);
        const TextMetrics m = text_measure(f, "Ab12", 1);
        draw_text_color(0, PANEL_H - m.height, f, "Ab12", 255, 160, 0, 1);
        present();
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

void test_scroll_text(const char *msg, int font, int scale, uint8_t r, uint8_t g, uint8_t b, int frame_ms)
{
    const Font *f   = font_get(font);
    const TextMetrics m = text_measure(f, msg, scale);
    const int   w   = m.width;
    const int   y   = (PANEL_H - m.height) / 2;
    TickType_t last = xTaskGetTickCount();

    for (int x = PANEL_W; x > -w; x--) {
        display->clearScreen();
        draw_text_color(x, y, f, msg, r, g, b, scale);
        present();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(frame_ms));
    }
}

void test_vietnamese(void)
{
    const char *lines[] = { "Tiếng Việt", "ĐẮK LẮK", "Ước mơ", "Hỗ trợ" };
    for (auto *t : lines) {
        display->clearScreen();
        const Font *f = font_get(7);                              // Spleen 6x12
        const TextMetrics m = text_measure(f, t, 1);
        draw_text_color((PANEL_W - m.width) / 2, (PANEL_H - m.height) / 2, f, t, 255, 200, 0, 1);
        present();
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

void test_rainbow(void)
{
    const Font *f = font_get(10);                                 // Jersey 10
    const char *t = "RGB";
    const TextMetrics m = text_measure(f, t, 1);
    TickType_t last = xTaskGetTickCount();
    for (int i = 0; i < 300; i++) {
        Paint p = { FX_RAINBOW, 0, 0, 0, (uint8_t)(i * 3) };
        display->clearScreen();
        draw_text((PANEL_W - m.width) / 2, (PANEL_H - m.height) / 2, f, t, 1, p, m.top);
        present();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(20));
    }
}
