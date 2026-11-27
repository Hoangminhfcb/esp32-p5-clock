#include "app/clock.h"
#include "app/lunar.h"
#include "app/clock_palette.h"
#include "hal/panel.h"
#include "hal/button.h"
#include "gfx/clock_face.h"
#include "config/app_config.h"
#include <atomic>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <cctype>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_netif_sntp.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "cJSON.h"

struct Credentials { char ssid[33]; char password[65]; };
static QueueHandle_t s_credentials;
static std::atomic<bool> s_connected{false}, s_setup{false}, s_open_setup{false}, s_has_credentials{false}, s_time_ready{false};
static char s_ap[32];
static constexpr int64_t SECOND = 1000000LL;
extern const char clock_html_start[] asm("_binary_clock_html_start");
static std::atomic<bool> s_scan_requested{false}, s_scan_busy{false}, s_connecting{false};
static SemaphoreHandle_t s_scan_mutex;
static char s_scan_result[8192]="{\"scanning\":false,\"networks\":[]}";

// HTTP chỉ yêu cầu quét; tác vụ mạng giữ quyền điều khiển Wi-Fi.
static esp_err_t scan_request(httpd_req_t *r) {
    httpd_resp_set_type(r,"application/json; charset=utf-8");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    if(!s_setup||s_connecting) {
        httpd_resp_set_status(r,"409 Conflict");
        return httpd_resp_sendstr(r,"{\"error\":\"Đồng hồ đang kết nối Wi-Fi. Chờ một chút rồi quét lại.\"}");
    }
    char query[32]={};
    httpd_req_get_url_query_str(r,query,sizeof(query));
    if(strcmp(query,"refresh=1")==0) {
        bool expected=false;
        if(s_scan_busy.compare_exchange_strong(expected,true)) s_scan_requested=true;
    }
    if(s_scan_busy) return httpd_resp_sendstr(r,"{\"scanning\":true,\"networks\":[]}");
    xSemaphoreTake(s_scan_mutex,portMAX_DELAY);
    char *copy=strdup(s_scan_result);
    xSemaphoreGive(s_scan_mutex);
    if(!copy) return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Khong du bo nho");
    esp_err_t result=httpd_resp_sendstr(r,copy);
    free(copy);
    return result;
}
static void scan_wifi() {
    wifi_scan_config_t config={};
    config.show_hidden=false;
    config.scan_time.active.min=40;
    config.scan_time.active.max=100;
    const esp_err_t error=esp_wifi_scan_start(&config,true);
    cJSON *root=cJSON_CreateObject();
    cJSON_AddBoolToObject(root,"scanning",false);
    cJSON *networks=cJSON_AddArrayToObject(root,"networks");
    if(error==ESP_OK) {
        wifi_ap_record_t records[24]={};
        uint16_t count=24;
        if(esp_wifi_scan_get_ap_records(&count,records)==ESP_OK) {
            for(int i=0;i<count;++i) {
                records[i].ssid[32]=0;
                if(!records[i].ssid[0]) continue;
                bool duplicate=false;
                for(int j=0;j<i;++j) if(strcmp((const char*)records[i].ssid,(const char*)records[j].ssid)==0) duplicate=true;
                if(duplicate) continue;
                cJSON *item=cJSON_CreateObject();
                cJSON_AddStringToObject(item,"ssid",(const char*)records[i].ssid);
                cJSON_AddNumberToObject(item,"rssi",records[i].rssi);
                cJSON_AddBoolToObject(item,"secured",records[i].authmode!=WIFI_AUTH_OPEN);
                cJSON_AddItemToArray(networks,item);
            }
            ESP_LOGI("clock","Quet WiFi: %d mang",cJSON_GetArraySize(networks));
        } else {
            esp_wifi_clear_ap_list();
            cJSON_AddStringToObject(root,"error","Không đọc được danh sách Wi-Fi. Hãy quét lại.");
        }
    } else {
        esp_wifi_clear_ap_list();
        cJSON_AddStringToObject(root,"error","Chưa quét được Wi-Fi. Chờ một chút rồi thử lại.");
        ESP_LOGW("clock","Quet WiFi: %s",esp_err_to_name(error));
    }
    char *json=cJSON_PrintUnformatted(root);
    if(json) {
        xSemaphoreTake(s_scan_mutex,portMAX_DELAY);
        snprintf(s_scan_result,sizeof(s_scan_result),"%s",json);
        xSemaphoreGive(s_scan_mutex);
        cJSON_free(json);
    }
    cJSON_Delete(root);
    s_scan_busy=false;
}
static esp_err_t page(httpd_req_t *r) {
    httpd_resp_set_type(r,"text/html; charset=utf-8");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    return httpd_resp_send(r,clock_html_start,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t save(httpd_req_t *r) {
    if(r->content_len<=0 || r->content_len>512) return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Yeu cau qua dai");
    char buf[513]; int n=0;
    while(n<r->content_len) { int got=httpd_req_recv(r,buf+n,r->content_len-n); if(got<=0) return ESP_FAIL; n+=got; }
    buf[n]=0; cJSON *j=cJSON_Parse(buf);
    const cJSON *ssid=cJSON_GetObjectItemCaseSensitive(j,"ssid"), *pw=cJSON_GetObjectItemCaseSensitive(j,"password");
    if(!cJSON_IsString(ssid)||!cJSON_IsString(pw)||strlen(ssid->valuestring)<1||strlen(ssid->valuestring)>32||strlen(pw->valuestring)>63||(strlen(pw->valuestring)>0&&strlen(pw->valuestring)<8)) {
        cJSON_Delete(j); return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"SSID 1-32 byte; mat khau rong hoac 8-63 byte");
    }
    Credentials c={}; strcpy(c.ssid,ssid->valuestring); strcpy(c.password,pw->valuestring); cJSON_Delete(j);
    xQueueOverwrite(s_credentials,&c);
    httpd_resp_set_type(r,"text/plain; charset=utf-8");
    return httpd_resp_sendstr(r,"Đang thử Wi-Fi và lấy giờ. Chỉ lưu sau khi thành công. Nếu thất bại, hãy nhập lại.");
}
static esp_err_t get_palette(httpd_req_t *r) {
    ClockPalette palette;clock_palette_get(&palette);
    cJSON *root=cJSON_CreateObject();
    const char *names[]={"time","date"};
    for(int group=0;group<2;++group) {
        cJSON *array=cJSON_AddArrayToObject(root,names[group]);
        for(int i=0;i<4;++i) {
            char hex[8];snprintf(hex,sizeof(hex),"#%06lX",static_cast<unsigned long>(group?palette.date[i]:palette.time[i]));
            cJSON_AddItemToArray(array,cJSON_CreateString(hex));
        }
    }
    char *json=cJSON_PrintUnformatted(root);cJSON_Delete(root);
    if(!json) return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Khong du bo nho");
    httpd_resp_set_type(r,"application/json; charset=utf-8");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    const esp_err_t error=httpd_resp_sendstr(r,json);cJSON_free(json);return error;
}
static esp_err_t save_palette(httpd_req_t *r) {
    if(r->content_len<=0||r->content_len>512) return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Bang mau khong hop le");
    char body[513];int length=0;
    while(length<r->content_len) {
        const int count=httpd_req_recv(r,body+length,r->content_len-length);
        if(count<=0) return ESP_FAIL;
        length+=count;
    }
    body[length]=0;
    cJSON *root=cJSON_Parse(body);ClockPalette palette={};bool valid=true;
    const char *names[]={"time","date"};
    for(int group=0;group<2;++group) {
        cJSON *array=cJSON_GetObjectItemCaseSensitive(root,names[group]);
        if(!cJSON_IsArray(array)||cJSON_GetArraySize(array)!=4) {valid=false;break;}
        for(int i=0;i<4;++i) {
            const cJSON *item=cJSON_GetArrayItem(array,i);
            if(!cJSON_IsString(item)||strlen(item->valuestring)!=7||item->valuestring[0]!='#') {valid=false;break;}
            for(int j=1;j<7;++j) if(!isxdigit(static_cast<unsigned char>(item->valuestring[j]))) valid=false;
            if(!valid) break;
            const uint32_t value=strtoul(item->valuestring+1,nullptr,16);
            (group?palette.date[i]:palette.time[i])=value;
        }
    }
    cJSON_Delete(root);
    if(!valid) return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Moi bo can 4 mau #RRGGBB");
    if(!clock_palette_store(palette)) return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Khong luu duoc bang mau");
    httpd_resp_set_type(r,"text/plain; charset=utf-8");
    return httpd_resp_sendstr(r,"Đã lưu bảng màu. Giờ đổi theo giờ, lịch đổi theo ngày; màu được phối từ các màu bạn chọn.");
}
static void event(void *,esp_event_base_t base,int32_t id,void *) {
    if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) s_connected=true;
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) s_connected=false;
}
static void display(void *) {
    button_init();
    bool latched=false, last_setup=false, last_connecting=false, last_ready=false;
    time_t last_second=-1;
    int64_t last_animation=-1;
    for(;;) {
        const auto held=button_held_ms();
        if(held>=2000&&!latched) {s_open_setup=true;latched=true;}
        if(!held) latched=false;
        const time_t now=time(nullptr);
        const bool setup=s_setup.load();
        const bool connecting=s_has_credentials.load()||s_connecting.load();
        const bool ready=s_time_ready.load();
        const int64_t uptime=esp_timer_get_time()/1000;
        const int64_t phase=uptime/CLOCK_CALENDAR_MS;
        const int elapsed=uptime%CLOCK_CALENDAR_MS;
        const bool entering=elapsed<CLOCK_CALENDAR_TRANSITION_MS;
        const int64_t animation=entering?uptime/40:-1;
        if(now==last_second&&setup==last_setup&&connecting==last_connecting&&ready==last_ready&&animation==last_animation) {
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }
        last_second=now;last_setup=setup;last_connecting=connecting;last_ready=ready;last_animation=animation;
        tm t={};localtime_r(&now,&t);
        panel_clear();
        if(t.tm_year>=124&&ready) {
            // Mỗi giờ có một sắc màu riêng; dấu hai chấm đổi trạng thái mỗi giây.
            ClockPalette palette;clock_palette_get(&palette);
            const Rgb time_color=clock_palette_mix(palette.time,t.tm_hour,24);
            // Ngày dân sự tại Việt Nam; không đổi màu lúc chuyển dương/âm lịch.
            const Rgb date_color=clock_palette_mix(palette.date,(now+7*3600)/86400,28);
            clock_face_time(t.tm_hour,t.tm_min,t.tm_sec%2==0,time_color);
            clock_face_seconds(t.tm_sec);
            char line[32];
            if(phase%2==0) {
                static const char *WEEKDAYS[]={"CN","T2","T3","T4","T5","T6","T7"};
                snprintf(line,sizeof(line),"%s %02d/%02d/%02d",WEEKDAYS[t.tm_wday],t.tm_mday,t.tm_mon+1,(t.tm_year+1900)%100);
            } else {
                const auto l=lunar_date(t.tm_mday,t.tm_mon+1,t.tm_year+1900);
                snprintf(line,sizeof(line),"%02d/%02d AL%s",l.day,l.month,l.leap?"N":"");
            }
            // Trượt lịch mới lên từ dưới; chỉ cắt trong vùng lịch, giờ luôn đứng yên.
            const int remaining=entering?CLOCK_CALENDAR_TRANSITION_MS-elapsed:0;
            const int offset=(10*remaining*remaining)/(CLOCK_CALENDAR_TRANSITION_MS*CLOCK_CALENDAR_TRANSITION_MS);
            clock_face_date(line,offset,date_color);
        } else {
            clock_face_status(connecting);
        }
        panel_present();
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}
static void network(void *) {
    Credentials saved={}, candidate={}; bool has_saved=false;
    nvs_handle_t h; if(nvs_open("clock",NVS_READWRITE,&h)==ESP_OK) {size_t n=sizeof(saved); has_saved=nvs_get_blob(h,"wifi",&saved,&n)==ESP_OK&&n==sizeof(saved)&&saved.ssid[0]&&memchr(saved.ssid,0,sizeof(saved.ssid))&&memchr(saved.password,0,sizeof(saved.password)); nvs_close(h);}
    s_has_credentials=has_saved;
    ESP_ERROR_CHECK(esp_netif_init()); ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap(); esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT(); ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,event,nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,event,nullptr));
    uint8_t mac[6]; esp_read_mac(mac,ESP_MAC_WIFI_STA); snprintf(s_ap,sizeof(s_ap),"P5-CLOCK-%02X%02X",mac[4],mac[5]);
    httpd_handle_t server=nullptr; bool radio=false, trying=false, pending=false, sntp=false;
    int64_t deadline=0, next_sync=0, setup_until=0, reconnect=0;
    auto setup=[&]() {
        if(radio) { if(sntp){esp_netif_sntp_deinit();sntp=false;} esp_wifi_stop(); }
        s_connected=false; trying=false;s_connecting=false;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        wifi_config_t ap={}; strcpy((char*)ap.ap.ssid,s_ap); ap.ap.ssid_len=strlen(s_ap); ap.ap.max_connection=2; ap.ap.channel=6; ap.ap.authmode=WIFI_AUTH_OPEN;
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&ap)); ESP_ERROR_CHECK(esp_wifi_start());radio=true;s_setup=true;
        setup_until=esp_timer_get_time()+300*SECOND;
        if(!server) {
            httpd_config_t hc=HTTPD_DEFAULT_CONFIG();hc.stack_size=6144; ESP_ERROR_CHECK(httpd_start(&server,&hc));
            httpd_uri_t root={};root.uri="/";root.method=HTTP_GET;root.handler=page;ESP_ERROR_CHECK(httpd_register_uri_handler(server,&root));
            httpd_uri_t scan={};scan.uri="/scan";scan.method=HTTP_GET;scan.handler=scan_request;ESP_ERROR_CHECK(httpd_register_uri_handler(server,&scan));
            httpd_uri_t colors={};colors.uri="/palette";colors.method=HTTP_GET;colors.handler=get_palette;ESP_ERROR_CHECK(httpd_register_uri_handler(server,&colors));
            colors.method=HTTP_POST;colors.handler=save_palette;ESP_ERROR_CHECK(httpd_register_uri_handler(server,&colors));
            httpd_uri_t post={};post.uri="/wifi";post.method=HTTP_POST;post.handler=save;ESP_ERROR_CHECK(httpd_register_uri_handler(server,&post));
        }
        s_scan_busy=true;s_scan_requested=true;
        ESP_LOGI("clock","Cau hinh: %s -> http://192.168.4.1",s_ap);
    };
    if(!has_saved) setup();
    for(;;) {
        int64_t now=esp_timer_get_time();
        if(s_open_setup.exchange(false)) setup();
        bool received=xQueueReceive(s_credentials,&candidate,0)==pdTRUE;
        if(received || (!trying&&now>=next_sync&&has_saved)) {
            if(sntp){esp_netif_sntp_deinit();sntp=false;}
            if(!received) candidate=saved;
            pending=received;s_connecting=true;
            if(radio) esp_wifi_disconnect(); else {ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));}
            wifi_config_t sta={}; memcpy(sta.sta.ssid,candidate.ssid,strlen(candidate.ssid));memcpy(sta.sta.password,candidate.password,strlen(candidate.password));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&sta));
            s_connected=false;
            if(!radio){ESP_ERROR_CHECK(esp_wifi_start());radio=true;}
            esp_wifi_connect(); trying=true;deadline=now+60*SECOND;reconnect=now+10*SECOND;
        }
        if(trying&&s_connected&&!sntp) {
            esp_sntp_config_t ntp=ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
            ESP_ERROR_CHECK(esp_netif_sntp_init(&ntp));sntp=true;
        }
        bool synced=trying&&sntp&&esp_netif_sntp_sync_wait(0)==ESP_OK;
        if(synced || (trying&&now>=deadline)) {
            if(sntp){esp_netif_sntp_deinit();sntp=false;}
            trying=false;s_connecting=false;
            if(synced) {
                if(pending) {
                    esp_err_t err=nvs_open("clock",NVS_READWRITE,&h);
                    if(err==ESP_OK){err=nvs_set_blob(h,"wifi",&candidate,sizeof(candidate));if(err==ESP_OK)err=nvs_commit(h);nvs_close(h);}
                    if(err!=ESP_OK){ESP_LOGE("clock","Khong luu duoc WiFi: %s",esp_err_to_name(err));setup();vTaskDelay(pdMS_TO_TICKS(200));continue;}
                    saved=candidate;has_saved=true;s_has_credentials=true;
                }
                next_sync=now+86400*SECOND;s_time_ready=true;
                if(server){httpd_stop(server);server=nullptr;}
                s_setup=false;esp_wifi_stop();radio=false;s_connected=false;
                ESP_LOGI("clock","Da dong bo; tat WiFi 24 gio");
            } else {
                ESP_LOGW("clock","Dong bo that bai; thu lai sau 1 gio");
                next_sync=now+3600*SECOND;
                if(has_saved&&time(nullptr)>=1704067200) s_time_ready=true;
                if(time(nullptr)<1704067200 || pending) setup();
                else if(!s_setup){esp_wifi_stop();radio=false;}
            }
        }
        if(trying&&!s_connected&&now>=reconnect) {esp_wifi_connect();reconnect=now+10*SECOND;}
        if(s_setup&&!trying&&has_saved&&time(nullptr)>=1704067200&&now>=setup_until) {
            if(server){httpd_stop(server);server=nullptr;}esp_wifi_stop();radio=false;s_setup=false;
        }
        if(s_scan_requested.exchange(false)) {
            if(s_setup&&!trying) scan_wifi();
            else s_scan_busy=false;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
void clock_start() {
    setenv("TZ","ICT-7",1);tzset();
    clock_palette_init();
    s_scan_mutex=xSemaphoreCreateMutex();configASSERT(s_scan_mutex);
    s_credentials=xQueueCreate(1,sizeof(Credentials));configASSERT(s_credentials);
    panel_set_brightness(48);
    configASSERT(xTaskCreate(display,"clock_display",4096,nullptr,3,nullptr)==pdPASS);
    configASSERT(xTaskCreate(network,"clock_network",8192,nullptr,4,nullptr)==pdPASS);
}
