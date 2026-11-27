// Lưu / nạp DisplaySettings vào NVS dưới dạng 1 blob.
// Đổi cấu trúc DisplaySettings -> tăng CFG_MAGIC để bỏ dữ liệu cũ.
#include "app/settings.h"
#include "nvs.h"

static const char    *NVS_NS    = "display";
static const char    *NVS_KEY   = "cfg";
static const uint32_t CFG_MAGIC = 0x48554234;   // đổi -> nạp lại mặc định (căn trái)

struct Stored { uint32_t magic; DisplaySettings s; };

bool settings_nvs_load(DisplaySettings *out)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    Stored st;
    size_t len = sizeof(st);
    const bool ok = nvs_get_blob(h, NVS_KEY, &st, &len) == ESP_OK && len == sizeof(st) && st.magic == CFG_MAGIC;
    nvs_close(h);
    if (ok) *out = st.s;
    return ok;
}

void settings_nvs_save(const DisplaySettings *s)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    const Stored st = { CFG_MAGIC, *s };
    nvs_set_blob(h, NVS_KEY, &st, sizeof(st));
    nvs_commit(h);
    nvs_close(h);
}
