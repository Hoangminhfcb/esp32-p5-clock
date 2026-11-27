#include "net/web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "net/web_api.h"
#include "config/app_config.h"

static const char *TAG = "web";

// ---------------------------------------------------------------------
//  File tĩnh nhúng bằng EMBED_TXTFILES (main/CMakeLists.txt)
// ---------------------------------------------------------------------
extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[]   asm("_binary_index_html_end");
extern const char style_css_start[]  asm("_binary_style_css_start");
extern const char style_css_end[]    asm("_binary_style_css_end");
extern const char app_js_start[]     asm("_binary_app_js_start");
extern const char app_js_end[]       asm("_binary_app_js_end");

struct StaticFile {
    const char *uri;
    const char *type;
    const char *start;
    const char *end;
};

static const StaticFile FILES[] = {
    { "/",          "text/html; charset=utf-8",              index_html_start, index_html_end },
    { "/style.css", "text/css; charset=utf-8",               style_css_start,  style_css_end  },
    { "/app.js",    "application/javascript; charset=utf-8", app_js_start,     app_js_end     },
};

static esp_err_t serve_static(httpd_req_t *req)
{
    const auto *f = (const StaticFile *)req->user_ctx;
    httpd_resp_set_type(req, f->type);
    return httpd_resp_send(req, f->start, f->end - f->start - 1);   // -1: bỏ '\0' cuối
}

// ---------------------------------------------------------------------
//  Cổng đăng nhập (captive portal): trang lạ -> chuyển về trang điều khiển.
//  Điện thoại kiểm tra Internet bằng các trang như /generate_204 (Android),
//  /hotspot-detect.html (iPhone), /connecttest.txt (Windows)... Nhận được lệnh
//  chuyển trang -> máy hiểu đây là "WiFi cần đăng nhập" và mở trang của ta.
// ---------------------------------------------------------------------
static esp_err_t redirect_home(httpd_req_t *req, httpd_err_code_t)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, nullptr, 0);
}

// ---------------------------------------------------------------------

void web_server_start(void)
{
    httpd_config_t cfg   = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size       = 6144;
    cfg.core_id          = 0;         // core 1 dành cho task vẽ panel
    cfg.lru_purge_enable = true;      // hết chỗ -> đóng kết nối cũ nhất thay vì từ chối máy mới
    cfg.max_uri_handlers = 24;
    // Điện thoại mở nhiều kết nối song song (+ kết nối kiểm tra Internet).
    // Cần CONFIG_LWIP_MAX_SOCKETS >= max_open_sockets + 3 (+1 cho DNS) — xem sdkconfig.defaults
    cfg.max_open_sockets = 10;
    // Phát hiện điện thoại đã rời mạng (kết nối "chết") để giải phóng chỗ sớm
    cfg.keep_alive_enable   = true;
    cfg.keep_alive_idle     = 5;
    cfg.keep_alive_interval = 5;
    cfg.keep_alive_count    = 3;

    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Khong khoi dong duoc web server");
        return;
    }

    for (const auto &f : FILES) {
        const httpd_uri_t u = { .uri = f.uri, .method = HTTP_GET, .handler = serve_static, .user_ctx = (void *)&f };
        httpd_register_uri_handler(server, &u);
    }

    const httpd_uri_t api[] = {
        { .uri = "/api/state", .method = HTTP_GET,  .handler = api_get_state,  .user_ctx = nullptr },
        { .uri = "/api/set",   .method = HTTP_POST, .handler = api_post_set,   .user_ctx = nullptr },
        { .uri = "/api/reset", .method = HTTP_POST, .handler = api_post_reset, .user_ctx = nullptr },
        { .uri = "/api/link",        .method = HTTP_GET,  .handler = api_get_link,         .user_ctx = nullptr },
        { .uri = "/api/link/scan",   .method = HTTP_POST, .handler = api_post_link_scan,   .user_ctx = nullptr },
        { .uri = "/api/link/pair",   .method = HTTP_POST, .handler = api_post_link_pair,   .user_ctx = nullptr },
        { .uri = "/api/link/unpair", .method = HTTP_POST, .handler = api_post_link_unpair, .user_ctx = nullptr },
        { .uri = "/api/link/side",   .method = HTTP_POST, .handler = api_post_link_side,   .user_ctx = nullptr },
        { .uri = "/api/profiles",       .method = HTTP_GET,  .handler = api_get_profiles,        .user_ctx = nullptr },
        { .uri = "/api/profile/save",   .method = HTTP_POST, .handler = api_post_profile_save,   .user_ctx = nullptr },
        { .uri = "/api/profile/load",   .method = HTTP_POST, .handler = api_post_profile_load,   .user_ctx = nullptr },
        { .uri = "/api/profile/delete", .method = HTTP_POST, .handler = api_post_profile_delete, .user_ctx = nullptr },
        { .uri = "/api/show",           .method = HTTP_POST, .handler = api_post_show,           .user_ctx = nullptr },
    };
    for (const auto &u : api) httpd_register_uri_handler(server, &u);

#if WIFI_CAPTIVE_PORTAL
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, redirect_home);
#endif

    ESP_LOGI(TAG, "Web server san sang");
}
