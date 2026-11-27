#pragma once
#include "gfx/color.h"
// Font bitmap tự nhiên: giờ Silkscreen Bold pixel cao 13 px, lịch cao 8 px, không kéo giãn.
void clock_face_time(int hour, int minute, bool colon, Rgb color);
// Viền rainbow 1 pixel quanh giờ, từ giữa cạnh trên theo chiều kim đồng hồ.
void clock_face_seconds(int second);
void clock_face_status(bool connecting);
int clock_face_date_width(const char *text);
void clock_face_date(const char *text, int offset_y, Rgb color);
int clock_face_width(const char *text, int scale);
void clock_face_draw(const char *text, int y, int scale, Rgb color);
