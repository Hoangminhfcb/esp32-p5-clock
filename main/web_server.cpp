#include "web_server.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "app_config.h"
#include "settings.h"
#include "font.h"
#include "text_renderer.h"

static const char *TAG = "web";

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[]   asm("_binary_index_html_end");

// ---------------------------------------------------------------------
//  Tien ich
// ---------------------------------------------------------------------

// Giai ma URL: "%41+b" -> "A b"
static void url_decode(char *s)
{
    char *o = s;
    for (; *s; s++) {
        if (*s == '+') *o++ = ' ';
        else if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) {
            char hex[3] = { s[1], s[2], 0 };
            *o++ = (char)strtol(hex, nullptr, 16);
            s += 2;
        } else *o++ = *s;
    }
    *o = '\0';
}

// Lay gia tri 1 key trong chuoi "a=1&b=2" (da giai ma). true neu co key
static bool form_get(const char *body, const char *key, char *out, size_t len)
{
    if (httpd_query_key_value(body, key, out, len) != ESP_OK) return false;
    url_decode(out);
    return true;
}

static esp_err_t send_json(httpd_req_t *req, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, json);
}

// ---------------------------------------------------------------------
//  Handlers
// ---------------------------------------------------------------------

static esp_err_t h_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, index_html_start, index_html_end - index_html_start - 1);
}

// Them chuoi vao buffer dong (tu noi rong)
struct Buf {
    char  *p = nullptr;
    size_t n = 0, cap = 0;
    bool add(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
    bool str(const char *s);
    ~Buf() { free(p); }
};

bool Buf::add(const char *fmt, ...)
{
    for (int tries = 0; tries < 2; tries++) {
        va_list ap;
        va_start(ap, fmt);
        int need = vsnprintf(p ? p + n : nullptr, p ? cap - n : 0, fmt, ap);
        va_end(ap);
        if (need < 0) return false;
        if (p && n + need < cap) { n += need; return true; }
        size_t ncap = (cap ? cap * 2 : 1024) + need;
        char *np = (char *)realloc(p, ncap);
        if (!np) return false;
        p = np; cap = ncap;
    }
    return false;
}

bool Buf::str(const char *s)
{
    if (!add("\"")) return false;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { if (!add("\\%c", c)) return false; }
        else if (c < 0x20)         { if (!add("\\u%04x", c)) return false; }
        else                       { if (!add("%c", c)) return false; }
    }
    return add("\"");
}

static esp_err_t h_state(httpd_req_t *req)
{
    DisplaySettings s;
    settings_get(&s);

    Buf b;
    b.add("{\"panel\":{\"w\":%d,\"h\":%d},\"lines\":%d,\"brightness\":%d,\"fx\":%d,\"maxBytes\":%d,\"gap\":%d,\"line\":[",
          PANEL_W, PANEL_H, s.lines, s.brightness, s.fx_speed, LINE_TEXT_MAX, LINE_GAP);
    for (int i = 0; i < MAX_LINES; i++) {
        const LineSettings &l = s.line[i];
        b.add("%s{\"text\":", i ? "," : "");
        b.str(l.text);
        b.add(",\"font\":%d,\"scale\":%d,\"align\":%d,\"effect\":%d,\"color\":\"#%06lx\",\"speed\":%d}",
              l.font, l.scale, l.align, l.effect, (unsigned long)l.color, l.speed_ms);
    }
    b.add("],\"fonts\":[");
    for (int i = 0; i < font_count(); i++) {
        b.add("%s{\"id\":%d,\"name\":", i ? "," : "", i);
        b.str(font_get(i)->name);
        b.add(",\"h\":%d}", font_get(i)->ascii_bottom - font_get(i)->ascii_top + 1);
    }
    b.add("],\"ext\":[");
    for (int i = 0; i < FONT_EXT_COUNT; i++) b.add("%s%u", i ? "," : "", FONT_EXT_CODES[i]);
    if (!b.add("]}")) return httpd_resp_send_500(req);
    return send_json(req, b.p);
}

// {"h":22,"at":7,"ab":21,"w":[do rong tung ky tu],"c":[tat ca cot]}
static esp_err_t h_font(httpd_req_t *req)
{
    char q[32], v[8];
    int id = 0;
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK &&
        httpd_query_key_value(q, "id", v, sizeof(v)) == ESP_OK) id = atoi(v);
    const Font *f = font_get(id);
    const int count = FONT_ASCII_COUNT + FONT_EXT_COUNT;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");
    char chunk[768];
    int n = snprintf(chunk, sizeof(chunk), "{\"h\":%d,\"at\":%d,\"ab\":%d,\"w\":[", f->height, f->ascii_top, f->ascii_bottom);
    for (int i = 0; i < count; i++) {
        n += snprintf(chunk + n, sizeof(chunk) - n, "%s%d", i ? "," : "", f->glyphs[i].width);
        if (n > (int)sizeof(chunk) - 24) { httpd_resp_send_chunk(req, chunk, n); n = 0; }
    }
    n += snprintf(chunk + n, sizeof(chunk) - n, "],\"c\":[");
    const int total = f->glyphs[count - 1].offset + f->glyphs[count - 1].width;
    for (int i = 0; i < total; i++) {
        n += snprintf(chunk + n, sizeof(chunk) - n, "%s%lu", i ? "," : "", (unsigned long)f->cols[i]);
        if (n > (int)sizeof(chunk) - 24) { httpd_resp_send_chunk(req, chunk, n); n = 0; }
    }
    n += snprintf(chunk + n, sizeof(chunk) - n, "]}");
    httpd_resp_send_chunk(req, chunk, n);
    return httpd_resp_send_chunk(req, nullptr, 0);
}

