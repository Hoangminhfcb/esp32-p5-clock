// =====================================================================
//  app/notice.h — Thông báo ngắn hiện đè lên panel (vd địa chỉ IP)
//  Hết thời gian thì panel tự quay về nội dung bình thường.
// =====================================================================
#pragma once
#include <stdint.h>

void     notice_init(void);
void     notice_show(const char *msg, uint32_t ms);
bool     notice_get(char *out, int len);    // true nếu đang có thông báo
uint32_t notice_version(void);              // tăng mỗi lần có thông báo mới
