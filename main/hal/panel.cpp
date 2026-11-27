#include "hal/panel.h"
#include "config/app_config.h"
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

static MatrixPanel_I2S_DMA *s_panel = nullptr;

void panel_init(void)
{
    HUB75_I2S_CFG::i2s_pins pins = PANEL_PINS;
    HUB75_I2S_CFG cfg(PANEL_W, PANEL_H, PANEL_CHAIN, pins);

    cfg.double_buff = true;                     // vẽ vào buffer phụ, xong mới flip -> hết chớp
    cfg.i2sspeed    = HUB75_I2S_CFG::HZ_10M;    // hạ clock cho tín hiệu sạch
    cfg.clkphase    = false;                    // đổi cạnh clock
    cfg.driver      = HUB75_I2S_CFG::FM6126A;   // chip driver của panel

    s_panel = new MatrixPanel_I2S_DMA(cfg);
    s_panel->begin();                           // từ đây DMA tự quét panel, CPU rảnh
    s_panel->setBrightness8(DEFAULT_BRIGHTNESS);
    s_panel->setLatBlanking(2);                 // 1..4, chống bóng mờ ngang

    // xóa cả 2 buffer
    s_panel->clearScreen();
    s_panel->flipDMABuffer();
    s_panel->clearScreen();
}

void panel_set_brightness(uint8_t value) { s_panel->setBrightness8(value); }

void panel_clear(void) { s_panel->clearScreen(); }

void panel_fill(uint8_t r, uint8_t g, uint8_t b) { s_panel->fillScreenRGB888(r, g, b); }

void panel_set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || y < 0 || x >= PANEL_W || y >= PANEL_H) return;
    s_panel->drawPixelRGB888(x, y, r, g, b);
}

void panel_present(void) { s_panel->flipDMABuffer(); }
