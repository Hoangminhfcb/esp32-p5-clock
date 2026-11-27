// =====================================================================
//  net/dns_server.h — DNS "bắt hết" cho cổng đăng nhập (captive portal)
//
//  Điện thoại vào WiFi của ESP32 sẽ hỏi DNS (ESP32 phát qua DHCP) rất nhiều
//  tên miền: kiểm tra Internet, đồng bộ app... Không có ai trả lời thì mỗi
//  câu hỏi phải chờ hết giờ -> web load chậm, điện thoại tưởng WiFi hỏng.
//
//  Server này trả lời NGAY mọi câu hỏi:
//    - bản ghi A (IPv4)  -> IP của AP (192.168.4.1)
//    - loại khác (AAAA…) -> "không có" (để máy khỏi chờ)
//  Kết hợp với web server chuyển trang lạ về "/" -> điện thoại hiện
//  "Đăng nhập vào mạng" và mở thẳng trang điều khiển.
//
//  Bật/tắt: WIFI_CAPTIVE_PORTAL trong config/app_config.h
// =====================================================================
#pragma once

void dns_server_start(void);    // gọi sau wifi_start() (chỉ khi phát AP)
