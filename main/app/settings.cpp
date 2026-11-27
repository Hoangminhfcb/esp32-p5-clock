#include "app/settings.h"
#include <string.h>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "gfx/font.h"
#include "gfx/text.h"
#include "gfx/color.h"
#include "app/link_state.h"

static const char *TAG = "settings";

static SemaphoreHandle_t     s_lock;
static DisplaySettings       s_cur;       // đang hiện
static DisplaySettings       s_saved;     // đã lưu (NVS)
static std::atomic<uint32_t> s_version{1};
static std::atomic<int64_t>  s_epoch{0};

// ---------------------------------------------------------------------
//  Mặc định + kiểm tra giá trị
// ---------------------------------------------------------------------
static void line_defaults(LineSettings *l, const char *text, int font, int size, uint32_t color, int effect)
{
    memset(l, 0, sizeof(*l));
    strncpy(l->text, text, LINE_TEXT_MAX);
    l->font     = font;
    l->size     = size;
    l->align    = DEFAULT_ALIGN;
    l->effect   = effect;
    l->color    = color;
    l->speed_ms = DEFAULT_SPEED_MS;
}

void settings_defaults(DisplaySettings *s)
{
    memset(s, 0, sizeof(*s));
    s->lines      = DEFAULT_LINES;
    s->brightness = DEFAULT_BRIGHTNESS;
    s->fx_speed   = DEFAULT_FX_SPEED;
    line_defaults(&s->line[0], DEFAULT_TEXT_1, DEFAULT_FONT_1, DEFAULT_SIZE_1, DEFAULT_COLOR_1, DEFAULT_EFFECT_1);
    line_defaults(&s->line[1], DEFAULT_TEXT_2, DEFAULT_FONT_2, DEFAULT_SIZE_2, DEFAULT_COLOR_2, DEFAULT_EFFECT_2);
}

template <typename T> static inline void clamp(T &v, T lo, T hi) { v = v < lo ? lo : (v > hi ? hi : v); }

static int line_height(const LineSettings *l)
{
    return text_measure(font_get(l->font, l->size), l->text).height;
}

void settings_sanitize(DisplaySettings *s)
{
    clamp<uint8_t>(s->lines, 1, MAX_LINES);
    clamp<uint8_t>(s->fx_speed, 1, 10);
    for (auto &l : s->line) {
        l.text[LINE_TEXT_MAX] = '\0';
        if (l.font >= font_family_count()) l.font = 0;
        clamp<uint8_t>(l.size, font_min_size(l.font), font_max_size(l.font));
        if (l.align > 2)          l.align = DEFAULT_ALIGN;
        if (l.effect >= FX_COUNT) l.effect = FX_SOLID;
        l.color &= 0xFFFFFF;
        clamp<uint16_t>(l.speed_ms, 10, 500);
    }
    // Giảm cỡ hàng cao nhất (mỗi lần 1 px) cho đến khi tất cả các hàng vừa chiều cao panel
    while (true) {
        int total = (s->lines - 1) * LINE_GAP, big = -1, big_h = 0;
        for (int i = 0; i < s->lines; i++) {
            const LineSettings &l = s->line[i];
            const int h = line_height(&l);
            total += h;
            if (l.size > font_min_size(l.font) && h > big_h) { big = i; big_h = h; }
        }
        if (total <= PANEL_H || big < 0) break;
        s->line[big].size--;
    }
}

// ---------------------------------------------------------------------
//  API
// ---------------------------------------------------------------------
void settings_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (settings_nvs_load(&s_cur)) {
        ESP_LOGI(TAG, "Da nap cai dat tu NVS");
    } else {
        ESP_LOGI(TAG, "Dung cai dat mac dinh");
        settings_defaults(&s_cur);
    }
    settings_sanitize(&s_cur);
    s_saved = s_cur;
    s_epoch = link_now_ms();
}

void settings_get(DisplaySettings *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_cur;
    xSemaphoreGive(s_lock);
}

void settings_get_saved(DisplaySettings *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_saved;
    xSemaphoreGive(s_lock);
}

void settings_set(const DisplaySettings *in)
{
    DisplaySettings tmp = *in;
    settings_sanitize(&tmp);
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_cur = s_saved = tmp;
    s_epoch = link_now_ms();
    s_version++;
    xSemaphoreGive(s_lock);
    settings_nvs_save(&tmp);
}

void settings_show(const DisplaySettings *in)
{
    DisplaySettings tmp = *in;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    tmp.brightness = s_saved.brightness;          // độ sáng là của mạch, không theo slide
    xSemaphoreGive(s_lock);
    settings_sanitize(&tmp);                       // (đo chữ: làm ngoài khóa)
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_cur = tmp;
    s_epoch = link_now_ms();
    s_version++;
    xSemaphoreGive(s_lock);
}

void settings_restore(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_cur = s_saved;
    s_epoch = link_now_ms();
    s_version++;
    xSemaphoreGive(s_lock);
}

void settings_set_brightness(uint8_t value)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_cur.brightness = s_saved.brightness = value;
    const DisplaySettings tmp = s_saved;
    s_version++;                                   // epoch giữ nguyên -> chữ không chạy lại
    xSemaphoreGive(s_lock);
    settings_nvs_save(&tmp);
}

void settings_reset(void)
{
    DisplaySettings d;
    settings_defaults(&d);
    settings_set(&d);
}

void settings_touch(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_epoch = link_now_ms();
    s_version++;
    xSemaphoreGive(s_lock);
}

uint32_t settings_version(void) { return s_version; }
int64_t  settings_epoch(void)   { return s_epoch; }
