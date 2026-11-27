// =====================================================================
//  app/profiles.h — Profile: các bộ nội dung (chữ + kiểu + màu...) đã lưu
//
//  Mỗi profile = 1 tên + 1 DisplaySettings, lưu NVS (namespace "profiles",
//  key "p0".."p{N-1}"), nhớ khi mất điện. Có tối đa PROFILE_MAX ô (slot).
//  Tên các profile được giữ sẵn trong RAM; nội dung chỉ đọc từ NVS khi cần.
//
//  Trình chiếu (app/slideshow.h) chiếu lần lượt các profile này.
//  An toàn đa luồng.
// =====================================================================
#pragma once
#include <stdint.h>
#include "app/settings.h"

#define PROFILE_NAME_MAX 47      // byte UTF-8 (không kể ký tự kết thúc), ~20 chữ tiếng Việt

void profiles_init(void);                                    // gọi sau nvs_flash_init

int  profiles_max(void);                                      // = PROFILE_MAX
bool profiles_used(int slot);
bool profiles_name(int slot, char *out, int len);             // false nếu ô trống
bool profiles_load(int slot, DisplaySettings *out);           // false nếu ô trống / lỗi

// Lưu vào ô 'slot'; slot < 0 -> ô trống đầu tiên. Trả về ô đã lưu, -1 nếu hết chỗ / lỗi.
int  profiles_save(int slot, const char *name, const DisplaySettings *s);
bool profiles_delete(int slot);
