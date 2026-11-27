#include "app/slideshow.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "app/settings.h"
#include "app/profiles.h"
#include "app/link_state.h"

static const char    *TAG     = "slideshow";
static const char    *NVS_NS  = "show";
static const char    *NVS_KEY = "cfg";
static const uint32_t S_MAGIC = 0x53484F31;      // "SHO1"

#define TASK_STACK    4096
#define TASK_PRIORITY 3
#define TASK_CORE     0
#define LOOP_MS       100

struct Stored {
    uint32_t magic;
    uint8_t  on;
    uint8_t  count;
    ShowItem items[PROFILE_MAX];
};

static SemaphoreHandle_t s_lock;
static Stored            s_cfg;
static int               s_idx     = -1;     // vị trí trong danh sách đang chiếu
static int               s_shown   = -1;     // ô profile đang hiện (-1 = chưa)
static int64_t           s_next_us = 0;      // lúc sang slide sau (0 = ngay)
static bool              s_restart = false;  // chiếu lại từ đầu
static bool              s_reload  = false;  // nạp lại slide hiện tại (profile vừa sửa)
static bool              s_stop    = false;  // vừa tắt -> hiện lại nội dung đã lưu

static inline int64_t now_us(void) { return esp_timer_get_time(); }

static void save_locked(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    s_cfg.magic = S_MAGIC;
    nvs_set_blob(h, NVS_KEY, &s_cfg, sizeof(s_cfg));
    nvs_commit(h);
    nvs_close(h);
}

static void load(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    Stored st;
    size_t len = sizeof(st);
    if (nvs_get_blob(h, NVS_KEY, &st, &len) == ESP_OK && len == sizeof(st) && st.magic == S_MAGIC) s_cfg = st;
    nvs_close(h);
    if (s_cfg.count > PROFILE_MAX) s_cfg.count = 0;
}

// Bỏ ô sai / trống / trùng, ép số giây về khoảng hợp lệ
static int clean(const ShowItem *in, int n, ShowItem *out)
{
    int k = 0;
    bool seen[PROFILE_MAX] = {};
    for (int i = 0; i < n && k < PROFILE_MAX; i++) {
        const int slot = in[i].slot;
        if (slot >= PROFILE_MAX || seen[slot] || !profiles_used(slot)) continue;
        seen[slot] = true;
        out[k] = in[i];
        if (out[k].secs < SHOW_SECS_MIN) out[k].secs = SHOW_SECS_MIN;
        if (out[k].secs > SHOW_SECS_MAX) out[k].secs = SHOW_SECS_MAX;
        k++;
    }
    return k;
}

// ---------------------------------------------------------------------
//  Task: đến giờ thì sang slide sau
// ---------------------------------------------------------------------
static void step(void)
{
    if (link_role() == LINK_SLAVE) return;       // slave hiện theo master

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_stop) {
        s_stop  = false;
        s_shown = -1;
        xSemaphoreGive(s_lock);
        settings_restore();
        return;
    }
    if (!s_cfg.on || s_cfg.count == 0) { xSemaphoreGive(s_lock); return; }

    const int64_t t = now_us();
    const bool reload = s_reload;
    if (!s_restart && !reload && s_next_us && t < s_next_us) { xSemaphoreGive(s_lock); return; }

    int idx = s_restart ? 0 : (reload ? s_idx : s_idx + 1);
    if (idx < 0 || idx >= s_cfg.count) idx = 0;
    const ShowItem it = s_cfg.items[idx];
    const int count   = s_cfg.count;
    const int shown   = s_shown;
    s_restart = s_reload = false;
    xSemaphoreGive(s_lock);

    // Chỉ 1 slide và đang hiện rồi -> khỏi nạp lại (chạy chữ không bị giật về đầu)
    const bool same = count == 1 && shown == it.slot && !reload;
    bool ok = true;
    if (!same) {
        DisplaySettings *s = new DisplaySettings;
        ok = profiles_load(it.slot, s);
        if (ok) settings_show(s);
        delete s;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_idx     = idx;
    s_shown   = ok ? it.slot : -1;
    s_next_us = now_us() + (ok ? it.secs * 1000000LL : 0);      // lỗi -> thử slide sau ngay
    xSemaphoreGive(s_lock);
}

