#include "app/display_task.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "config/app_config.h"
#include "app/settings.h"
#include "app/notice.h"
#include "app/screen.h"
#include "app/link_state.h"
#include "gfx/font.h"
#include "gfx/text.h"
#include "hal/panel.h"

#define FRAME_MS          10      // chu kỳ vẽ tối đa (100 khung/giây)
#define TASK_STACK        4096
#define TASK_PRIORITY     5
#define TASK_CORE         1       // core 0 dành cho WiFi


// Biến thông báo (vd địa chỉ IP) thành cài đặt 1 hàng tạm thời
static void make_notice(DisplaySettings *n, const char *msg)
{
    n->lines = 1;
    LineSettings &l = n->line[0];
    strncpy(l.text, msg, LINE_TEXT_MAX);
    l.text[LINE_TEXT_MAX] = '\0';
    l.color  = NOTICE_COLOR;
    l.effect = FX_SOLID;
    l.align  = 1;
    l.font   = NOTICE_FONT;
    l.size   = NOTICE_SIZE;
    // giảm cỡ đến khi vừa chiều ngang (không cần chạy chữ)
    while (l.size > font_min_size(l.font) &&
           text_measure(font_get(l.font, l.size), l.text).width > PANEL_W) l.size--;
}

// Lấy nội dung cần hiện + vùng màn hình ảo
static void load_content(bool has_notice, const char *msg)
{
    DisplaySettings cfg;
    settings_get(&cfg);                          // cài đặt riêng của mạch (độ sáng khi chờ...)
    int64_t epoch = settings_epoch();
    int canvas_w  = link_canvas_width();
    int view_x    = link_view_x();
    bool local    = false;                       // true: chỉ hiện trên panel này (64x32)

    if (link_role() == LINK_SLAVE && !link_remote_get(&cfg, &epoch)) {
        make_notice(&cfg, "Chờ master...");      // slave chưa nhận được nội dung
        local = true;
    }
    if (has_notice) {
        make_notice(&cfg, msg);
        local = true;
    }
    if (local) {
        epoch    = link_now_ms();
        canvas_w = PANEL_W;
        view_x   = 0;
    }
    panel_set_brightness(cfg.brightness);
    screen_load(cfg, epoch, canvas_w, view_x);
}

static void task(void *)
{
    uint32_t   seen_settings = 0, seen_notice = 0, seen_link = 0;
    bool       notice_on = false;
    TickType_t last = xTaskGetTickCount();
    char       msg[48];

    while (true) {
        // 1. Có thay đổi (cài đặt / thông báo / ghép nối) -> nạp lại
        const bool     has_notice = notice_get(msg, sizeof(msg));
        const uint32_t vs = settings_version(), vn = notice_version(), vl = link_state_version();
        if (vs != seen_settings || vn != seen_notice || vl != seen_link || has_notice != notice_on) {
            seen_settings = vs;
            seen_notice   = vn;
            seen_link     = vl;
            notice_on     = has_notice;
            load_content(has_notice, msg);
        }

        // 2-3. Cập nhật chuyển động theo đồng hồ chung, vẽ nếu cần
        if (screen_tick(link_now_ms())) {
            screen_draw();
            panel_present();
        }

        const TickType_t period = pdMS_TO_TICKS(FRAME_MS);
        vTaskDelayUntil(&last, period ? period : 1);
    }
}

void display_task_start(void)
{
    xTaskCreatePinnedToCore(task, "display", TASK_STACK, nullptr, TASK_PRIORITY, nullptr, TASK_CORE);
}
