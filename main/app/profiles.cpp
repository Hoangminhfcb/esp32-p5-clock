#include "app/profiles.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs.h"
#include "config/app_config.h"

static const char    *TAG      = "profiles";
static const char    *NVS_NS   = "profiles";
static const uint32_t P_MAGIC  = 0x50524631;    // "PRF1" — đổi DisplaySettings thì tăng

struct Stored {
    uint32_t        magic;
    char            name[PROFILE_NAME_MAX + 1];
    DisplaySettings s;
};

static SemaphoreHandle_t s_lock;
static char              s_names[PROFILE_MAX][PROFILE_NAME_MAX + 1];
static bool              s_used[PROFILE_MAX];

static void key_of(int slot, char *k) { snprintf(k, 8, "p%d", slot); }
static bool valid(int slot) { return slot >= 0 && slot < PROFILE_MAX; }

// Đọc 1 ô từ NVS (gọi khi đang giữ khóa)
static bool read_slot(nvs_handle_t h, int slot, Stored *st)
{
    char k[8];
    key_of(slot, k);
    size_t len = sizeof(*st);
    if (nvs_get_blob(h, k, st, &len) != ESP_OK || len != sizeof(*st) || st->magic != P_MAGIC) return false;
    st->name[PROFILE_NAME_MAX] = '\0';
    return true;
}

void profiles_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    memset(s_used, 0, sizeof(s_used));
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;   // chưa có profile nào
    Stored *st = new Stored;
    int n = 0;
    for (int i = 0; i < PROFILE_MAX; i++) {
        if (!read_slot(h, i, st)) continue;
        strcpy(s_names[i], st->name);
        s_used[i] = true;
        n++;
    }
    delete st;
    nvs_close(h);
    ESP_LOGI(TAG, "Co %d profile", n);
}

int profiles_max(void) { return PROFILE_MAX; }

bool profiles_used(int slot)
{
    if (!valid(slot)) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const bool u = s_used[slot];
    xSemaphoreGive(s_lock);
    return u;
}

bool profiles_name(int slot, char *out, int len)
{
    if (!valid(slot) || len <= 0) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const bool u = s_used[slot];
    if (u) { strncpy(out, s_names[slot], len - 1); out[len - 1] = '\0'; }
    xSemaphoreGive(s_lock);
    return u;
}

bool profiles_load(int slot, DisplaySettings *out)
{
    if (!valid(slot)) return false;
    Stored *st = new Stored;
    bool ok = false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    nvs_handle_t h;
    if (s_used[slot] && nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        ok = read_slot(h, slot, st);
        nvs_close(h);
    }
    xSemaphoreGive(s_lock);
    if (ok) *out = st->s;
    delete st;
    return ok;
}

int profiles_save(int slot, const char *name, const DisplaySettings *s)
{
    Stored *st = new Stored;
    memset(st, 0, sizeof(*st));
    st->magic = P_MAGIC;
    strncpy(st->name, name, PROFILE_NAME_MAX);
    st->s = *s;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (slot < 0)
        for (int i = 0; i < PROFILE_MAX; i++) if (!s_used[i]) { slot = i; break; }
    bool ok = false;
    nvs_handle_t h;
    if (valid(slot) && nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        char k[8];
        key_of(slot, k);
        ok = nvs_set_blob(h, k, st, sizeof(*st)) == ESP_OK && nvs_commit(h) == ESP_OK;
        nvs_close(h);
    }
    if (ok) {
        strcpy(s_names[slot], st->name);
        s_used[slot] = true;
    }
    xSemaphoreGive(s_lock);
    delete st;
    if (!ok) ESP_LOGW(TAG, "Khong luu duoc profile (o %d)", slot);
    return ok ? slot : -1;
}

bool profiles_delete(int slot)
{
    if (!valid(slot)) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        char k[8];
        key_of(slot, k);
        nvs_erase_key(h, k);
        nvs_commit(h);
        nvs_close(h);
    }
    const bool was = s_used[slot];
    s_used[slot] = false;
    s_names[slot][0] = '\0';
    xSemaphoreGive(s_lock);
    return was;
}
