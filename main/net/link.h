// =====================================================================
//  net/link.h — Ghép 2 màn hình qua ESP-NOW (chi tiết: docs/pairing.md)
//
//  Cả 2 mạch nạp CÙNG 1 firmware. Mặc định mỗi mạch là master riêng.
//
//  Ghép (làm trên web của mạch sẽ làm MASTER):
//    link_scan()   -> phát "DISCOVER", các mạch riêng xung quanh trả lời
//                     (và tự hiện mã của nó lên panel để bạn nhận ra)
//    link_pair()   -> gửi "PAIR" cho mạch được chọn -> nó thành SLAVE,
//                     khởi động lại, tắt WiFi, chờ nội dung từ master
//
//  Khi đã ghép:
//    master -> slave : DATA  (cài đặt + mốc epoch, chia nhiều gói)
//                      SYNC  (đồng hồ master, mỗi LINK_SYNC_MS)
//    slave  -> master: ALIVE (mỗi giây; báo bản nội dung đang có)
//    Slave mất master > LINK_LOST_MS -> dò lần lượt các kênh 1..13.
//
//  Hủy ghép: link_unpair() (web), hoặc giữ nút BOOT LINK_BUTTON_HOLD_MS
//  trên mạch bất kỳ -> gửi "UNPAIR" cho mạch kia -> cả 2 về master riêng.
//
//  Gọi link_start() sau wifi_start(). Các hàm khác an toàn đa luồng.
// =====================================================================
#pragma once
#include <stdint.h>

#define LINK_MAX_FOUND 8

struct LinkFound {
    uint8_t mac[6];
    char    id[8];       // mã 4 ký tự (khớp tên WiFi HUB75-LED-xxxx của mạch đó)
    int8_t  rssi;
};

struct LinkStatus {
    uint8_t role;          // LinkRole (app/link_state.h)
    uint8_t master_side;   // 0 master bên trái, 1 bên phải
    char    peer_id[8];    // mã mạch đang ghép ("" nếu không)
    bool    peer_online;   // đang nghe thấy mạch kia
    bool    scanning;      // đang tìm
    bool    pairing;       // đang chờ mạch kia đồng ý ghép
    bool    pair_failed;   // lần ghép gần nhất không thành
    int     found_count;
    LinkFound found[LINK_MAX_FOUND];
};

void link_start(void);
void link_status(LinkStatus *out);

void link_scan(void);
void link_pair(const uint8_t mac[6]);
void link_unpair(void);
void link_set_side(uint8_t master_side);
