// =====================================================================
//  secrets.example.h — MẪU cấu hình WiFi
//
//  Copy file này thành  secrets.h  (cùng thư mục) rồi điền thông tin thật.
//  secrets.h đã nằm trong .gitignore nên KHÔNG bị đẩy lên git.
// =====================================================================
#pragma once

// WiFi do ESP32 phát ra (luôn bật)
#define WIFI_AP_SSID    "HUB75-LED"
#define WIFI_AP_PASS    "12345678"      // >= 8 ký tự, để "" nếu muốn mạng mở

// WiFi nhà (tùy chọn): để "" nếu không dùng
#define WIFI_STA_SSID   ""
#define WIFI_STA_PASS   ""
