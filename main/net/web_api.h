// =====================================================================
//  net/web_api.h — Các handler của /api/*  (đăng ký trong web_server.cpp)
// =====================================================================
#pragma once
#include "esp_http_server.h"

esp_err_t api_get_state(httpd_req_t *req);
esp_err_t api_post_set(httpd_req_t *req);
esp_err_t api_post_reset(httpd_req_t *req);

// Ghép 2 màn hình
esp_err_t api_get_link(httpd_req_t *req);
esp_err_t api_post_link_scan(httpd_req_t *req);
esp_err_t api_post_link_pair(httpd_req_t *req);
esp_err_t api_post_link_unpair(httpd_req_t *req);
esp_err_t api_post_link_side(httpd_req_t *req);

// Profile + trình chiếu
esp_err_t api_get_profiles(httpd_req_t *req);
esp_err_t api_post_profile_save(httpd_req_t *req);
esp_err_t api_post_profile_load(httpd_req_t *req);
esp_err_t api_post_profile_delete(httpd_req_t *req);
esp_err_t api_post_show(httpd_req_t *req);
