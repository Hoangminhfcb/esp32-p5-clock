idf_component_register(
    SRCS "main.cpp" "hal/panel.cpp" "hal/button.cpp"
         "gfx/clock_face.cpp" "gfx/color.cpp"
         "app/clock.cpp" "app/lunar.cpp"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_event esp_netif esp_http_server esp_timer json driver ESP32-HUB75-MatrixPanel-DMA
)
