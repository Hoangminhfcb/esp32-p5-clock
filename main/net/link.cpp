#include "net/link.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_log.h"
#include "config/app_config.h"
#include "app/link_state.h"
#include "app/settings.h"
#include "app/notice.h"
#include "hal/button.h"

static const char *TAG = "link";

#define TASK_STACK      6144
#define TASK_PRIORITY   4
#define TASK_CORE       0
#define LOOP_MS         50
#define SCAN_MS         3000      // thời gian tìm mạch
#define PAIR_MS         6000      // chờ mạch kia đồng ý (gồm cả lúc nó khởi động lại)
#define ALIVE_MS        1000      // slave báo "còn sống"
#define HOP_MS          250       // slave dò kênh khi mất master
#define RESEND_MIN_MS   300       // gửi lại DATA tối đa bao lâu 1 lần
#define CHUNK           200       // byte dữ liệu / gói (ESP-NOW tối đa 250)

// ---------------------------------------------------------------------
//  Gói tin (cả 2 mạch cùng firmware -> cùng cấu trúc)
// ---------------------------------------------------------------------
static const uint32_t PKT_MAGIC = 0x4B4E4C48;    // "HLNK"
static const uint8_t  PKT_VER   = 1;

enum PktType : uint8_t {
    P_DISCOVER = 1,   // broadcast: "ai đang riêng thì trả lời"
    P_HELLO,          // trả lời DISCOVER
    P_PAIR,           // master mời ghép
    P_PAIR_OK,        // slave đồng ý
    P_UNPAIR,         // hủy ghép
    P_DATA,           // master -> slave: 1 mảnh nội dung
    P_SYNC,           // master -> slave: đồng hồ
    P_ALIVE,          // slave -> master: còn sống + bản nội dung đang có
};

struct __attribute__((packed)) PktHdr  { uint32_t magic; uint8_t ver; uint8_t type; };
struct __attribute__((packed)) PktPair { PktHdr h; uint8_t master_side; };
struct __attribute__((packed)) PktSync { PktHdr h; int64_t master_ms; uint16_t seq; };
struct __attribute__((packed)) PktAlive{ PktHdr h; uint16_t seq_have; };
struct __attribute__((packed)) PktData {
    PktHdr   h;
    int64_t  master_ms;
    int64_t  epoch_ms;
    uint16_t seq;          // bản nội dung
    uint16_t total;        // tổng số byte (= sizeof(DisplaySettings))
    uint8_t  master_side;
    uint8_t  idx, count;   // mảnh thứ idx / count
    uint8_t  payload[CHUNK];
};

// ---------------------------------------------------------------------
//  Hàng đợi sự kiện: gói nhận được (từ callback WiFi) + lệnh từ web
// ---------------------------------------------------------------------
enum EvType : uint8_t { EV_RX, EV_SCAN, EV_PAIR, EV_UNPAIR, EV_SIDE };

struct Event {
    EvType  type;
    uint8_t mac[6];
    uint8_t channel;
    int8_t  rssi;
    int64_t rx_us;
    int     len;
    uint8_t data[ESP_NOW_MAX_DATA_LEN];
};

