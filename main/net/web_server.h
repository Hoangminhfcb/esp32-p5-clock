// =====================================================================
//  net/web_server.h — HTTP server: giao diện web + API JSON
//
//  Trang tĩnh (net/www/, nhúng vào firmware lúc build):
//    GET  /            index.html
//    GET  /style.css   style.css
//    GET  /app.js      app.js
//
//  API (chi tiết: docs/web-api.md):
//    GET  /api/state        cài đặt hiện tại + danh sách kiểu chữ
//    POST /api/set          lưu cài đặt (form urlencoded, chỉ gửi key cần đổi)
//    POST /api/reset        về mặc định
//    GET  /api/link         trạng thái ghép 2 màn hình (+ /scan /pair /unpair /side)
// =====================================================================
#pragma once

void web_server_start(void);
