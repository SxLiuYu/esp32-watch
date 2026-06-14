/* esp32-watch main.c
 * Smart home voice frontend for yuanfang-brain WS server
 * Target: ESP32-S3R8 + 2.06" AMOLED + FT3168 + ES8311 + QMI8658 + AXP2101
 *
 * Modified 2026-06-14:
 * - WiFi credentials hardcoded (development board)
 * - Skip SmartConfig (NVS empty or credentials match -> connect directly)
 * - Add log of IP after WiFi up
 * - Init ws_client as soon as WiFi is up
 */
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_wifi.h>
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

#define WIFI_SSID    "CMCC-egTm"
#define WIFI_PASS    "fneme97c"
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

/* Wait for IP event - set in event handler registered in wifi_init */
static volatile bool s_got_ip = false;

static void ip_event_handler(void *arg, esp_event_base_t ev_base,
                              int32_t ev_id, void *ev_data)
{
    if (ev_base == IP_EVENT && ev_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)ev_data;
        ESP_LOGI(TAG, "*** Got IP: " IPSTR " ***", IP2STR(&ev->ip_info.ip));
        s_got_ip = true;
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

    /* Initialize power management first (AXP2101) - soft-fail OK */
    ESP_ERROR_CHECK(power_init());

    /* Initialize display */
    ESP_ERROR_CHECK(display_init());
    ui_init();

    /* Initialize touch - soft-fail OK */
    touch_init();

    /* Initialize sensors - soft-fail OK */
    sensors_init();

    /* Initialize audio codec - soft-fail OK */
    ESP_ERROR_CHECK(audio_init());

    /* Connect WiFi - directly with hardcoded credentials */
    ESP_ERROR_CHECK(wifi_init());
    /* Register IP event to know when we have a real IP */
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               ip_event_handler, NULL));
    s_got_ip = false;
    wifi_connected = wifi_connect_sta(WIFI_SSID, WIFI_PASS);
    if (!wifi_connected) {
        ESP_LOGW(TAG, "WiFi connect failed, retrying in 30s...");
        vTaskDelay(pdMS_TO_TICKS(30000));
        s_got_ip = false;
        wifi_connected = wifi_connect_sta(WIFI_SSID, WIFI_PASS);
    }

    if (wifi_connected && s_got_ip) {
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
            ui_set_status("WS未连");
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
