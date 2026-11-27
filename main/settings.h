// =====================================================================
//  settings.h - Cai dat hien thi (luu NVS, an toan giua cac task)
// =====================================================================
#pragma once
#include <stdint.h>
#include "app_config.h"

struct LineSettings {
    char     text[LINE_TEXT_MAX + 1];   // UTF-8, co the co dau tieng Viet
    uint8_t  font;         // id font (fonts/fonts.cpp)
    uint8_t  scale;        // 1..4  (size)
    uint8_t  align;        // 0 trai, 1 giua, 2 phai
    uint8_t  effect;       // 0 mot mau, 1 cau vong chay, 2 doi mau lien tuc
    uint32_t color;        // 0xRRGGBB (khi effect = 0)
    uint16_t speed_ms;     // ms / pixel khi chay chu
};

struct DisplaySettings {
    uint8_t      lines;        // 1 hoac 2
    uint8_t      brightness;   // 0..255
    uint8_t      fx_speed;     // 1..10 toc do doi mau
    LineSettings line[MAX_LINES];
};

void     settings_init(void);                        // nap tu NVS (goi sau nvs_flash_init)
void     settings_get(DisplaySettings *out);         // lay ban sao
void     settings_set(const DisplaySettings *in);    // kiem tra gia tri, ap dung, luu NVS
void     settings_reset(void);                       // ve mac dinh
void     settings_sanitize(DisplaySettings *s);      // ep gia tri hop le + chu vua chieu cao panel
uint32_t settings_version(void);                     // tang moi lan thay doi

// Thong bao ngan hien len panel (vd dia chi IP), tu tat sau ms
void     settings_notify(const char *msg, uint32_t ms);
bool     settings_get_notice(char *out, int len);    // true neu dang co thong bao
