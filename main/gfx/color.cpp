#include "gfx/color.h"
#include "config/app_config.h"

Rgb hue_to_rgb(uint8_t hue)
{
    const uint8_t region = hue / 43;
    const uint8_t up = (hue - region * 43) * 6, down = 255 - up;
    switch (region) {
        case 0:  return { 255, up, 0 };
        case 1:  return { down, 255, 0 };
        case 2:  return { 0, 255, up };
        case 3:  return { 0, down, 255 };
        case 4:  return { up, 0, 255 };
        default: return { 255, 0, down };
    }
}

Rgb paint_color_at(const Paint &p, int x)
{
    switch (p.effect) {
        case FX_RAINBOW:                        // 1 vòng màu trải hết chiều ngang panel
            if (x < 0) x = 0;
            return hue_to_rgb((uint8_t)(p.hue + x * 256 / PANEL_W));
        case FX_CYCLE:
            return hue_to_rgb(p.hue);
        case FX_SOLID:
        default:
            return p.color;
    }
}
