#include "net/web_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_app_desc.h"
#include "config/app_config.h"
#include "app/settings.h"
#include "app/link_state.h"
#include "app/profiles.h"
#include "app/slideshow.h"
#include "net/link.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gfx/font.h"
#include "gfx/utf8.h"
#include "gfx/text.h"
#include "net/http_util.h"

static const char *TAG = "api";

// ---------------------------------------------------------------------
//  GET /api/state
// ---------------------------------------------------------------------
esp_err_t api_get_state(httpd_req_t *req)
{
    // Trang web chỉnh nội dung ĐÃ LƯU (khi trình chiếu, panel đang hiện profile khác)
    DisplaySettings s;
    settings_get_saved(&s);

    const int canvas_w = link_canvas_width();         // 128 khi đang ghép 2 màn hình

    JsonBuf j;
    j.add("{\"version\":");
    j.str(esp_app_get_description()->version);
    j.add(",\"panel\":{\"w\":%d,\"h\":%d},\"maxLines\":%d,\"maxBytes\":%d",
          canvas_w, PANEL_H, MAX_LINES, LINE_TEXT_MAX);
    j.add(",\"lines\":%d,\"brightness\":%d,\"fx\":%d,\"line\":[", s.lines, s.brightness, s.fx_speed);
    for (int i = 0; i < MAX_LINES; i++) {
        const LineSettings &l = s.line[i];
        const TextMetrics m = text_measure(font_get(l.font, l.size), l.text);
        j.add("%s{\"text\":", i ? "," : "");
        j.str(l.text);
        j.add(",\"font\":%d,\"size\":%d,\"align\":%d,\"effect\":%d,\"color\":\"#%06lx\",\"speed\":%d",
              l.font, l.size, l.align, l.effect, (unsigned long)l.color, l.speed_ms);
        // kích thước thật trên panel -> web báo "đứng yên" hay "chạy chữ"
        j.add(",\"width\":%d,\"height\":%d,\"scroll\":%s}", m.width, m.height, m.width > canvas_w ? "true" : "false");
    }
    j.add("],\"fonts\":[");
    for (int i = 0; i < font_family_count(); i++) {
        j.add("%s{\"id\":%d,\"name\":", i ? "," : "", i);
        j.str(font_family(i)->name);
        j.add(",\"min\":%d,\"max\":%d}", font_min_size(i), font_max_size(i));
    }
    j.add("]}");

    if (!j.ok()) return httpd_resp_send_500(req);
    return http_send_json(req, j.c_str());
}

// ---------------------------------------------------------------------
//  POST /api/set   (form: lines, brightness, fx,
//                         text0, font0, size0, align0, effect0, color0, speed0, text1 ...)
// ---------------------------------------------------------------------

// Chép chữ: bỏ ký tự điều khiển, cắt đúng ranh giới ký tự UTF-8
static void copy_text(char *dst, const char *src)
{
    char tmp[LINE_TEXT_MAX + 4];                  // dư vài byte để biết chỗ cắt
    int j = 0;
    for (int i = 0; src[i] && j < (int)sizeof(tmp) - 1; i++) {
        const unsigned char c = (unsigned char)src[i];
        if (c >= 0x20 && c != 0x7F) tmp[j++] = c;
    }
    tmp[j] = '\0';
    utf8_truncate(tmp, LINE_TEXT_MAX);
    strcpy(dst, tmp);
}

static bool form_int(const char *body, const char *key, int idx, int *out)
{
    char k[16], v[16];
    if (idx >= 0) snprintf(k, sizeof(k), "%s%d", key, idx);
    else          snprintf(k, sizeof(k), "%s", key);
    if (!form_get(body, k, v, sizeof(v))) return false;
    *out = (int)strtol(v[0] == '#' ? v + 1 : v, nullptr, strcmp(key, "color") == 0 ? 16 : 10);
    return true;
}

