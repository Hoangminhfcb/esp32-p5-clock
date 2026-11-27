#pragma once
#include "gfx/color.h"
// Font bitmap tự nhiên: giờ cao 19 px, lịch cao 8 px, không kéo giãn.
void clock_face_time(int hour, int minute, bool colon, Rgb color);
// 60 pixel giữa panel, thêm một pixel mỗi giây; chỉ dùng hàng y=21.
void clock_face_seconds(int second, Rgb color);
int clock_face_date_width(const char *text);
void clock_face_date(const char *text, int offset_y, Rgb color);
int clock_face_width(const char *text, int scale);
void clock_face_draw(const char *text, int y, int scale, Rgb color);
