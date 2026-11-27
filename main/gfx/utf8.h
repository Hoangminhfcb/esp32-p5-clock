// =====================================================================
//  gfx/utf8.h — Đọc chuỗi UTF-8 (chữ tiếng Việt có dấu = 2-3 byte)
// =====================================================================
#pragma once
#include <stdint.h>

// Đọc 1 ký tự, tiến con trỏ *s. Byte lỗi -> trả về '?'
uint32_t utf8_next(const char **s);

// Dấu tổ hợp (U+0300..U+036F) — bỏ qua khi vẽ, vì font dùng chữ dựng sẵn
static inline bool utf8_is_combining(uint32_t cp) { return cp >= 0x0300 && cp <= 0x036F; }

// Cắt chuỗi về tối đa max_bytes mà không cắt đôi 1 ký tự. Trả về độ dài mới.
int utf8_truncate(char *s, int max_bytes);