// Đọc các key nội dung (lines, fx, textN, fontN ...) từ form vào *s.
// Trả về true nếu có ít nhất 1 key nội dung (khác "brightness").
static bool parse_content(const char *body, DisplaySettings *s, char *text, int text_len)
{
    bool content = false;
    int v;
    if (form_int(body, "lines", -1, &v)) { s->lines    = v; content = true; }
    if (form_int(body, "fx", -1, &v))    { s->fx_speed = v; content = true; }
    for (int i = 0; i < MAX_LINES; i++) {
        LineSettings &l = s->line[i];
        char key[16];
        snprintf(key, sizeof(key), "text%d", i);
        if (form_get(body, key, text, text_len)) { copy_text(l.text, text); content = true; }
        if (form_int(body, "font", i, &v))   { l.font     = v; content = true; }
        if (form_int(body, "size", i, &v))   { l.size     = v; content = true; }
        if (form_int(body, "align", i, &v))  { l.align    = v; content = true; }
        if (form_int(body, "effect", i, &v)) { l.effect   = v; content = true; }
        if (form_int(body, "speed", i, &v))  { l.speed_ms = v; content = true; }
        if (form_int(body, "color", i, &v))  { l.color    = (uint32_t)v; content = true; }
    }
    return content;
}

static const int BODY_MAX     = MAX_LINES * (LINE_TEXT_MAX * 3 + 200) + 400;   // chữ bị mã hóa %XX
static const int TEXT_BUF_LEN = LINE_TEXT_MAX * 3 + 8;

esp_err_t api_post_set(httpd_req_t *req)
{
    char *body = http_read_body(req, BODY_MAX);
    if (!body) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "body loi hoac qua dai");
        return ESP_FAIL;
    }
    char *text = (char *)malloc(TEXT_BUF_LEN);
    DisplaySettings *s = new DisplaySettings;
    if (!text) { free(body); delete s; return httpd_resp_send_500(req); }

    settings_get_saved(s);                            // chỉ đổi các key có gửi lên
    const bool content = parse_content(body, s, text, TEXT_BUF_LEN);
    int v;
    const bool has_bright = form_int(body, "brightness", -1, &v);
    free(body);
    free(text);

    if (content) {
        if (has_bright) s->brightness = v;
        slideshow_stop();                             // gửi nội dung mới -> dừng trình chiếu
        settings_set(s);
        ESP_LOGI(TAG, "Cap nhat: %d hang", s->lines);
    } else if (has_bright) {
        settings_set_brightness(v < 0 ? 0 : v > 255 ? 255 : v);   // chỉ độ sáng: không dừng trình chiếu
    }
    delete s;
    return api_get_state(req);
}

// ---------------------------------------------------------------------
//  POST /api/reset
// ---------------------------------------------------------------------
esp_err_t api_post_reset(httpd_req_t *req)
{
    settings_reset();
    return api_get_state(req);
}

// ---------------------------------------------------------------------
//  GHÉP 2 MÀN HÌNH
//    GET  /api/link            trạng thái + danh sách mạch tìm thấy
//    POST /api/link/scan       tìm mạch xung quanh (~3 giây, web hỏi lại /api/link)
//    POST /api/link/pair       mac=aa:bb:cc:dd:ee:ff  -> mạch này làm master
//    POST /api/link/unpair     hủy ghép (cả 2 về master riêng)
//    POST /api/link/side       side=0 (master bên trái) | 1 (bên phải)
// ---------------------------------------------------------------------
static const char *role_name(uint8_t r)
{
    return r == LINK_MASTER ? "master" : r == LINK_SLAVE ? "slave" : "standalone";
}

