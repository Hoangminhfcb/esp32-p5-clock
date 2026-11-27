#include "wifi_manager.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "app_config.h"
#include "settings.h"

static const char *TAG = "wifi";

static bool sta_enabled(void) { return strlen(WIFI_STA_SSID) > 0; }

static void on_event(void *, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Mat ket noi WiFi nha, thu lai...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto *e = (ip_event_got_ip_t *)data;
        char ip[20];
        snprintf(ip, sizeof(ip), IPSTR, IP2STR(&e->ip_info.ip));
        ESP_LOGI(TAG, "Da vao WiFi nha, IP: %s", ip);
        settings_notify(ip, 6000);                  // hien IP len panel 6 giay
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "Co thiet bi ket noi vao AP");
    }
}

void wifi_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    if (sta_enabled()) esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, nullptr));

    // --- AP ---
    wifi_config_t ap = {};
    strncpy((char *)ap.ap.ssid, WIFI_AP_SSID, sizeof(ap.ap.ssid));
    strncpy((char *)ap.ap.password, WIFI_AP_PASS, sizeof(ap.ap.password));
    ap.ap.ssid_len       = strlen(WIFI_AP_SSID);
    ap.ap.channel        = WIFI_AP_CHANNEL;
    ap.ap.max_connection = 4;
    ap.ap.authmode       = strlen(WIFI_AP_PASS) >= 8 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(sta_enabled() ? WIFI_MODE_APSTA : WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));

    // --- STA (WiFi nha) ---
    if (sta_enabled()) {
        wifi_config_t sta = {};
        strncpy((char *)sta.sta.ssid, WIFI_STA_SSID, sizeof(sta.sta.ssid));
        strncpy((char *)sta.sta.password, WIFI_STA_PASS, sizeof(sta.sta.password));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "AP \"%s\" da bat -> http://192.168.4.1", WIFI_AP_SSID);
}