static QueueHandle_t     s_q;
static SemaphoreHandle_t s_lock;
static LinkStatus        s_st;                         // trạng thái cho web (có khóa)
static const uint8_t     BCAST[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// Biến chỉ task link dùng (không cần khóa)
static int64_t  s_scan_until = 0, s_next_discover = 0;
static int64_t  s_pair_until = 0, s_next_pair = 0;
static uint8_t  s_pair_mac[6];
static int64_t  s_last_peer_us = 0;                    // lần cuối nghe mạch kia
static int64_t  s_next_sync = 0, s_next_alive = 0, s_next_hop = 0, s_last_data_sent = 0;
static bool     s_button_done = false;

// Master: bản nội dung đang phát
static uint16_t        s_seq = 0;
static uint32_t        s_seen_settings = 0, s_seen_link = 0;
static DisplaySettings s_tx;
static int64_t         s_tx_epoch = 0;

// Slave: ghép các mảnh nhận được
static uint8_t  s_rx_buf[sizeof(DisplaySettings)];
static uint16_t s_rx_seq = 0, s_rx_have = 0;           // s_rx_have: bản đã nhận đủ (0 = chưa có)
static uint32_t s_rx_mask = 0;

static_assert(sizeof(DisplaySettings) <= CHUNK * 32, "DisplaySettings qua lon cho 32 manh");

static inline int64_t now_us(void) { return esp_timer_get_time(); }

static void mac_id(const uint8_t *mac, char *out) { snprintf(out, 8, "%02X%02X", mac[4], mac[5]); }

// ---------------------------------------------------------------------
//  Gửi
// ---------------------------------------------------------------------
static void ensure_peer(const uint8_t *mac)
{
    if (esp_now_is_peer_exist(mac)) return;
    esp_now_peer_info_t p = {};
    memcpy(p.peer_addr, mac, 6);
    p.channel = 0;                 // 0 = kênh hiện tại
    p.ifidx   = WIFI_IF_STA;
    p.encrypt = false;
    if (esp_now_add_peer(&p) != ESP_OK) {
        // đầy danh sách -> xóa bớt rồi thử lại
        esp_now_peer_info_t old;
        if (esp_now_fetch_peer(true, &old) == ESP_OK) esp_now_del_peer(old.peer_addr);
        esp_now_add_peer(&p);
    }
}

static void send_raw(const uint8_t *mac, const void *pkt, size_t len)
{
    ensure_peer(mac);
    esp_now_send(mac, (const uint8_t *)pkt, len);
}

static void hdr(PktHdr *h, PktType t) { h->magic = PKT_MAGIC; h->ver = PKT_VER; h->type = t; }

static void send_simple(const uint8_t *mac, PktType t)
{
    PktHdr h;
    hdr(&h, t);
    send_raw(mac, &h, sizeof(h));
}

static void send_data(const uint8_t *mac)
{
    LinkConfig lc;
    link_state_get(&lc);
    const uint16_t total = sizeof(DisplaySettings);
    const uint8_t  count = (total + CHUNK - 1) / CHUNK;
    PktData d;
    hdr(&d.h, P_DATA);
    d.epoch_ms    = s_tx_epoch;
    d.seq         = s_seq;
    d.total       = total;
    d.master_side = lc.master_side;
    d.count       = count;
    for (uint8_t i = 0; i < count; i++) {
        const int off = i * CHUNK, n = total - off < CHUNK ? total - off : CHUNK;
        d.idx       = i;
        d.master_ms = link_now_ms();
        memcpy(d.payload, (const uint8_t *)&s_tx + off, n);
        send_raw(mac, &d, offsetof(PktData, payload) + n);
    }
    s_last_data_sent = now_us();
}

// ---------------------------------------------------------------------
//  Đổi vai trò
// ---------------------------------------------------------------------
static void set_status_peer(const LinkConfig &c)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_st.role        = c.role;
    s_st.master_side = c.master_side;
    if (c.role == LINK_STANDALONE) s_st.peer_id[0] = '\0';
    else mac_id(c.peer, s_st.peer_id);
    xSemaphoreGive(s_lock);
}

static void restart_soon(void)
{
    vTaskDelay(pdMS_TO_TICKS(400));        // cho gói cuối kịp đi
    esp_restart();
}

// Về master riêng. tell_peer: báo cho mạch kia hủy theo.
static void go_standalone(bool tell_peer)
{
    LinkConfig c;
    link_state_get(&c);
    if (c.role == LINK_STANDALONE) return;
    const bool was_slave = c.role == LINK_SLAVE;
    if (tell_peer)
        for (int i = 0; i < 3; i++) { send_simple(c.peer, P_UNPAIR); vTaskDelay(pdMS_TO_TICKS(30)); }

    c.role = LINK_STANDALONE;
    memset(c.peer, 0, sizeof(c.peer));
    link_state_set(&c);
    set_status_peer(c);
    ESP_LOGI(TAG, "Huy ghep -> ve master rieng");
    if (was_slave) restart_soon();         // slave phải bật lại WiFi (AP) -> khởi động lại
    link_clock_reset();
    settings_touch();
    notice_show("Đã hủy ghép", 2500);
}

