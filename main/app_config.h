// =====================================================================
//  app_config.h - TAT CA cau hinh cua du an nam o day
// =====================================================================
#pragma once

// ---------------------------------------------------------------------
//  TEST: bo comment dong duoi de chay cac bai test luc khoi dong.
//        Chon tung bai test trong tests.cpp -> run_startup_tests().
// ---------------------------------------------------------------------
// #define RUN_STARTUP_TESTS

// ---------------------------------------------------------------------
//  PANEL
// ---------------------------------------------------------------------
#define PANEL_W      64
#define PANEL_H      32
#define PANEL_CHAIN  1

// Thu tu: R1  G1  B1  R2  G2  B2  A   B   C   D   E   LAT OE  CLK
#define PANEL_PINS {10, 11, 12, 16, 17, 18, 35, 36, 37, 38, -1, 5, 6, 4}
// ---------------------------------------------------------------------
//  WIFI
//  - ESP32 LUON phat WiFi rieng (AP) -> vao http://192.168.4.1
//  - Neu dien WIFI_STA_SSID thi ESP32 vao them WiFi nha ban;
//    IP duoc hien len panel vai giay khi ket noi xong.
// ---------------------------------------------------------------------
#define WIFI_AP_SSID    "HUB75-LED"
#define WIFI_AP_PASS    "12345678"      // >= 8 ky tu, de "" neu muon mo
#define WIFI_AP_CHANNEL 6

#define WIFI_STA_SSID   ""              // vd "MyHomeWifi", de "" = khong dung
#define WIFI_STA_PASS   ""

// ---------------------------------------------------------------------
//  MAC DINH (lan dau chay hoac khi bam "Mac dinh" tren web)
//  Font: xem danh sach trong fonts/fonts.cpp
//  Hieu ung: 0 = 1 mau, 1 = cau vong chay, 2 = doi mau lien tuc
// ---------------------------------------------------------------------
#define DEFAULT_LINES       1           // 1 hoac 2 hang chu
#define DEFAULT_BRIGHTNESS  128         // 0..255
#define DEFAULT_FX_SPEED    5           // 1..10: toc do doi mau cau vong

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

#define DEFAULT_SCALE       1
#define DEFAULT_SPEED_MS    30          // ms / 1 pixel khi chay chu
#define DEFAULT_ALIGN       0           // 0 trai, 1 giua, 2 phai (khi khong chay chu)

#define MAX_LINES           2
#define LINE_TEXT_MAX       255         // byte UTF-8 / hang (chu Viet co dau = 2-3 byte)
#define LINE_GAP            1           // khoang cach toi thieu giua 2 hang (px)
