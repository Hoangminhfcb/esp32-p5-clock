// =====================================================================
//  text_renderer.h - Ve chu UTF-8 (co tieng Viet) bang font bitmap
// =====================================================================
#pragma once
#include <stdint.h>
#include "font.h"

// Hieu ung mau
enum TextEffect : uint8_t {
    FX_SOLID   = 0,   // 1 mau
    FX_RAINBOW = 1,   // cau vong chay ngang
    FX_CYCLE   = 2,   // ca dong doi mau lien tuc
};

struct Paint {
    uint8_t effect;   // TextEffect
    uint8_t r, g, b;  // mau khi FX_SOLID
    uint8_t hue;      // pha mau hien tai (0..255) cho FX_RAINBOW / FX_CYCLE
};

// Kich thuoc that cua 1 dong chu (theo hang font, chua nhan scale)
struct TextMetrics {
    int width;        // px, da nhan scale, bo khoang trong cuoi
    int top;          // hang dau tien can ve (gom dau tieng Viet neu co)
    int bottom;       // hang cuoi cung
    int height;       // px, da nhan scale
};

// Doc 1 ky tu UTF-8, tien con tro. Ky tu loi -> '?'
uint32_t    utf8_next(const char **s);

TextMetrics text_measure(const Font *f, const char *s, int scale);

// Mau cau vong: hue 0..255 -> RGB
void        hue_to_rgb(uint8_t hue, uint8_t *r, uint8_t *g, uint8_t *b);

// Ve chuoi; (x, y) = goc tren-trai cua hang 'top_row' trong font
void        draw_text(int x, int y, const Font *f, const char *s, int scale,
                      const Paint &paint, int top_row = 0);

// Tien loi: ve 1 mau, y = goc tren cua o chu (theo text_measure)
void        draw_text_color(int x, int y, const Font *f, const char *s,
                            uint8_t r, uint8_t g, uint8_t b, int scale = 1);
