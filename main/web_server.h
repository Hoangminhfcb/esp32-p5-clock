// =====================================================================
//  web_server.h - Trang web chinh 2 hang chu: text / font / size / mau / hieu ung / toc do
//    GET  /              giao dien (web/index.html)
//    GET  /api/state     cai dat hien tai + danh sach font (JSON)
//    GET  /api/font?id=N du lieu font de web ve preview giong het panel
//    POST /api/set       luu cai dat (form: lines, brightness, fx, text0/1, font0/1, scale0/1,
//                        align0/1, effect0/1, color0/1, speed0/1)
//    POST /api/reset     ve mac dinh
// =====================================================================
#pragma once

void web_server_start(void);
