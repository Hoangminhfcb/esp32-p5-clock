// =====================================================================
//  app_config.h — Cấu hình chung của dự án (KHÔNG chứa mật khẩu)
//
//  Mật khẩu / tên WiFi nằm trong config/secrets.h (không đưa lên git).
//  Lần đầu: copy secrets.example.h -> secrets.h rồi sửa.
// =====================================================================
#pragma once

// ---------------------------------------------------------------------
//  TEST: bỏ comment dòng dưới để chạy các bài test lúc khởi động.
//        Chọn từng bài test trong tests/panel_tests.cpp -> tests_run_startup().
// ---------------------------------------------------------------------
// #define RUN_STARTUP_TESTS

// ---------------------------------------------------------------------
//  PANEL
// ---------------------------------------------------------------------
#define PANEL_W      64
#define PANEL_H      32
#define PANEL_CHAIN  1

// Thứ tự:          R1  G1  B1  R2  G2  B2  A   B   C   D   E   LAT OE  CLK
#define PANEL_PINS {10, 11, 12, 16, 17, 18, 35, 36, 37, 38, -1, 5, 6, 4}

// ---------------------------------------------------------------------
//  WIFI (tên + mật khẩu xem config/secrets.h)
//  - ESP32 phát WiFi riêng (AP) -> vào http://192.168.4.1
//    Tên WiFi = WIFI_AP_SSID + "-" + mã 4 ký tự lấy từ MAC (vd HUB75-LED-A1B2)
//    -> nạp cùng 1 firmware cho nhiều mạch mà tên WiFi vẫn khác nhau.
//  - Nếu điền WIFI_STA_SSID thì vào thêm WiFi nhà, IP hiện lên panel.
//  - Mạch đang làm SLAVE (ghép 2 màn hình) KHÔNG phát WiFi.
// ---------------------------------------------------------------------
#define WIFI_AP_CHANNEL     6
#define WIFI_AP_MAX_CLIENTS 4

// "Cổng đăng nhập" (captive portal): ESP32 trả lời mọi tên miền (DNS) bằng
// 192.168.4.1 và chuyển mọi trang lạ về trang điều khiển. Nhờ vậy điện thoại
// không chờ DNS / kiểm tra Internet đến hết giờ (nguyên nhân web load lâu,
// "như mất kết nối"), và tự bật trang điều khiển khi vừa vào WiFi.
// Đặt 0 để tắt.
#define WIFI_CAPTIVE_PORTAL 1

// ---------------------------------------------------------------------
//  GHÉP 2 MÀN HÌNH (ESP-NOW) — xem docs/pairing.md
//  - Mặc định mỗi mạch là 1 master riêng (phát WiFi, có web riêng).
//  - Trên web của 1 mạch bấm "Tìm" -> "Ghép": mạch đó làm MASTER,
//    mạch kia làm SLAVE (tắt WiFi, hiện nửa còn lại của màn hình 128x32).
//  - Giữ nút BOOT ~2 giây trên mạch nào cũng được -> hủy ghép,
//    cả 2 về làm master riêng. Ghép nối được nhớ khi mất điện.
// ---------------------------------------------------------------------
#define LINK_BUTTON_GPIO    0           // nút BOOT (ESP32-S3: GPIO0)
#define LINK_BUTTON_HOLD_MS 2000        // giữ bao lâu thì hủy ghép
#define LINK_SYNC_MS        500         // master gửi đồng hồ cho slave
#define LINK_LOST_MS        3000        // quá lâu không nghe nhau -> báo mất kết nối

// ---------------------------------------------------------------------
//  GIỚI HẠN
// ---------------------------------------------------------------------
#define MAX_LINES           2           // số hàng chữ tối đa
#define LINE_TEXT_MAX       255         // byte UTF-8 / hàng (chữ Việt có dấu = 2-3 byte)
#define LINE_GAP            1           // khoảng cách tối thiểu giữa 2 hàng (px)
#define PROFILE_MAX         10          // số profile lưu được (mỗi cái ~600 byte NVS)

// ---------------------------------------------------------------------
//  MẶC ĐỊNH (lần đầu chạy hoặc khi bấm "Mặc định" trên web)
//  Kiểu chữ: 0 = Đậm, 1 = Mỏng  (danh sách: gfx/fonts/fonts.cpp)
//  Cỡ chữ  : chiều cao chữ IN HOA, px (6..17)
//  Hiệu ứng: 0 = một màu, 1 = cầu vồng chạy, 2 = đổi màu liên tục
// ---------------------------------------------------------------------
#define DEFAULT_LINES       2           // 1 hoặc 2 hàng chữ
#define DEFAULT_BRIGHTNESS  128         // 0..255
#define DEFAULT_FX_SPEED    5           // 1..10: tốc độ đổi màu cầu vồng

#define DEFAULT_TEXT_1      "Facebook"
#define DEFAULT_FONT_1      1
#define DEFAULT_SIZE_1      8
#define DEFAULT_COLOR_1     0x0000FF    // 0xRRGGBB
#define DEFAULT_EFFECT_1    0

#define DEFAULT_TEXT_2      "Hoàng Lê Nhật Minh"
#define DEFAULT_FONT_2      0
#define DEFAULT_SIZE_2      13
#define DEFAULT_COLOR_2     0x00C8FF
#define DEFAULT_EFFECT_2    1

#define DEFAULT_SPEED_MS    30          // ms / 1 pixel khi chạy chữ
#define DEFAULT_ALIGN       0           // 0 trái, 1 giữa, 2 phải (khi không chạy chữ)
#define SCROLL_GAP_SPACES   4           // khoảng cách giữa 2 lần lặp chữ = 1 tab (4 dấu cách)

// ---------------------------------------------------------------------
//  THÔNG BÁO HỆ THỐNG
//  - Lời chào lúc khởi động (để "" nếu không muốn hiện)
//  - IP khi vào được WiFi nhà (chỉ khi có cấu hình WIFI_STA_SSID)
// ---------------------------------------------------------------------
#define NOTICE_FONT         0           // Đậm (dễ đọc nhất)
#define NOTICE_SIZE         8           // tự giảm nếu chữ dài hơn panel
#define NOTICE_COLOR        0x00C8FF
#define NOTICE_IP_MS        6000        // hiện IP khi vào được WiFi nhà

#define BOOT_MESSAGE        "hi there"   // lời chào lúc khởi động
#define BOOT_MESSAGE_MS     3000
#define SHOW_SSID_MS        5000        // sau lời chào: hiện tên WiFi của mạch (0 = tắt)
