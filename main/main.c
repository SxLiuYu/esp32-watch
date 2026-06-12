/* esp32-watch main.c
 * Smart home voice frontend for yuanfang-brain WS server
 * Target: ESP32-S3R8 + 2.06" AMOLED + FT3168 + ES8311 + QMI8658 + AXP2101
 */
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_event.h>
#include <nvs_flash.h>

#include "wifi.h"
#include "ws_client.h"
#include "audio.h"
#include "display.h"
#include "touch.h"
#include "power.h"
#include "sensors.h"
#include "ui.h"

static const char *TAG = "main";

#define WS_SERVER_HOST "192.168.1.10"
#define WS_SERVER_PORT 7103
#define WS_SERVER_PATH "/ws/audio"

static void button_handler(void *arg)
{
    int btn = (int)(uintptr_t)arg;
    if (btn == 0) {
        ESP_LOGI(TAG, "BOOT btn short press - start recording");
        ws_client_send_audio_trigger();
    }
}

static void main_task(void *pvParameters)
{
    bool wifi_connected = false;

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* Initialize power management first (AXP2101) */
    ESP_ERROR_CHECK(power_init());

    /* Initialize display */
    ESP_ERROR_CHECK(display_init());
    ui_init();

    /* Initialize touch */
    touch_init();

    /* Initialize sensors */
    sensors_init();

    /* Initialize audio codec */
    ESP_ERROR_CHECK(audio_init());

    /* Connect WiFi */
    ESP_ERROR_CHECK(wifi_init());
    wifi_connected = wifi_connect_with_smartconfig();
    if (!wifi_connected) {
        ESP_LOGW(TAG, "WiFi connect failed, retrying in 30s...");
        vTaskDelay(pdMS_TO_TICKS(30000));
        wifi_connected = wifi_connect_with_smartconfig();
    }

    if (wifi_connected) {
        ESP_LOGI(TAG, "WiFi connected! Connecting to WS server...");
        ws_client_init(WS_SERVER_HOST, WS_SERVER_PORT, WS_SERVER_PATH);

        /* Wait for connection */
        int retries = 0;
        while (!ws_client_is_connected() && retries < 30) {
            vTaskDelay(pdMS_TO_TICKS(500));
            retries++;
        }

        if (ws_client_is_connected()) {
            ESP_LOGI(TAG, "WS connected!");
            ui_set_status("在线");
        } else {
            ESP_LOGW(TAG, "WS server unreachable");
            ui_set_status("离线");
        }
    } else {
        ui_set_status("WiFi失败");
    }

    /* Register BOOT button (GPIO0) for manual trigger */
    wifi_register_button_callback(button_handler, 0);

    /* Main loop */
    while (1) {
        /* Handle touch events */
        touch_process();

        /* Handle WS client events */
        ws_client_task();

        /* Update UI */
        ui_task();

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    xTaskCreatePinnedToCore(main_task, "main_task", 8192, NULL, 5, NULL, 1);
}
