// =====================================================================
//  net/wifi.h — WiFi theo vai trò ghép nối (app/link_state.h)
//
//    riêng / MASTER : AP "WIFI_AP_SSID-xxxx" (xxxx từ MAC) + STA (WiFi nhà, tùy chọn)
//    SLAVE          : chỉ bật radio (STA, không kết nối) để nghe ESP-NOW từ master,
//                     KHÔNG phát WiFi
//  Tên / mật khẩu: config/secrets.h
//  Gọi sau link_state_init().
// =====================================================================
#pragma once

void wifi_start(void);
