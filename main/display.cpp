#include "display.h"
#include "app_config.h"

MatrixPanel_I2S_DMA *display = nullptr;

void display_setup(void)
{
    HUB75_I2S_CFG::i2s_pins pins = PANEL_PINS;
    HUB75_I2S_CFG mxconfig(PANEL_W, PANEL_H, PANEL_CHAIN, pins);

    mxconfig.double_buff = true;                    // ve vao buffer phu, xong moi flip -> het chop
    mxconfig.i2sspeed    = HUB75_I2S_CFG::HZ_10M;    // ha clock cho tin hieu sach
    mxconfig.clkphase    = false;                   // doi canh clock
    mxconfig.driver      = HUB75_I2S_CFG::FM6126A;  // chip driver cua panel

    display = new MatrixPanel_I2S_DMA(mxconfig);
    display->begin();                               // tu day DMA tu quet panel, CPU ranh
    display->setBrightness8(DEFAULT_BRIGHTNESS);    // 0..255
    display->setLatBlanking(2);                     // 1..4, chong bong mo ngang
    display->clearScreen();
    present();
    display->clearScreen();
}

void display_set_brightness(uint8_t value)
{
    display->setBrightness8(value);
}
