// =====================================================================
//  tests/panel_tests.h — Các bài kiểm tra panel lúc khởi động
//
//  Bật/tắt: bỏ comment  #define RUN_STARTUP_TESTS  trong config/app_config.h
//  Chọn bài test: comment / bỏ comment từng dòng trong tests_run_startup()
// =====================================================================
#pragma once
#include <stdint.h>

void tests_run_startup(void);

void test_colors(void);        // tô màu R/G/B/trắng -> kiểm tra màu và nguồn
void test_sweep_bar(void);     // thanh trắng chạy ngang -> kiểm tra thứ tự cột
void test_static_text(void);   // chữ tĩnh 2 dòng
void test_all_sizes(void);     // mọi kiểu chữ, mọi cỡ (tăng 1 px)
void test_vietnamese(void);    // chữ tiếng Việt có dấu
void test_rainbow(void);       // hiệu ứng cầu vồng
void test_scroll_text(const char *msg, int font, int size, uint32_t color, int frame_ms);
