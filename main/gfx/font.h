// =====================================================================
//  gfx/font.h — Font bitmap: KIỂU CHỮ (family) x CỠ CHỮ (size)
//
//  - Cỡ chữ = chiều cao chữ IN HOA (px). Mỗi cỡ là 1 bitmap riêng được
//    render sẵn, nên tăng/giảm cỡ đúng 1 px mà chữ vẫn sắc nét.
//  - Mỗi ký tự có độ rộng riêng; mỗi cột 1 uint32_t (bit0 = hàng trên) -> cao ≤ 32 px
//  - Ký tự: ASCII 0x20..0x7E, tiếp theo là 134 chữ tiếng Việt (FONT_EXT_CODES)
//  - Chữ ASCII nằm trong hàng ascii_top..ascii_bottom; các hàng trên đó
//    chỉ dùng cho dấu tiếng Việt -> chữ không dấu vẫn gọn, căn giữa đẹp.
//
//  Dữ liệu font nằm trong gfx/fonts/, SINH TỰ ĐỘNG bởi
//  tools/gen_fonts/gen_fonts.py — không sửa tay.
// =====================================================================
#pragma once
#include <stdint.h>

struct FontGlyph {
    uint16_t offset;   // vị trí cột đầu tiên trong mảng cols[]
    uint8_t  width;    // số cột (đã gồm khoảng cách sau ký tự)
};

// 1 kiểu chữ ở 1 cỡ
struct Font {
    uint8_t          height;        // tổng số hàng (gồm chỗ cho dấu tiếng Việt)
    uint8_t          ascii_top;     // hàng cao nhất có mực của chữ ASCII
    uint8_t          ascii_bottom;  // hàng thấp nhất có mực của chữ ASCII
    const FontGlyph *glyphs;        // [0..94] ASCII, [95..] theo FONT_EXT_CODES
    const uint32_t  *cols;
};

// 1 kiểu chữ ở mọi cỡ: sizes[i] là cỡ min_size + i
struct FontFamily {
    const char        *name;
    uint8_t            min_size;
    uint8_t            count;
    const Font *const *sizes;
};

#define FONT_ASCII_FIRST  0x20
#define FONT_ASCII_LAST   0x7E
#define FONT_ASCII_COUNT  (FONT_ASCII_LAST - FONT_ASCII_FIRST + 1)
#define FONT_EXT_COUNT    134
#define FONT_GLYPH_COUNT  (FONT_ASCII_COUNT + FONT_EXT_COUNT)

// Mã Unicode của các ký tự mở rộng (sau ASCII), sắp xếp tăng dần
extern const uint16_t FONT_EXT_CODES[FONT_EXT_COUNT];

int               font_family_count(void);
const FontFamily *font_family(int id);                       // id sai -> kiểu 0

static inline int font_min_size(int family) { return font_family(family)->min_size; }
static inline int font_max_size(int family) { const FontFamily *f = font_family(family); return f->min_size + f->count - 1; }

const Font       *font_get(int family, int size);            // cỡ ngoài khoảng -> gần nhất
const FontGlyph  *font_glyph(const Font *f, uint32_t cp);    // ký tự không có -> '?'