esp_err_t api_get_link(httpd_req_t *req)
{
    LinkStatus st;
    link_status(&st);
    JsonBuf j;
    j.add("{\"self\":");
    j.str(link_device_id());
    j.add(",\"ssid\":");
    j.str(link_ap_ssid());
    j.add(",\"role\":\"%s\",\"side\":%d,\"peer\":", role_name(st.role), st.master_side);
    j.str(st.peer_id);
    j.add(",\"online\":%s,\"scanning\":%s,\"pairing\":%s,\"pairFailed\":%s,\"found\":[",
          st.peer_online ? "true" : "false", st.scanning ? "true" : "false",
          st.pairing ? "true" : "false", st.pair_failed ? "true" : "false");
    for (int i = 0; i < st.found_count; i++) {
        const uint8_t *m = st.found[i].mac;
        j.add("%s{\"id\":", i ? "," : "");
        j.str(st.found[i].id);
        j.add(",\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"rssi\":%d}",
              m[0], m[1], m[2], m[3], m[4], m[5], st.found[i].rssi);
    }
    j.add("]}");
    if (!j.ok()) return httpd_resp_send_500(req);
    return http_send_json(req, j.c_str());
}

// Lệnh chạy bất đồng bộ trong task link -> chờ chút cho kết quả kịp cập nhật
static esp_err_t link_reply(httpd_req_t *req)
{
    vTaskDelay(pdMS_TO_TICKS(100));
    return api_get_link(req);
}

esp_err_t api_post_link_scan(httpd_req_t *req)   { link_scan();   return link_reply(req); }
esp_err_t api_post_link_unpair(httpd_req_t *req) { link_unpair(); return link_reply(req); }

esp_err_t api_post_link_pair(httpd_req_t *req)
{
    char *body = http_read_body(req, 128);
    char v[64];                 // giá trị còn mã hóa URL ("aa%3Abb...") dài ~27 ký tự
    unsigned m[6];
    const bool ok = body && form_get(body, "mac", v, sizeof(v)) &&
                    sscanf(v, "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6;
    free(body);
    if (!ok) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "mac sai");
        return ESP_FAIL;
    }
    uint8_t mac[6];
    for (int i = 0; i < 6; i++) mac[i] = (uint8_t)m[i];
    link_pair(mac);
    return link_reply(req);
}

esp_err_t api_post_link_side(httpd_req_t *req)
{
    char *body = http_read_body(req, 64);
    int side = 0;
    if (body) form_int(body, "side", -1, &side);
    free(body);
    link_set_side(side ? 1 : 0);
    return link_reply(req);
}

// ---------------------------------------------------------------------
//  PROFILE + TRÌNH CHIẾU
//    GET  /api/profiles          danh sách profile + trạng thái trình chiếu
//    POST /api/profile/save      name, slot (bỏ trống = ô mới) + các key như /api/set
//    POST /api/profile/load      slot  -> hiện + lưu làm nội dung chính (dừng trình chiếu)
//    POST /api/profile/delete    slot
//    POST /api/show              on=0|1, items="slot:giây,slot:giây,..."
// ---------------------------------------------------------------------
esp_err_t api_get_profiles(httpd_req_t *req)
{
    ShowStatus st;
    slideshow_status(&st);
    JsonBuf j;
    j.add("{\"max\":%d,\"nameMax\":%d,\"items\":[", profiles_max(), PROFILE_NAME_MAX);
    bool first = true;
    char name[PROFILE_NAME_MAX + 1];
    for (int i = 0; i < profiles_max(); i++) {
        if (!profiles_name(i, name, sizeof(name))) continue;
        j.add("%s{\"slot\":%d,\"name\":", first ? "" : ",", i);
        j.str(name);
        j.add("}");
        first = false;
    }
    j.add("],\"show\":{\"on\":%s,\"cur\":%d,\"left\":%lu,\"min\":%d,\"maxSecs\":%d,\"items\":[",
          st.on ? "true" : "false", st.cur_slot, (unsigned long)st.left_ms, SHOW_SECS_MIN, SHOW_SECS_MAX);
    for (int i = 0; i < st.count; i++)
        j.add("%s{\"slot\":%d,\"secs\":%d}", i ? "," : "", st.items[i].slot, st.items[i].secs);
    j.add("]}}");
    if (!j.ok()) return httpd_resp_send_500(req);
    return http_send_json(req, j.c_str());
}

static int form_slot(const char *body)
{
    int v = -1;
    if (!body || !form_int(body, "slot", -1, &v)) return -1;
    return v;
}

