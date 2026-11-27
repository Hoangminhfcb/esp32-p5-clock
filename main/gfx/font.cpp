#include "gfx/font.h"

const Font *font_get(int family, int size)
{
    const FontFamily *fam = font_family(family);
    int i = size - fam->min_size;
    if (i < 0) i = 0;
    if (i >= fam->count) i = fam->count - 1;
    return fam->sizes[i];
}

const FontGlyph *font_glyph(const Font *f, uint32_t cp)
{
    if (cp >= FONT_ASCII_FIRST && cp <= FONT_ASCII_LAST)
        return &f->glyphs[cp - FONT_ASCII_FIRST];

    int lo = 0, hi = FONT_EXT_COUNT - 1;                 // tìm nhị phân
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (FONT_EXT_CODES[mid] == cp) return &f->glyphs[FONT_ASCII_COUNT + mid];
        if (FONT_EXT_CODES[mid] < cp) lo = mid + 1;
        else                          hi = mid - 1;
    }
    return &f->glyphs['?' - FONT_ASCII_FIRST];
}
