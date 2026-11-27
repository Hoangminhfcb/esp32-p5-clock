// =====================================================================
//  hal/panel.h — Lớp phần cứng: panel LED HUB75
//
//  Bọc thư viện ESP32-HUB75-MatrixPanel-DMA. Các module khác CHỈ gọi
//  các hàm dưới đây, không đụng trực tiếp vào thư viện -> sau này đổi
//  panel / đổi thư viện chỉ cần sửa panel.cpp.
//
//  Panel chạy double buffer: vẽ vào buffer phụ rồi gọi panel_present()
//  để hiện ra (không bị chớp).
// =====================================================================
#pragma once
#include <stdint.h>

void panel_init(void);
void panel_set_brightness(uint8_t value);                       // 0..255

void panel_clear(void);                                          // xóa buffer phụ
void panel_fill(uint8_t r, uint8_t g, uint8_t b);                // tô cả màn hình
void panel_set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);   // tự bỏ qua nếu ngoài màn hình
void panel_present(void);                                        // đẩy buffer phụ ra panel
