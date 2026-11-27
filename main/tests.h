// =====================================================================
//  tests.h - Cac bai kiem tra panel
//  Bat/tat: bo comment  #define RUN_STARTUP_TESTS  trong app_config.h
// =====================================================================
#pragma once
#include <stdint.h>

void run_startup_tests(void);

void test_colors(void);        // to mau R/G/B/trang -> kiem tra mau va nguon
void test_sweep_bar(void);     // thanh trang chay ngang -> kiem tra thu tu cot
void test_static_text(void);   // chu tinh 2 dong
void test_all_fonts(void);     // lan luot hien ten tung font
void test_vietnamese(void);    // chu tieng Viet co dau
void test_rainbow(void);       // hieu ung cau vong
void test_scroll_text(const char *msg, int font, int scale,
                      uint8_t r, uint8_t g, uint8_t b, int frame_ms);
