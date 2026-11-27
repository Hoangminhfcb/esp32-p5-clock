#include "net/http_util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

char *http_read_body(httpd_req_t *req, int max_len)
{
    if (req->content_len <= 0 || (int)req->content_len > max_len) return nullptr;
    char *body = (char *)calloc(1, req->content_len + 1);
    if (!body) return nullptr;
    int got = 0, timeouts = 0;
    while (got < (int)req->content_len) {
        const int r = httpd_req_recv(req, body + got, req->content_len - got);
        // Điện thoại rớt sóng giữa chừng: chỉ chờ thêm vài lần. Trước đây chờ mãi
        // -> web server (chỉ 1 luồng) bị treo, mọi máy khác cũng load mãi không xong.
        if (r == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < 3) continue;
        if (r <= 0) { free(body); return nullptr; }
        got += r;
    }
    return body;
}

// "%41+b" -> "A b"
static void url_decode(char *s)
{
    char *o = s;
    for (; *s; s++) {
        if (*s == '+') {
            *o++ = ' ';
        } else if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) {
            const char hex[3] = { s[1], s[2], 0 };
            *o++ = (char)strtol(hex, nullptr, 16);
            s += 2;
        } else {
            *o++ = *s;
        }
    }
    *o = '\0';
}

bool form_get(const char *body, const char *key, char *out, size_t len)
{
    if (httpd_query_key_value(body, key, out, len) != ESP_OK) return false;
    url_decode(out);
    return true;
}

esp_err_t http_send_json(httpd_req_t *req, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, json);
}

// ---------------------------------------------------------------------
JsonBuf::~JsonBuf() { free(p_); }

bool JsonBuf::add(const char *fmt, ...)
{
    if (!ok_) return false;
    for (int tries = 0; tries < 2; tries++) {
        va_list ap;
        va_start(ap, fmt);
        const int need = vsnprintf(p_ ? p_ + n_ : nullptr, p_ ? cap_ - n_ : 0, fmt, ap);
        va_end(ap);
        if (need < 0) break;
        if (p_ && n_ + need < cap_) { n_ += need; return true; }
        const size_t ncap = (cap_ ? cap_ * 2 : 1024) + need;
        char *np = (char *)realloc(p_, ncap);
        if (!np) break;
        p_ = np;
        cap_ = ncap;
    }
    ok_ = false;
    return false;
}

bool JsonBuf::str(const char *s)
{
    add("\"");
    for (; *s; s++) {
        const unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') add("\\%c", c);
        else if (c < 0x20)         add("\\u%04x", c);
        else                       add("%c", c);
    }
    return add("\"");
}
