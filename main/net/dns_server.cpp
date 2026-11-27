#include "net/dns_server.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"

static const char *TAG = "dns";

#define DNS_PORT      53
#define TASK_STACK    3072
#define TASK_PRIORITY 4
#define TASK_CORE     0
#define BUF_LEN       512        // gói DNS UDP tối đa 512 byte
#define ANSWER_TTL    60         // giây

struct __attribute__((packed)) DnsHdr {
    uint16_t id, flags, qd, an, ns, ar;
};

// Tìm cuối phần tên miền (chuỗi nhãn kết thúc bằng 0). -1 nếu gói lỗi.
static int skip_name(const uint8_t *p, int pos, int len)
{
    while (pos < len) {
        const uint8_t n = p[pos];
        if (n == 0) return pos + 1;
        if ((n & 0xC0) == 0xC0) return pos + 2 <= len ? pos + 2 : -1;   // con trỏ nén
        pos += n + 1;
    }
    return -1;
}

// Dựng câu trả lời ngay trong buf. Trả về độ dài gói trả lời, 0 nếu bỏ qua.
static int make_reply(uint8_t *buf, int len, uint32_t ip_be)
{
    if (len < (int)sizeof(DnsHdr)) return 0;
    DnsHdr *h = (DnsHdr *)buf;
    const uint16_t flags = ntohs(h->flags);
    if ((flags & 0x8000) || ((flags >> 11) & 0xF) != 0) return 0;      // không phải câu hỏi chuẩn
    if (ntohs(h->qd) < 1) return 0;

    // Chỉ trả lời câu hỏi đầu tiên
    const int qname = sizeof(DnsHdr);
    const int qend  = skip_name(buf, qname, len);
    if (qend < 0 || qend + 4 > len) return 0;
    const uint16_t qtype  = (buf[qend] << 8) | buf[qend + 1];
    const uint16_t qclass = (buf[qend + 2] << 8) | buf[qend + 3];
    int out = qend + 4;                                                 // bỏ phần dư (EDNS…)

    const bool answer = qtype == 1 /*A*/ && (qclass & 0x7FFF) == 1 /*IN*/;
    h->flags = htons(0x8000 | 0x0400 | (flags & 0x0100) | 0x0080);      // QR AA RD RA, NOERROR
    h->qd = htons(1);
    h->an = htons(answer ? 1 : 0);
    h->ns = 0;
    h->ar = 0;
    if (answer) {
        if (out + 16 > BUF_LEN) return 0;
        const uint8_t rr[16] = {
            0xC0, 0x0C,                         // tên = tên trong câu hỏi
            0x00, 0x01, 0x00, 0x01,             // A, IN
            0, 0, 0, ANSWER_TTL,                // TTL
            0x00, 0x04,                         // 4 byte IPv4
        };
        memcpy(buf + out, rr, 12);
        memcpy(buf + out + 12, &ip_be, 4);
        out += 16;
    }
    return out;
}

static uint32_t ap_ip(void)
{
    esp_netif_ip_info_t info = {};
    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap && esp_netif_get_ip_info(ap, &info) == ESP_OK && info.ip.addr) return info.ip.addr;
    return inet_addr("192.168.4.1");
}

static void task(void *)
{
    const int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) { ESP_LOGE(TAG, "Khong tao duoc socket"); vTaskDelete(nullptr); return; }
    sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(DNS_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Khong mo duoc cong 53");
        close(sock);
        vTaskDelete(nullptr);
        return;
    }
    const uint32_t ip = ap_ip();
    ESP_LOGI(TAG, "DNS captive portal san sang");

    static uint8_t buf[BUF_LEN];
    while (true) {
        sockaddr_in from = {};
        socklen_t   flen = sizeof(from);
        const int n = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr *)&from, &flen);
        if (n <= 0) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
        const int r = make_reply(buf, n, ip);
        if (r > 0) sendto(sock, buf, r, 0, (sockaddr *)&from, flen);
    }
}

void dns_server_start(void)
{
    xTaskCreatePinnedToCore(task, "dns", TASK_STACK, nullptr, TASK_PRIORITY, nullptr, TASK_CORE);
}
