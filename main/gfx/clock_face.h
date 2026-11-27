#pragma once
#include "gfx/color.h"
// Giờ cao 20 px; hàng lịch cao 10 px, tự chọn độ rộng vừa panel.
void clock_face_time(int hour, int minute, bool colon, Rgb color);
int clock_face_date_width(const char *text);
void clock_face_date(const char *text, int offset_y, Rgb color);
// Chữ nhỏ dùng cho hướng dẫn cấu hình khi chưa lấy được giờ.
int clock_face_width(const char *text, int scale);
void clock_face_draw(const char *text, int y, int scale, Rgb color);
