// =====================================================================
//  net/http_util.h — Tiện ích HTTP dùng chung: đọc form, dựng JSON
// =====================================================================
#pragma once
#include <stddef.h>
#include "esp_http_server.h"

// Đọc toàn bộ body request (malloc, nhớ free). nullptr nếu lỗi / quá max_len.
char *http_read_body(httpd_req_t *req, int max_len);

// Lấy giá trị của key trong chuỗi form "a=1&b=x%20y" (đã giải mã URL)
bool  form_get(const char *body, const char *key, char *out, size_t len);

// Gửi chuỗi JSON (không cache)
esp_err_t http_send_json(httpd_req_t *req, const char *json);

// Bộ đệm dựng chuỗi JSON, tự nới rộng
class JsonBuf {
public:
    ~JsonBuf();
    bool add(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
    bool str(const char *s);                 // chuỗi có ngoặc kép + escape
    const char *c_str() const { return p_ ? p_ : ""; }
    bool ok() const { return ok_; }
private:
    char  *p_ = nullptr;
    size_t n_ = 0, cap_ = 0;
    bool   ok_ = true;
};
