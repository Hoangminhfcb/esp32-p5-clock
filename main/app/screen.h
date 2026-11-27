// =====================================================================
//  app/screen.h — Bố cục + chuyển động của nội dung trên panel
//
//  Không có FreeRTOS ở đây, chỉ có logic thuần:
//    screen_load()  : nhận cài đặt mới -> tính vị trí từng hàng
//    screen_tick()  : cập nhật chạy chữ / màu theo thời gian, báo có cần vẽ lại
//    screen_draw()  : vẽ khung hình vào buffer phụ của panel
//
//  Màn hình ảo (canvas): rộng canvas_w (64 khi riêng, 128 khi ghép 2 panel).
//  Panel này chỉ vẽ cột [view_x, view_x + PANEL_W) của canvas.
//
//  Vị trí chạy chữ và pha màu được TÍNH THẲNG từ (now - epoch), không cộng
//  dồn -> 2 mạch dùng chung đồng hồ + chung epoch sẽ vẽ khớp nhau từng pixel.
//
//  Quy tắc:
//    - 1 hàng: căn giữa theo chiều dọc
//    - 2 hàng: chia đều khoảng trống trên / giữa / dưới
//    - Hàng vừa canvas  -> đứng yên (căn trái/giữa/phải)
//    - Hàng dài hơn     -> chạy chữ liên tục, mỗi lần lặp cách nhau 1 tab
// =====================================================================
#pragma once
#include <stdint.h>
#include "app/settings.h"

void screen_load(const DisplaySettings &cfg, int64_t epoch_ms, int canvas_w, int view_x);
bool screen_tick(int64_t now_ms);     // true nếu khung hình thay đổi
void screen_draw(void);