// ---------------------------------------------------------------------
//  Xử lý gói nhận được
// ---------------------------------------------------------------------
static void on_rx(const Event &e)
{
    if (e.len < (int)sizeof(PktHdr)) return;
    const PktHdr *h = (const PktHdr *)e.data;
    if (h->magic != PKT_MAGIC || h->ver != PKT_VER) return;

    LinkConfig c;
    link_state_get(&c);
    const bool from_peer = c.role != LINK_STANDALONE && memcmp(e.mac, c.peer, 6) == 0;
    if (from_peer) s_last_peer_us = now_us();

    switch (h->type) {
    case P_DISCOVER:
        if (c.role != LINK_STANDALONE) break;              // đã ghép rồi thì im
        send_simple(e.mac, P_HELLO);
        {   // hiện mã của mình lên panel để người dùng nhận ra mạch nào
            char m[24];
            snprintf(m, sizeof(m), "Mã: %s", link_device_id());
            notice_show(m, 5000);
        }
        break;

    case P_HELLO:
        if (now_us() > s_scan_until) break;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        {
            int i = 0;
            for (; i < s_st.found_count; i++) if (memcmp(s_st.found[i].mac, e.mac, 6) == 0) break;
            if (i == s_st.found_count && s_st.found_count < LINK_MAX_FOUND) s_st.found_count++;
            if (i < LINK_MAX_FOUND) {
                memcpy(s_st.found[i].mac, e.mac, 6);
                mac_id(e.mac, s_st.found[i].id);
                s_st.found[i].rssi = e.rssi;
            }
        }
        xSemaphoreGive(s_lock);
        break;

    case P_PAIR:
        if (c.role == LINK_SLAVE && from_peer) { send_simple(e.mac, P_PAIR_OK); break; }   // gói lặp lại
        if (c.role != LINK_STANDALONE || e.len < (int)sizeof(PktPair)) break;
        for (int i = 0; i < 3; i++) { send_simple(e.mac, P_PAIR_OK); vTaskDelay(pdMS_TO_TICKS(30)); }
        c.role        = LINK_SLAVE;
        c.master_side = ((const PktPair *)e.data)->master_side ? 1 : 0;
        c.channel     = e.channel;
        memcpy(c.peer, e.mac, 6);
        link_state_set(&c);
        {
            char id[8], m[32];
            mac_id(e.mac, id);
            snprintf(m, sizeof(m), "Ghép với %s", id);
            notice_show(m, 3000);
            ESP_LOGI(TAG, "Lam SLAVE cua %s -> khoi dong lai", id);
        }
        vTaskDelay(pdMS_TO_TICKS(1500));                   // cho người dùng kịp đọc
        restart_soon();
        break;

    case P_PAIR_OK:
        if (c.role != LINK_STANDALONE || s_pair_until == 0 || memcmp(e.mac, s_pair_mac, 6) != 0) break;
        s_pair_until = 0;
        c.role = LINK_MASTER;
        memcpy(c.peer, e.mac, 6);
        link_state_set(&c);
        set_status_peer(c);
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_st.pairing = false;
        xSemaphoreGive(s_lock);
        s_last_peer_us = now_us();
        settings_touch();                                  // chạy chữ lại từ đầu trên màn hình ảo 128x32
        {
            char id[8], m[32];
            mac_id(e.mac, id);
            snprintf(m, sizeof(m), "Đã ghép %s", id);
            notice_show(m, 2500);
        }
        ESP_LOGI(TAG, "Da ghep, lam MASTER");
        break;

    case P_UNPAIR:
        if (from_peer) go_standalone(false);
        break;

    case P_SYNC:
    case P_DATA:
        if (c.role == LINK_STANDALONE) { send_simple(e.mac, P_UNPAIR); break; }   // nó tưởng còn ghép
        if (c.role != LINK_SLAVE || !from_peer) break;
        if (c.channel != e.channel) { c.channel = e.channel; link_state_set(&c); }  // nhớ kênh master
        if (h->type == P_SYNC) {
            if (e.len < (int)sizeof(PktSync)) break;
            link_clock_sample(((const PktSync *)e.data)->master_ms, e.rx_us);
            break;
        } else {
            const PktData *d = (const PktData *)e.data;
            const int hdr_len = offsetof(PktData, payload);
            if (e.len < hdr_len || d->total != sizeof(DisplaySettings) || d->idx >= d->count) break;
            link_clock_sample(d->master_ms, e.rx_us);
            if (d->seq == s_rx_have) break;                // đã có bản này
            if (d->seq != s_rx_seq) { s_rx_seq = d->seq; s_rx_mask = 0; }
            const int off = d->idx * CHUNK, n = e.len - hdr_len;
            if (off + n > (int)sizeof(s_rx_buf)) break;
            memcpy(s_rx_buf + off, d->payload, n);
            s_rx_mask |= 1u << d->idx;
            if (s_rx_mask == (1u << d->count) - 1) {       // đủ mảnh
                s_rx_have = d->seq;
                if (c.master_side != d->master_side) { c.master_side = d->master_side ? 1 : 0; link_state_set(&c); }
                DisplaySettings s;
                memcpy(&s, s_rx_buf, sizeof(s));
                link_remote_set(&s, d->epoch_ms);
                ESP_LOGI(TAG, "Nhan noi dung moi tu master (ban %u)", d->seq);
            }
        }
        break;

    case P_ALIVE:
        if (c.role == LINK_MASTER && from_peer) {
            if (e.len >= (int)sizeof(PktAlive) && ((const PktAlive *)e.data)->seq_have != s_seq &&
                now_us() - s_last_data_sent > RESEND_MIN_MS * 1000)
                send_data(c.peer);                         // slave thiếu bản mới -> gửi lại
        } else if (!from_peer) {
            send_simple(e.mac, P_UNPAIR);                  // slave lạc: ta không ghép với nó nữa
        }
        break;
    }
}

