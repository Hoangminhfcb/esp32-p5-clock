#pragma once
// Âm lịch Việt Nam, múi giờ UTC+7. Có đánh dấu tháng nhuận.
struct LunarDate { int day, month, year; bool leap; };
LunarDate lunar_date(int day, int month, int year);
