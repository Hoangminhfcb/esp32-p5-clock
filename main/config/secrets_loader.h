// =====================================================================
//  secrets_loader.h — Nạp secrets.h; nếu chưa có thì dùng file mẫu
//  (để người mới clone về vẫn build được ngay).
// =====================================================================
#pragma once

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "Chua co config/secrets.h -> dang dung secrets.example.h. Hay copy va sua lai."
#include "secrets.example.h"
#endif
