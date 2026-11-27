#include "net/wifi.h"
#include <string.h>
#include <stdio.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "config/app_config.h"
#include "config/secrets_loader.h"
#include "app/notice.h"
#include "app/link_state.h"
#include "esp_mac.h"
#include "esp_timer.h"

static const char *TAG = "wifi";

static bool s_slave = false;

// WiFi nhà không vào được: thử lại GIÃN DẦN (5s, 10s, 20s ... tối đa 60s).
// Trước đây thử lại liên tục -> ESP32 quét hết các kênh liên tục, AP bị nhảy
// kênh / ngắt quãng -> điện thoại đang vào AP bị treo, web load rất lâu.
#define STA_RETRY_MIN_MS  5000
#define STA_RETRY_MAX_MS  60000
static esp_timer_handle_t s_retry_timer = nullptr;
static uint32_t           s_retry_ms    = STA_RETRY_MIN_MS;

static void retry_connect(void *) { esp_wifi_connect(); }

// WiFi nhà: chỉ khi có cấu hình và mạch không phải slave
static bool sta_enabled(void) { return !s_slave && strlen(WIFI_STA_SSID) > 0; }

static void on_event(void *, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (sta_enabled()) esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED && sta_enabled()) {
        ESP_LOGW(TAG, "Mat ket noi WiFi nha, thu lai sau %lu ms", (unsigned long)s_retry_ms);
        if (s_retry_timer) {
            esp_timer_stop(s_retry_timer);
            esp_timer_start_once(s_retry_timer, (uint64_t)s_retry_ms * 1000);
        }
        s_retry_ms = s_retry_ms * 2 > STA_RETRY_MAX_MS ? STA_RETRY_MAX_MS : s_retry_ms * 2;
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_retry_ms = STA_RETRY_MIN_MS;
        const auto *e = (ip_event_got_ip_t *)data;
        char ip[20];
        snprintf(ip, sizeof(ip), IPSTR, IP2STR(&e->ip_info.ip));
        ESP_LOGI(TAG, "Da vao WiFi nha, IP: %s", ip);
        notice_show(ip, NOTICE_IP_MS);                  // hiện IP lên panel
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "Co thiet bi ket noi vao AP");
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STADISCONNECTED) {
        const auto *e = (wifi_event_ap_stadisconnected_t *)data;
        ESP_LOGI(TAG, "Thiet bi roi AP (ly do %d)", e->reason);
    }
}

static void config_ap(void)
{
    const char *ssid = link_ap_ssid();               // "HUB75-LED-A1B2": mỗi mạch 1 tên
    wifi_config_t ap = {};
    strncpy((char *)ap.ap.ssid, ssid, sizeof(ap.ap.ssid));
    strncpy((char *)ap.ap.password, WIFI_AP_PASS, sizeof(ap.ap.password));
    ap.ap.ssid_len       = strlen(ssid);
    ap.ap.channel        = WIFI_AP_CHANNEL;
    ap.ap.max_connection = WIFI_AP_MAX_CLIENTS;
    ap.ap.authmode       = strlen(WIFI_AP_PASS) >= 8 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
}

static void config_sta(void)
{
    wifi_config_t sta = {};
    strncpy((char *)sta.sta.ssid, WIFI_STA_SSID, sizeof(sta.sta.ssid));
    strncpy((char *)sta.sta.password, WIFI_STA_PASS, sizeof(sta.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
}

// Mã mạch = 2 byte cuối MAC STA (MAC này cố định ở mọi vai trò, ESP-NOW cũng dùng nó)
static void make_device_id(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char id[8];
    snprintf(id, sizeof(id), "%02X%02X", mac[4], mac[5]);
    link_set_device_id(id);
}

void wifi_start(void)
{
    LinkConfig lc;
    link_state_get(&lc);
    s_slave = lc.role == LINK_SLAVE;
    make_device_id();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if (!s_slave) esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();            // luôn có STA: ESP-NOW chạy trên giao diện STA

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, nullptr));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    if (s_slave) {
        // Slave: không phát WiFi, chỉ nghe master trên kênh lần cuối thấy nó
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        esp_wifi_set_ps(WIFI_PS_NONE);              // không ngủ -> không lỡ gói từ master
        const uint8_t ch = (lc.channel >= 1 && lc.channel <= 13) ? lc.channel : WIFI_AP_CHANNEL;
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        ESP_LOGI(TAG, "SLAVE: tat AP, nghe master tren kenh %d", ch);
        return;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    config_ap();
    if (sta_enabled()) {
        config_sta();
        const esp_timer_create_args_t ta = { .callback = retry_connect, .arg = nullptr,
                                             .dispatch_method = ESP_TIMER_TASK, .name = "sta_retry",
                                             .skip_unhandled_events = true };
        esp_timer_create(&ta, &s_retry_timer);
    }
    // Kênh 20 MHz: ổn định hơn với điện thoại cũ / chỗ nhiều WiFi (tốc độ vẫn thừa cho web)
    esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);

    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_LOGI(TAG, "AP \"%s\" da bat -> http://192.168.4.1", link_ap_ssid());
}
