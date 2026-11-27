#include "settings.h"
#include <string.h>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "font.h"
#include "text_renderer.h"

static const char *TAG = "settings";
static const char *NVS_NS  = "display";
static const char *NVS_KEY = "cfg";
static const uint32_t CFG_MAGIC = 0x48554232;   // doi khi thay doi struct

struct Stored { uint32_t magic; DisplaySettings s; };

static SemaphoreHandle_t s_lock;
static DisplaySettings   s_cur;
static std::atomic<uint32_t> s_version{1};

static char    s_notice[48];
static int64_t s_notice_until = 0;

static void line_defaults(LineSettings *l, const char *text, int font, uint32_t color, int effect)
{
    memset(l, 0, sizeof(*l));
    strncpy(l->text, text, LINE_TEXT_MAX);
    l->font     = font;
    l->scale    = DEFAULT_SCALE;
    l->align    = DEFAULT_ALIGN;
    l->effect   = effect;
    l->color    = color;
    l->speed_ms = DEFAULT_SPEED_MS;
}

static void load_defaults(DisplaySettings *s)
{
    memset(s, 0, sizeof(*s));
    s->lines      = DEFAULT_LINES;
    s->brightness = DEFAULT_BRIGHTNESS;
    s->fx_speed   = DEFAULT_FX_SPEED;
    line_defaults(&s->line[0], DEFAULT_TEXT_1, DEFAULT_FONT_1, DEFAULT_COLOR_1, DEFAULT_EFFECT_1);
    line_defaults(&s->line[1], DEFAULT_TEXT_2, DEFAULT_FONT_2, DEFAULT_COLOR_2, DEFAULT_EFFECT_2);
}

static int line_height(const LineSettings *l)
{
    return text_measure(font_get(l->font), l->text, l->scale).height;
}

void settings_sanitize(DisplaySettings *s)
{
    if (s->lines < 1 || s->lines > MAX_LINES) s->lines = 1;
    if (s->fx_speed < 1)  s->fx_speed = 1;
    if (s->fx_speed > 10) s->fx_speed = 10;
    for (int i = 0; i < MAX_LINES; i++) {
        LineSettings *l = &s->line[i];
        l->text[LINE_TEXT_MAX] = '\0';
        if (l->font >= font_count()) l->font = 0;
        if (l->scale < 1) l->scale = 1;
        if (l->scale > 4) l->scale = 4;
        if (l->align > 2)  l->align = 1;
        if (l->effect > 2) l->effect = 0;
        l->color &= 0xFFFFFF;
        if (l->speed_ms < 10)  l->speed_ms = 10;
        if (l->speed_ms > 500) l->speed_ms = 500;
    }
    // Giam size cho den khi tat ca cac hang vua chieu cao panel
    while (true) {
        int total = (s->lines - 1) * LINE_GAP, big = -1, big_h = 0;
        for (int i = 0; i < s->lines; i++) {
            int h = line_height(&s->line[i]);
            total += h;
            if (s->line[i].scale > 1 && h > big_h) { big = i; big_h = h; }
        }
        if (total <= PANEL_H || big < 0) break;
        s->line[big].scale--;
    }
}

static void save_nvs(const DisplaySettings *s)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    Stored st = { CFG_MAGIC, *s };
    nvs_set_blob(h, NVS_KEY, &st, sizeof(st));
    nvs_commit(h);
    nvs_close(h);
}

void settings_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    load_defaults(&s_cur);

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        Stored st;
        size_t len = sizeof(st);
        if (nvs_get_blob(h, NVS_KEY, &st, &len) == ESP_OK && len == sizeof(st) && st.magic == CFG_MAGIC) {
            s_cur = st.s;
            ESP_LOGI(TAG, "Da nap cai dat tu NVS");
        }
        nvs_close(h);
    }
    settings_sanitize(&s_cur);
}

void settings_get(DisplaySettings *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_cur;
    xSemaphoreGive(s_lock);
}

void settings_set(const DisplaySettings *in)
{
    DisplaySettings tmp = *in;
    settings_sanitize(&tmp);
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_cur = tmp;
    s_version++;
    xSemaphoreGive(s_lock);
    save_nvs(&tmp);
}

void settings_reset(void)
{
    DisplaySettings d;
    load_defaults(&d);
    settings_set(&d);
}

uint32_t settings_version(void) { return s_version; }

void settings_notify(const char *msg, uint32_t ms)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    strncpy(s_notice, msg, sizeof(s_notice) - 1);
    s_notice[sizeof(s_notice) - 1] = '\0';
    s_notice_until = esp_timer_get_time() + (int64_t)ms * 1000;
    s_version++;
    xSemaphoreGive(s_lock);
}

bool settings_get_notice(char *out, int len)
{
    bool active;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    active = esp_timer_get_time() < s_notice_until;
    if (active) { strncpy(out, s_notice, len - 1); out[len - 1] = '\0'; }
    xSemaphoreGive(s_lock);
    return active;
}