esp_err_t api_post_profile_save(httpd_req_t *req)
{
    char *body = http_read_body(req, BODY_MAX);
    if (!body) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "body loi hoac qua dai");
        return ESP_FAIL;
    }
    char *text = (char *)malloc(TEXT_BUF_LEN);
    DisplaySettings *s = new DisplaySettings;
    if (!text) { free(body); delete s; return httpd_resp_send_500(req); }

    settings_get_saved(s);                            // key nào không gửi -> lấy theo nội dung hiện tại
    parse_content(body, s, text, TEXT_BUF_LEN);
    settings_sanitize(s);

    char name[PROFILE_NAME_MAX + 1] = "";
    if (form_get(body, "name", text, TEXT_BUF_LEN)) {
        copy_text(text, text);
        utf8_truncate(text, PROFILE_NAME_MAX);
        strcpy(name, text);
    }
    int slot = form_slot(body);
    if (slot >= profiles_max()) slot = -1;
    if (!name[0] && slot >= 0) profiles_name(slot, name, sizeof(name));   // lưu đè, giữ tên cũ
    free(body);
    free(text);

    if (!name[0]) {                                   // chưa đặt tên -> "Profile N"
        int n = 1;
        while (n <= profiles_max()) {
            snprintf(name, sizeof(name), "Profile %d", n);
            bool dup = false;
            char other[PROFILE_NAME_MAX + 1];
            for (int i = 0; i < profiles_max() && !dup; i++)
                dup = profiles_name(i, other, sizeof(other)) && strcmp(other, name) == 0;
            if (!dup) break;
            n++;
        }
    }

    const int saved = profiles_save(slot, name, s);
    delete s;
    if (saved < 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Het cho luu profile");
        return ESP_FAIL;
    }
    slideshow_on_profile_saved(saved);
    ESP_LOGI(TAG, "Luu profile %d: %s", saved, name);
    return api_get_profiles(req);
}

esp_err_t api_post_profile_load(httpd_req_t *req)
{
    char *body = http_read_body(req, 64);
    const int slot = form_slot(body);
    free(body);
    DisplaySettings *s = new DisplaySettings;
    if (!profiles_load(slot, s)) {
        delete s;
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Khong co profile nay");
        return ESP_FAIL;
    }
    DisplaySettings cur;
    settings_get_saved(&cur);
    s->brightness = cur.brightness;                   // độ sáng giữ nguyên
    slideshow_stop();
    settings_set(s);
    delete s;
    return api_get_state(req);
}

esp_err_t api_post_profile_delete(httpd_req_t *req)
{
    char *body = http_read_body(req, 64);
    const int slot = form_slot(body);
    free(body);
    if (profiles_delete(slot)) slideshow_on_profile_deleted(slot);
    return api_get_profiles(req);
}

esp_err_t api_post_show(httpd_req_t *req)
{
    char *body = http_read_body(req, 512);
    if (!body) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "body loi");
        return ESP_FAIL;
    }
    char v[400];
    if (form_get(body, "items", v, sizeof(v))) {      // "3:10,0:5" (có thể rỗng)
        ShowItem items[PROFILE_MAX];
        int n = 0;
        for (char *p = v; *p && n < PROFILE_MAX;) {
            char *end;
            const long slot = strtol(p, &end, 10);
            if (end == p) break;
            long secs = 10;
            if (*end == ':') secs = strtol(end + 1, &end, 10);
            if (slot >= 0 && slot < PROFILE_MAX) {
                items[n].slot = (uint8_t)slot;
                items[n].secs = (uint16_t)(secs < 0 ? 0 : secs > 65535 ? 65535 : secs);
                n++;
            }
            while (*end && *end != ',') end++;
            p = *end ? end + 1 : end;
        }
        slideshow_set_items(items, n);
    }
    int on;
    if (form_int(body, "on", -1, &on)) {
        if (on) slideshow_start();
        else    slideshow_stop();
    }
    free(body);
    vTaskDelay(pdMS_TO_TICKS(150));                   // chờ task trình chiếu đổi slide
    return api_get_profiles(req);
}
