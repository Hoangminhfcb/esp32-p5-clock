// =====================================================================
//  app/link_state.h — Trạng thái ghép 2 màn hình (dữ liệu, không có mạng)
//
//  net/link.cpp lo phần truyền ESP-NOW và gọi các hàm ở đây để cập nhật.
//  display_task / settings / web_api chỉ ĐỌC từ đây.
//
//  Vai trò:
//    LINK_STANDALONE  master riêng: phát WiFi, hiện nội dung của chính nó (64x32)
//    LINK_MASTER      phát WiFi, nội dung trải trên 2 panel (màn hình ảo 128x32)
//    LINK_SLAVE       không phát WiFi, hiện phần còn lại theo cấu hình của master
//
//  Màn hình ảo (canvas): rộng PANEL_W khi riêng, 2*PANEL_W khi ghép.
//  Mỗi mạch chỉ vẽ vùng [view_x, view_x + PANEL_W) của canvas.
//
//  Đồng hồ chung: link_now_ms() = đồng hồ của master (slave tự cộng độ lệch)
//  -> chạy chữ / đổi màu trên 2 panel khớp nhau.
// =====================================================================
#pragma once
#include <stdint.h>
#include "app/settings.h"

enum LinkRole : uint8_t {
    LINK_STANDALONE = 0,
    LINK_MASTER     = 1,
    LINK_SLAVE      = 2,
};

// Cấu hình ghép nối (lưu NVS, nhớ khi mất điện)
struct LinkConfig {
    uint8_t role;          // LinkRole
    uint8_t master_side;   // 0 = master bên TRÁI, 1 = master bên PHẢI
    uint8_t channel;       // slave: kênh WiFi lần cuối nghe thấy master
    uint8_t peer[6];       // MAC mạch còn lại
};

void     link_state_init(void);                    // nạp NVS (gọi sau nvs_flash_init)
void     link_state_get(LinkConfig *out);
void     link_state_set(const LinkConfig *in);     // lưu NVS + version++
LinkRole link_role(void);
uint32_t link_state_version(void);                 // tăng khi vai trò / vị trí / nội dung slave đổi

// Vùng màn hình ảo mà mạch này hiển thị
int      link_canvas_width(void);
int      link_view_x(void);

// Mã 4 ký tự của mạch (từ MAC) + tên WiFi phát ra
void     link_set_device_id(const char *id);       // net/wifi.cpp gọi lúc khởi động
const char *link_device_id(void);
const char *link_ap_ssid(void);

// Đồng hồ chung (ms)
int64_t  link_now_ms(void);
void     link_clock_sample(int64_t master_ms, int64_t local_us);   // slave: mỗi gói từ master
void     link_clock_reset(void);

// Nội dung slave nhận từ master (chỉ trong RAM — không đè cài đặt riêng của slave)
void     link_remote_set(const DisplaySettings *s, int64_t epoch_ms);
bool     link_remote_get(DisplaySettings *out, int64_t *epoch_ms);   // false nếu chưa nhận được
