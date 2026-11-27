// =====================================================================
//  gfx/text.h — Đo và vẽ chữ UTF-8 (có tiếng Việt) bằng font bitmap
// =====================================================================
#pragma once
#include <stdint.h>
#include "gfx/font.h"
#include "gfx/color.h"

// Kích thước thật của 1 dòng chữ
struct TextMetrics {
    int width;        // px (bỏ khoảng trống sau ký tự cuối)
    int top;          // hàng font đầu tiên cần vẽ (gồm dấu tiếng Việt nếu có)
    int bottom;       // hàng font cuối cùng
    int height;       // px = bottom - top + 1
};

TextMetrics text_measure(const Font *f, const char *s);

// Vẽ chuỗi; (x, y) = góc trên-trái của hàng font 'top_row'
// (thường truyền metrics.top để chữ nằm gọn trong ô của nó)
// view_x: x là tọa độ trên màn hình ảo (ghép 2 panel); panel này hiện cột
//         [view_x, view_x + PANEL_W). Màu cầu vồng tính theo tọa độ ảo -> liền mạch.
void text_draw(int x, int y, const Font *f, const char *s, const Paint &paint, int top_row, int view_x = 0);

// Tiện lợi: vẽ một màu, (x, y) = góc trên-trái ô chữ
void text_draw_color(int x, int y, const Font *f, const char *s, Rgb color);
