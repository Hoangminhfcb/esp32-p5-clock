#pragma once
#include "gfx/color.h"
// Giờ 12x21 lấy từ ảnh mẫu; lịch bitmap cao 8 px sát đáy.
void clock_face_time(int hour, int minute, uint32_t second_ms, Rgb color);
// Một hàng rainbow tại y=22: 30s trái->phải, 30s phải->trái, nội suy mượt.
void clock_face_seconds(uint32_t minute_ms);
void clock_face_status(bool connecting);
int clock_face_date_width(const char *text);
void clock_face_date(const char *text, int offset_y, Rgb color);
int clock_face_width(const char *text, int scale);
void clock_face_draw(const char *text, int y, int scale, Rgb color);
