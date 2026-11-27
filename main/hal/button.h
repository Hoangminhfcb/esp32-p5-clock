// =====================================================================
//  hal/button.h — Nút BOOT trên mạch (LINK_BUTTON_GPIO)
//
//  Gọi button_held_ms() đều đặn (vd mỗi 50 ms): trả về số ms nút đang
//  được giữ liên tục, 0 nếu đang nhả.
// =====================================================================
#pragma once
#include <stdint.h>

void     button_init(void);
uint32_t button_held_ms(void);