// Chep chu UTF-8: bo ky tu dieu khien, cat dung ranh gioi ky tu
static void copy_text(char *dst, const char *src)
{
    int j = 0;
    for (int i = 0; src[i]; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x20 || c == 0x7F) continue;
        if (j >= LINE_TEXT_MAX) break;
        dst[j++] = c;
    }
    // neu cat giua 1 ky tu nhieu byte -> lui ve dau ky tu do
    if (j >= LINE_TEXT_MAX) {
        int k = j;
        while (k > 0 && ((unsigned char)dst[k - 1] & 0xC0) == 0x80) k--;
        if (k > 0 && ((unsigned char)dst[k - 1] & 0x80)) {
            unsigned char lead = dst[k - 1];
            int len = (lead & 0xE0) == 0xC0 ? 2 : (lead & 0xF0) == 0xE0 ? 3 : 4;
            if (j - (k - 1) < len) j = k - 1;
        }
    }
    dst[j] = '\0';
}

static esp_err_t h_set(httpd_req_t *req)
{
    const int max_body = MAX_LINES * (LINE_TEXT_MAX * 3 + 200) + 200;   // chu bi ma hoa %XX
    if (req->content_len <= 0 || (int)req->content_len > max_body) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "body qua dai");
        return ESP_FAIL;
    }
    char *body = (char *)calloc(1, req->content_len + 1);
    char *val  = (char *)malloc(LINE_TEXT_MAX * 3 + 8);
    if (!body || !val) { free(body); free(val); return httpd_resp_send_500(req); }

    int got = 0;
    while (got < (int)req->content_len) {
        int r = httpd_req_recv(req, body + got, req->content_len - got);
        if (r <= 0) { if (r == HTTPD_SOCK_ERR_TIMEOUT) continue; free(body); free(val); return ESP_FAIL; }
        got += r;
    }

    DisplaySettings s;
    settings_get(&s);                                  // chi doi cac key co gui len
    if (form_get(body, "lines", val, 16))      s.lines      = atoi(val);
    if (form_get(body, "brightness", val, 16)) s.brightness = atoi(val);
    if (form_get(body, "fx", val, 16))         s.fx_speed   = atoi(val);
    for (int i = 0; i < MAX_LINES; i++) {
        LineSettings &l = s.line[i];
        char key[16];
        snprintf(key, sizeof(key), "text%d", i);
        if (form_get(body, key, val, LINE_TEXT_MAX * 3 + 8)) copy_text(l.text, val);
        snprintf(key, sizeof(key), "font%d", i);   if (form_get(body, key, val, 16)) l.font     = atoi(val);
        snprintf(key, sizeof(key), "scale%d", i);  if (form_get(body, key, val, 16)) l.scale    = atoi(val);
        snprintf(key, sizeof(key), "align%d", i);  if (form_get(body, key, val, 16)) l.align    = atoi(val);
        snprintf(key, sizeof(key), "effect%d", i); if (form_get(body, key, val, 16)) l.effect   = atoi(val);
        snprintf(key, sizeof(key), "speed%d", i);  if (form_get(body, key, val, 16)) l.speed_ms = atoi(val);
        snprintf(key, sizeof(key), "color%d", i);
        if (form_get(body, key, val, 16)) l.color = strtoul(val[0] == '#' ? val + 1 : val, nullptr, 16);
    }
    free(body);
    free(val);

    settings_set(&s);
    ESP_LOGI(TAG, "Cap nhat: %d hang, \"%s\" / \"%s\"", s.lines, s.line[0].text, s.line[1].text);
    return h_state(req);
}

static esp_err_t h_reset(httpd_req_t *req)
{
    settings_reset();
    return h_state(req);
}

// ---------------------------------------------------------------------

void web_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 6144;
    cfg.core_id    = 0;                 // de core 1 cho task ve panel
    cfg.lru_purge_enable = true;

    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Khong khoi dong duoc web server");
        return;
    }
    const httpd_uri_t routes[] = {
        { .uri = "/",          .method = HTTP_GET,  .handler = h_index, .user_ctx = nullptr },
        { .uri = "/api/state", .method = HTTP_GET,  .handler = h_state, .user_ctx = nullptr },
        { .uri = "/api/font",  .method = HTTP_GET,  .handler = h_font,  .user_ctx = nullptr },
        { .uri = "/api/set",   .method = HTTP_POST, .handler = h_set,   .user_ctx = nullptr },
        { .uri = "/api/reset", .method = HTTP_POST, .handler = h_reset, .user_ctx = nullptr },
    };
    for (auto &r : routes) httpd_register_uri_handler(server, &r);
    ESP_LOGI(TAG, "Web server san sang");
}
