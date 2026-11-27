// =====================================================================
//  gfx/color.h — Màu và hiệu ứng màu cho chữ
//
//  THÊM HIỆU ỨNG MỚI:
//    1. Thêm giá trị vào enum TextEffect (trước FX_COUNT)
//    2. Thêm 1 case trong paint_color_at() (color.cpp)
//    3. Thêm nút trên web: net/www/index.html (#effect) + hàm colorAt trong app.js
// =====================================================================
#pragma once
#include <stdint.h>

enum TextEffect : uint8_t {
    FX_SOLID   = 0,   // một màu
    FX_RAINBOW = 1,   // cầu vồng chạy ngang
    FX_CYCLE   = 2,   // cả hàng đổi màu liên tục
    FX_COUNT
};

struct Rgb { uint8_t r, g, b; };

// Cách tô màu 1 hàng chữ trong 1 khung hình
struct Paint {
    uint8_t effect;   // TextEffect
    Rgb     color;    // màu khi FX_SOLID
    uint8_t hue;      // pha màu hiện tại (0..255) cho các hiệu ứng động
};

static inline Rgb rgb_from_hex(uint32_t c) { return { (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c }; }

// Bánh xe màu: hue 0..255 -> RGB (bão hòa, sáng tối đa)
Rgb  hue_to_rgb(uint8_t hue);

// Màu tại cột x trên màn hình
Rgb  paint_color_at(const Paint &p, int x);

// Hiệu ứng có cần vẽ lại liên tục không
static inline bool paint_is_animated(uint8_t effect) { return effect != FX_SOLID; }
