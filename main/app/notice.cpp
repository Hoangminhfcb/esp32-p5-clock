#include "app/notice.h"
#include <string.h>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

static SemaphoreHandle_t     s_lock;
static char                  s_msg[48];
static int64_t               s_until_us = 0;
static std::atomic<uint32_t> s_version{0};

void notice_init(void) { s_lock = xSemaphoreCreateMutex(); }

void notice_show(const char *msg, uint32_t ms)
{
    if (!msg || !msg[0] || ms == 0) return;        // chuỗi rỗng -> không hiện gì
    xSemaphoreTake(s_lock, portMAX_DELAY);
    strncpy(s_msg, msg, sizeof(s_msg) - 1);
    s_msg[sizeof(s_msg) - 1] = '\0';
    s_until_us = esp_timer_get_time() + (int64_t)ms * 1000;
    s_version++;
    xSemaphoreGive(s_lock);
}

bool notice_get(char *out, int len)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const bool active = esp_timer_get_time() < s_until_us;
    if (active) { strncpy(out, s_msg, len - 1); out[len - 1] = '\0'; }
    xSemaphoreGive(s_lock);
    return active;
}

uint32_t notice_version(void) { return s_version; }
