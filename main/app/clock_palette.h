#pragma once
#include "gfx/color.h"
#include <stdint.h>
// Bốn màu gốc cho mỗi bộ; trộn vòng thành 24 màu giờ và 28 màu ngày.
struct ClockPalette { uint32_t time[4]; uint32_t date[4]; };
void clock_palette_init();
void clock_palette_get(ClockPalette *out);
bool clock_palette_store(const ClockPalette &palette);
static inline Rgb clock_palette_mix(const uint32_t colors[4],int64_t slot,int cycle) {
    const int position=static_cast<int>((slot%cycle+cycle)%cycle)*4*256/cycle;
    const int index=position/256, fraction=position%256;
    const Rgb a=rgb_from_hex(colors[index]),b=rgb_from_hex(colors[(index+1)%4]);
    return {static_cast<uint8_t>((a.r*(256-fraction)+b.r*fraction)/256),
            static_cast<uint8_t>((a.g*(256-fraction)+b.g*fraction)/256),
            static_cast<uint8_t>((a.b*(256-fraction)+b.b*fraction)/256)};
}