static void task(void *)
{
    while (true) {
        step();
        vTaskDelay(pdMS_TO_TICKS(LOOP_MS));
    }
}

// ---------------------------------------------------------------------
//  API
// ---------------------------------------------------------------------
void slideshow_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    load();
    ShowItem tmp[PROFILE_MAX];
    s_cfg.count = clean(s_cfg.items, s_cfg.count, tmp);
    memcpy(s_cfg.items, tmp, sizeof(tmp));
    if (s_cfg.count == 0) s_cfg.on = 0;
    s_restart = s_cfg.on;
    ESP_LOGI(TAG, "%d slide, %s", s_cfg.count, s_cfg.on ? "dang chieu" : "tat");
    xTaskCreatePinnedToCore(task, "slideshow", TASK_STACK, nullptr, TASK_PRIORITY, nullptr, TASK_CORE);
}

void slideshow_status(ShowStatus *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    out->on    = s_cfg.on;
    out->count = s_cfg.count;
    memcpy(out->items, s_cfg.items, sizeof(out->items));
    out->cur_slot = s_cfg.on ? s_shown : -1;
    const int64_t left = s_next_us - now_us();
    out->left_ms = (s_cfg.on && left > 0) ? (uint32_t)(left / 1000) : 0;
    xSemaphoreGive(s_lock);
}

bool slideshow_running(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const bool on = s_cfg.on;
    xSemaphoreGive(s_lock);
    return on;
}

void slideshow_set_items(const ShowItem *items, int count)
{
    ShowItem tmp[PROFILE_MAX];
    const int n = clean(items, count, tmp);
    xSemaphoreTake(s_lock, portMAX_DELAY);
    // Giữ slide đang chiếu nếu nó vẫn còn trong danh sách mới
    int keep = -1;
    for (int i = 0; i < n; i++) if (tmp[i].slot == s_shown) keep = i;
    memcpy(s_cfg.items, tmp, sizeof(tmp));
    s_cfg.count = n;
    if (n == 0 && s_cfg.on) { s_cfg.on = 0; s_stop = true; }
    else if (s_cfg.on) {
        if (keep >= 0) s_idx = keep;
        else s_restart = true;
    }
    save_locked();
    xSemaphoreGive(s_lock);
}

void slideshow_start(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_cfg.count > 0) {
        s_cfg.on  = 1;
        s_stop    = false;
        s_restart = true;
        s_shown   = -1;
        save_locked();
    }
    xSemaphoreGive(s_lock);
}

void slideshow_stop(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_cfg.on) {
        s_cfg.on = 0;
        s_stop   = true;
        save_locked();
    }
    xSemaphoreGive(s_lock);
}

void slideshow_on_profile_saved(int slot)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_cfg.on && slot == s_shown) s_reload = true;    // đang chiếu profile vừa sửa -> hiện bản mới
    xSemaphoreGive(s_lock);
}

void slideshow_on_profile_deleted(int slot)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int k = 0;
    for (int i = 0; i < s_cfg.count; i++) {
        if (s_cfg.items[i].slot == slot) { if (i <= s_idx) s_idx--; continue; }
        s_cfg.items[k++] = s_cfg.items[i];
    }
    const bool changed = k != s_cfg.count;
    s_cfg.count = k;
    if (changed) {
        if (k == 0 && s_cfg.on) { s_cfg.on = 0; s_stop = true; }
        else if (s_cfg.on && slot == s_shown) s_next_us = 0;   // đang chiếu nó -> sang slide sau ngay
        save_locked();
    }
    xSemaphoreGive(s_lock);
}
