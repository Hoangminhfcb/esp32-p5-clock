#include "hal/button.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "config/app_config.h"

static int64_t s_down_since = -1;     // thời điểm bắt đầu nhấn (us), -1 = đang nhả

void button_init(void)
{
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << LINK_BUTTON_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
}

uint32_t button_held_ms(void)
{
    const bool down = gpio_get_level((gpio_num_t)LINK_BUTTON_GPIO) == 0;   // nhấn = mức 0
    const int64_t now = esp_timer_get_time();
    if (!down) { s_down_since = -1; return 0; }
    if (s_down_since < 0) s_down_since = now;
    return (uint32_t)((now - s_down_since) / 1000);
}
