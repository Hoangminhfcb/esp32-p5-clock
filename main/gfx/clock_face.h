#pragma once
#include "gfx/color.h"
// Chữ 3x5 cho đồng hồ 64x32; scale=3 tạo giờ cao 15 px.
int clock_face_width(const char *text, int scale);
void clock_face_draw(const char *text, int y, int scale, Rgb color);
