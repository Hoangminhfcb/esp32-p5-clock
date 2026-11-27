#pragma once
#include "gfx/color.h"
// Giờ LED 7 đoạn cao 19 px; lịch bitmap cao 8 px.
void clock_face_time(int hour, int minute, bool colon, Rgb color);
// Đoạn rainbow có đuôi mờ quanh toàn panel; pha 0..59999 ms, 1 vòng/phút.
void clock_face_seconds(uint32_t minute_ms);
void clock_face_status(bool connecting);
int clock_face_date_width(const char *text);
void clock_face_date(const char *text, int offset_y, Rgb color);
int clock_face_width(const char *text, int scale);
void clock_face_draw(const char *text, int y, int scale, Rgb color);
