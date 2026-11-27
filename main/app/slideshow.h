// =====================================================================
//  app/slideshow.h — Trình chiếu: hiện lần lượt các profile đã lưu
//
//  Danh sách chiếu = các (ô profile, số giây) theo thứ tự, lưu NVS
//  (namespace "show"), cùng trạng thái bật/tắt -> mất điện bật lại vẫn chiếu tiếp.
//
//  Mỗi slide gọi settings_show() (KHÔNG ghi flash). Tắt trình chiếu ->
//  settings_restore() hiện lại nội dung đã lưu. Khi ghép 2 màn hình, master
//  chiếu và tự gửi từng slide sang slave như nội dung bình thường.
//
//  Gửi nội dung mới từ web (POST /api/set) sẽ dừng trình chiếu.
// =====================================================================
#pragma once
#include <stdint.h>
#include "config/app_config.h"

#define SHOW_SECS_MIN 2
#define SHOW_SECS_MAX 3600

struct ShowItem {
    uint8_t  slot;      // ô profile
    uint16_t secs;      // thời gian hiện (giây)
};

struct ShowStatus {
    bool     on;
    int      count;
    ShowItem items[PROFILE_MAX];
    int      cur_slot;   // ô đang chiếu, -1 nếu không
    uint32_t left_ms;    // còn bao lâu thì sang slide sau
};

void slideshow_init(void);                     // sau settings_init + profiles_init
void slideshow_status(ShowStatus *out);
bool slideshow_running(void);

// Đặt danh sách chiếu (bỏ ô trống / trùng / sai). Không đổi trạng thái bật/tắt.
void slideshow_set_items(const ShowItem *items, int count);
void slideshow_start(void);                    // chiếu từ slide đầu
void slideshow_stop(void);                     // dừng + hiện lại nội dung đã lưu

// Báo khi profile thay đổi để trình chiếu cập nhật
void slideshow_on_profile_saved(int slot);
void slideshow_on_profile_deleted(int slot);
