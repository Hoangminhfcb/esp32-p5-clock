#include "app/clock_palette.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "esp_log.h"
static SemaphoreHandle_t s_mutex;
static ClockPalette s_palette={{0x21B8FF,0xB366FF,0xFF647C,0x50E8AD},{0xFF9D52,0xE875DD,0x64D4FF,0xADE566}};
void clock_palette_init() {
    s_mutex=xSemaphoreCreateMutex();configASSERT(s_mutex);
    nvs_handle_t handle;
    if(nvs_open("clock",NVS_READONLY,&handle)==ESP_OK) {
        ClockPalette loaded={};size_t size=sizeof(loaded);
        if(nvs_get_blob(handle,"palette",&loaded,&size)==ESP_OK&&size==sizeof(loaded)) {
            bool valid=true;
            for(int i=0;i<4;++i) if(loaded.time[i]>0xFFFFFF||loaded.date[i]>0xFFFFFF) valid=false;
            if(valid) s_palette=loaded;
        }
        nvs_close(handle);
    }
}
void clock_palette_get(ClockPalette *out) {
    xSemaphoreTake(s_mutex,portMAX_DELAY);*out=s_palette;xSemaphoreGive(s_mutex);
}
bool clock_palette_store(const ClockPalette &palette) {
    nvs_handle_t handle;
    esp_err_t error=nvs_open("clock",NVS_READWRITE,&handle);
    if(error==ESP_OK) {
        error=nvs_set_blob(handle,"palette",&palette,sizeof(palette));
        if(error==ESP_OK) error=nvs_commit(handle);
        nvs_close(handle);
    }
    if(error!=ESP_OK) {ESP_LOGE("palette","Luu mau: %s",esp_err_to_name(error));return false;}
    xSemaphoreTake(s_mutex,portMAX_DELAY);s_palette=palette;xSemaphoreGive(s_mutex);
    return true;
}
