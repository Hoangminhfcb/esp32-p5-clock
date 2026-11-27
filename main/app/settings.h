// =====================================================================
//  app/settings.h — Cài đặt hiển thị (dữ liệu trung tâm của ứng dụng)
//
//  Luồng dữ liệu:
//     Web (POST /api/set) ──> settings_set() ──> lưu NVS
//                                  │ tăng version
//     display_task  <── settings_version() thay đổi -> settings_get() -> vẽ lại
//
//  Có 2 bản cài đặt:
//     ĐÃ LƯU  (settings_get_saved) : nội dung bạn gửi từ web, nhớ khi mất điện
//     ĐANG HIỆN (settings_get)     : thường = bản đã lưu; khi trình chiếu
//                                    (app/slideshow) là profile đang chiếu.
//     settings_show() chỉ đổi bản ĐANG HIỆN, KHÔNG ghi flash (đổi slide liên tục
//     không làm mòn flash).
//
//  An toàn đa luồng: mọi hàm đều có mutex, có thể gọi từ task bất kỳ.
// =====================================================================
#pragma once
#include <stdint.h>
#include "config/app_config.h"

// Cài đặt của 1 hàng chữ
struct LineSettings {
    char     text[LINE_TEXT_MAX + 1];   // UTF-8, có thể có dấu tiếng Việt
    uint8_t  font;         // id kiểu chữ (gfx/fonts/fonts.cpp)
    uint8_t  size;         // cỡ chữ = chiều cao chữ in hoa (px)
    uint8_t  align;        // 0 trái, 1 giữa, 2 phải (khi không chạy chữ)
    uint8_t  effect;       // TextEffect (gfx/color.h)
    uint32_t color;        // 0xRRGGBB (khi effect = FX_SOLID)
    uint16_t speed_ms;     // ms / pixel khi chạy chữ
};

// Cài đặt toàn màn hình
struct DisplaySettings {
    uint8_t      lines;        // 1..MAX_LINES
    uint8_t      brightness;   // 0..255
    uint8_t      fx_speed;     // 1..10 tốc độ đổi màu
    LineSettings line[MAX_LINES];
};

void     settings_init(void);                        // nạp từ NVS (gọi sau nvs_flash_init)
void     settings_get(DisplaySettings *out);         // bản ĐANG HIỆN trên panel
void     settings_get_saved(DisplaySettings *out);   // bản ĐÃ LƯU (nội dung chỉnh trên web)
void     settings_set(const DisplaySettings *in);    // kiểm tra, áp dụng, lưu NVS (cả 2 bản)
void     settings_reset(void);                       // về mặc định

// Hiện tạm 1 nội dung (slide trình chiếu): không lưu NVS, giữ độ sáng hiện tại
void     settings_show(const DisplaySettings *in);
// Bỏ nội dung tạm, hiện lại bản đã lưu
void     settings_restore(void);
// Chỉ đổi độ sáng (không chạy chữ lại từ đầu, không dừng trình chiếu)
void     settings_set_brightness(uint8_t value);
uint32_t settings_version(void);                     // tăng mỗi lần thay đổi
void     settings_touch(void);                       // báo "nội dung mới" (chạy chữ lại từ đầu), không đổi giá trị

// Mốc thời gian (đồng hồ chung link_now_ms) của lần thay đổi gần nhất:
// chạy chữ / đổi màu tính từ mốc này -> 2 màn hình ghép chạy khớp nhau.
int64_t  settings_epoch(void);

// Ép mọi giá trị về khoảng hợp lệ + tự giảm cỡ chữ cho vừa chiều cao panel
void     settings_sanitize(DisplaySettings *s);
void     settings_defaults(DisplaySettings *s);

// --- Lưu trữ (settings_nvs.cpp) ---
bool     settings_nvs_load(DisplaySettings *out);
void     settings_nvs_save(const DisplaySettings *s);