// ---------------------------------------------------------------------
//  Lệnh từ web
// ---------------------------------------------------------------------
static void on_cmd(const Event &e)
{
    LinkConfig c;
    link_state_get(&c);
    switch (e.type) {
    case EV_SCAN:
        if (c.role != LINK_STANDALONE) break;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_st.found_count = 0;
        s_st.scanning    = true;
        xSemaphoreGive(s_lock);
        s_scan_until    = now_us() + SCAN_MS * 1000LL;
        s_next_discover = 0;
        break;
    case EV_PAIR:
        if (c.role != LINK_STANDALONE) break;
        memcpy(s_pair_mac, e.mac, 6);
        s_pair_until = now_us() + PAIR_MS * 1000LL;
        s_next_pair  = 0;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_st.pairing = true;
        s_st.pair_failed = false;
        xSemaphoreGive(s_lock);
        break;
    case EV_UNPAIR:
        go_standalone(true);
        break;
    case EV_SIDE:                                          // riêng: nhớ sẵn cho lần ghép sau
        if (c.role == LINK_SLAVE) break;
        c.master_side = e.data[0] ? 1 : 0;
        link_state_set(&c);
        set_status_peer(c);
        settings_touch();
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------
//  Việc định kỳ
// ---------------------------------------------------------------------
static void periodic(void)
{
    const int64_t t = now_us();
    LinkConfig c;
    link_state_get(&c);

    // Nút BOOT giữ lâu -> hủy ghép (đang riêng thì hiện lại tên WiFi)
    const uint32_t held = button_held_ms();
    if (held == 0) s_button_done = false;
    else if (held >= LINK_BUTTON_HOLD_MS && !s_button_done) {
        s_button_done = true;
        if (c.role != LINK_STANDALONE) go_standalone(true);
        else notice_show(link_ap_ssid(), SHOW_SSID_MS ? SHOW_SSID_MS : 4000);
        link_state_get(&c);
    }

    // Tìm mạch
    if (s_scan_until) {
        if (t > s_scan_until) {
            s_scan_until = 0;
            xSemaphoreTake(s_lock, portMAX_DELAY);
            s_st.scanning = false;
            xSemaphoreGive(s_lock);
        } else if (t >= s_next_discover) {
            send_simple(BCAST, P_DISCOVER);
            s_next_discover = t + 1000 * 1000LL;
        }
    }

    // Đang mời ghép
    if (s_pair_until) {
        if (t > s_pair_until) {
            s_pair_until = 0;
            xSemaphoreTake(s_lock, portMAX_DELAY);
            s_st.pairing = false;
            s_st.pair_failed = true;
            xSemaphoreGive(s_lock);
            ESP_LOGW(TAG, "Ghep that bai (khong co tra loi)");
        } else if (t >= s_next_pair) {
            PktPair p;
            hdr(&p.h, P_PAIR);
            p.master_side = c.master_side;
            send_raw(s_pair_mac, &p, sizeof(p));
            s_next_pair = t + 300 * 1000LL;
        }
    }

    const bool online = s_last_peer_us && t - s_last_peer_us < LINK_LOST_MS * 1000LL;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_st.peer_online = c.role != LINK_STANDALONE && online;
    xSemaphoreGive(s_lock);

    if (c.role == LINK_MASTER) {
        // Nội dung đổi -> bản mới, gửi ngay
        const uint32_t vs = settings_version(), vl = link_state_version();
        if (vs != s_seen_settings || vl != s_seen_link) {
            s_seen_settings = vs;
            s_seen_link     = vl;
            settings_get(&s_tx);
            s_tx_epoch = settings_epoch();
            if (++s_seq == 0) s_seq = 1;                    // 0 = "chưa có"
            send_data(c.peer);
        }
        if (t >= s_next_sync) {
            PktSync s;
            hdr(&s.h, P_SYNC);
            s.master_ms = link_now_ms();
            s.seq       = s_seq;
            send_raw(c.peer, &s, sizeof(s));
            s_next_sync = t + LINK_SYNC_MS * 1000LL;
        }
    } else if (c.role == LINK_SLAVE) {
        // Mất master -> dò kênh
        if (!online && t >= s_next_hop) {
            uint8_t ch = 1;
            wifi_second_chan_t sc;
            esp_wifi_get_channel(&ch, &sc);
            ch = ch >= 13 ? 1 : ch + 1;
            esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
            s_next_hop   = t + HOP_MS * 1000LL;
            s_next_alive = 0;                               // gọi master ngay trên kênh mới
        }
        if (t >= s_next_alive) {
            PktAlive a;
            hdr(&a.h, P_ALIVE);
            a.seq_have = s_rx_have;
            send_raw(c.peer, &a, sizeof(a));
            s_next_alive = t + (online ? ALIVE_MS : HOP_MS) * 1000LL;
        }
    }
}

// ---------------------------------------------------------------------
//  Task + callback
// ---------------------------------------------------------------------
static void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len <= 0 || len > ESP_NOW_MAX_DATA_LEN) return;
    static Event e;                         // callback chạy trong task WiFi -> 1 bản tĩnh là đủ
    e.type    = EV_RX;
    memcpy(e.mac, info->src_addr, 6);
    e.channel = info->rx_ctrl ? info->rx_ctrl->channel : 0;
    e.rssi    = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
    e.rx_us   = now_us();
    e.len     = len;
    memcpy(e.data, data, len);
    xQueueSend(s_q, &e, 0);                 // đầy thì bỏ gói
}

