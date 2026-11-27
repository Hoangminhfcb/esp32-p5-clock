// =====================================================================
//  font.h - Dinh dang font bitmap dung chung
//  - Moi ky tu co do rong rieng (font tu le / proportional)
//  - Moi cot 1 uint32_t, bit0 = hang tren cung -> font cao toi da 32 px
//  - Ky tu: ASCII 0x20..0x7E, sau do 134 chu tieng Viet (FONT_EXT_CODES)
//  - Cac hang tren cung chi dung cho dau tieng Viet; chu ASCII nam trong
//    hang ascii_top..ascii_bottom -> chu khong dau van gon, can giua dep.
// =====================================================================
#pragma once
#include <stdint.h>

struct FontGlyph {
    uint16_t offset;   // vi tri cot dau tien trong mang cols[]
    uint8_t  width;    // so cot (da gom khoang cach sau ky tu)
};

struct Font {
    const char      *name;
    uint8_t          height;        // tong so hang (gom cho dau tieng Viet)
    uint8_t          ascii_top;     // hang cao nhat co muc cua chu ASCII
    uint8_t          ascii_bottom;  // hang thap nhat co muc cua chu ASCII
    const FontGlyph *glyphs;        // [0..94] ASCII, [95..] FONT_EXT_CODES
    const uint32_t  *cols;
};

#define FONT_ASCII_FIRST  0x20
#define FONT_ASCII_LAST   0x7E
#define FONT_ASCII_COUNT  (FONT_ASCII_LAST - FONT_ASCII_FIRST + 1)
#define FONT_EXT_COUNT    134
extern const uint16_t FONT_EXT_CODES[FONT_EXT_COUNT];

int         font_count(void);
const Font *font_get(int id);   // id sai -> tra ve font 0
