#include "app/link_state.h"
#include <string.h>
#include <stdio.h>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "config/app_config.h"
#include "config/secrets_loader.h"

static const char    *TAG       = "link_state";
static const char    *NVS_NS    = "link";
static const char    *NVS_KEY   = "cfg";
static const uint32_t CFG_MAGIC = 0x4C4E4B31;   // "LNK1" — đổi LinkConfig thì tăng

static SemaphoreHandle_t     s_lock;
static LinkConfig            s_cfg;
static std::atomic<uint32_t> s_version{1};
static char                  s_id[8]   = "0000";
static char                  s_ssid[33] = WIFI_AP_SSID;

// Nội dung slave nhận được
static DisplaySettings s_remote;
static int64_t         s_remote_epoch = 0;
static bool            s_remote_ok    = false;

// Đồng hồ: offset = đồng hồ master - đồng hồ mạch này (ms)
// Lấy giá trị LỚN NHẤT trong vài mẫu gần nhất = mẫu bị trễ đường truyền ít nhất.
#define CLOCK_SAMPLES 8
static std::atomic<int64_t> s_offset{0};
static int64_t s_samples[CLOCK_SAMPLES];
static int     s_nsamples = 0, s_sample_pos = 0;

// ---------------------------------------------------------------------
//  NVS
// ---------------------------------------------------------------------
struct Stored { uint32_t magic; LinkConfig c; };

static void nvs_load(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    Stored st;
    size_t len = sizeof(st);
    if (nvs_get_blob(h, NVS_KEY, &st, &len) == ESP_OK && len == sizeof(st) && st.magic == CFG_MAGIC) s_cfg = st.c;
    nvs_close(h);
    if (s_cfg.role > LINK_SLAVE) s_cfg.role = LINK_STANDALONE;
    if (s_cfg.master_side > 1)   s_cfg.master_side = 0;
}

static void nvs_save(const LinkConfig *c)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    const Stored st = { CFG_MAGIC, *c };
    nvs_set_blob(h, NVS_KEY, &st, sizeof(st));
    nvs_commit(h);
    nvs_close(h);
}

// ---------------------------------------------------------------------
//  Cấu hình
// ---------------------------------------------------------------------
void link_state_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    nvs_load();
    ESP_LOGI(TAG, "Vai tro: %s", s_cfg.role == LINK_MASTER ? "MASTER (ghep)" :
                                 s_cfg.role == LINK_SLAVE  ? "SLAVE (ghep)"  : "rieng");
}

void link_state_get(LinkConfig *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_cfg;
    xSemaphoreGive(s_lock);
}

void link_state_set(const LinkConfig *in)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const bool changed = memcmp(&s_cfg, in, sizeof(s_cfg)) != 0;
    s_cfg = *in;
    if (changed) s_version++;
    xSemaphoreGive(s_lock);
    if (changed) nvs_save(in);
}

LinkRole link_role(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const LinkRole r = (LinkRole)s_cfg.role;
    xSemaphoreGive(s_lock);
    return r;
}

uint32_t link_state_version(void) { return s_version; }

int link_canvas_width(void) { return link_role() == LINK_STANDALONE ? PANEL_W : 2 * PANEL_W; }

int link_view_x(void)
{
    LinkConfig c;
    link_state_get(&c);
    if (c.role == LINK_STANDALONE) return 0;
    const bool master_left = c.master_side == 0;
    const bool i_am_left   = (c.role == LINK_MASTER) == master_left;
    return i_am_left ? 0 : PANEL_W;
}

// ---------------------------------------------------------------------
//  Tên mạch
// ---------------------------------------------------------------------
void link_set_device_id(const char *id)
{
    strncpy(s_id, id, sizeof(s_id) - 1);
    snprintf(s_ssid, sizeof(s_ssid), "%s-%s", WIFI_AP_SSID, s_id);
}

const char *link_device_id(void) { return s_id; }
const char *link_ap_ssid(void)   { return s_ssid; }

// ---------------------------------------------------------------------
//  Đồng hồ chung
// ---------------------------------------------------------------------
int64_t link_now_ms(void) { return esp_timer_get_time() / 1000 + s_offset; }

void link_clock_sample(int64_t master_ms, int64_t local_us)
{
    const int64_t sample = master_ms - local_us / 1000;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    // Lệch quá xa (master vừa khởi động lại...) -> bỏ các mẫu cũ
    if (s_nsamples && (sample > s_offset + 1000 || sample < s_offset - 1000)) s_nsamples = 0;
    if (s_nsamples == 0) s_sample_pos = 0;
    s_samples[s_sample_pos] = sample;
    s_sample_pos = (s_sample_pos + 1) % CLOCK_SAMPLES;
    if (s_nsamples < CLOCK_SAMPLES) s_nsamples++;
    int64_t best = s_samples[0];
    for (int i = 1; i < s_nsamples; i++) if (s_samples[i] > best) best = s_samples[i];
    s_offset = best;
    xSemaphoreGive(s_lock);
}

void link_clock_reset(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_nsamples = 0;
    s_offset   = 0;
    xSemaphoreGive(s_lock);
}

// ---------------------------------------------------------------------
//  Nội dung của slave
// ---------------------------------------------------------------------
void link_remote_set(const DisplaySettings *s, int64_t epoch_ms)
{
    DisplaySettings tmp = *s;
    settings_sanitize(&tmp);           // dữ liệu từ ngoài vào -> kiểm tra lại
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_remote       = tmp;
    s_remote_epoch = epoch_ms;
    s_remote_ok    = true;
    s_version++;
    xSemaphoreGive(s_lock);
}

bool link_remote_get(DisplaySettings *out, int64_t *epoch_ms)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const bool ok = s_remote_ok;
    if (ok) { *out = s_remote; *epoch_ms = s_remote_epoch; }
    xSemaphoreGive(s_lock);
    return ok;
}