static void task(void *)
{
    static Event e;
    while (true) {
        if (xQueueReceive(s_q, &e, pdMS_TO_TICKS(LOOP_MS)) == pdTRUE) {
            if (e.type == EV_RX) on_rx(e);
            else on_cmd(e);
        }
        periodic();
    }
}

static void post_cmd(EvType t, const uint8_t *mac, uint8_t arg)
{
    static Event e;                         // chỉ dùng phần đầu, gọi từ task httpd
    static SemaphoreHandle_t lk = xSemaphoreCreateMutex();
    xSemaphoreTake(lk, portMAX_DELAY);
    e.type = t;
    if (mac) memcpy(e.mac, mac, 6);
    e.data[0] = arg;
    e.len = 1;
    xQueueSend(s_q, &e, pdMS_TO_TICKS(100));
    xSemaphoreGive(lk);
}

void link_start(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_q    = xQueueCreate(12, sizeof(Event));
    button_init();

    LinkConfig c;
    link_state_get(&c);
    set_status_peer(c);
    // Slave vừa khởi động: nghe thử trên kênh đã nhớ một lúc rồi mới dò kênh khác
    if (c.role == LINK_SLAVE) s_next_hop = now_us() + LINK_LOST_MS * 1000LL;

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_recv));
    ensure_peer(BCAST);
    if (c.role != LINK_STANDALONE) ensure_peer(c.peer);

    xTaskCreatePinnedToCore(task, "link", TASK_STACK, nullptr, TASK_PRIORITY, nullptr, TASK_CORE);
    ESP_LOGI(TAG, "ESP-NOW san sang, ma mach %s", link_device_id());
}

void link_status(LinkStatus *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_st;
    xSemaphoreGive(s_lock);
}

void link_scan(void)                    { post_cmd(EV_SCAN, nullptr, 0); }
void link_pair(const uint8_t mac[6])    { post_cmd(EV_PAIR, mac, 0); }
void link_unpair(void)                  { post_cmd(EV_UNPAIR, nullptr, 0); }
void link_set_side(uint8_t master_side) { post_cmd(EV_SIDE, nullptr, master_side); }
