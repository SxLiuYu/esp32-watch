/* wifi.c - WiFi STA + SmartConfig fallback (ESP-IDF v5.3.2 API)
 *
 * Modified 2026-06-14:
 * - Idempotent init: esp_netif_create_default_wifi_sta() is NOT idempotent
 *   in IDF v5.3.2 (returns ESP_ERR_INVALID_STATE on second call -> abort).
 *   We guard with a flag.
 * - Split wifi_init() (driver only) and wifi_connect_sta(ssid, pass).
 */
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <esp_smartconfig.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <nvs_flash.h>

#include "wifi.h"

static const char *TAG = "wifi";
static bool s_connected = false;
static bool s_sc_done = false;
static bool s_netif_ready = false;
static void (*s_btn_cb)(void*) = NULL;
static void *s_btn_arg = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t ev_base,
                               int32_t ev_id, void *ev_data)
{
    if (ev_base == WIFI_EVENT && ev_id == WIFI_EVENT_STA_CONNECTED) {
        s_connected = true;
        ESP_LOGI(TAG, "WiFi STA connected");
    } else if (ev_base == WIFI_EVENT && ev_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        ESP_LOGW(TAG, "WiFi disconnected, retrying...");
        esp_wifi_connect();
    } else if (ev_base == WIFI_EVENT && ev_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA started");
    } else if (ev_base == IP_EVENT && ev_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)ev_data;
        ESP_LOGI(TAG, "*** Got IP: " IPSTR " ***", IP2STR(&ev->ip_info.ip));
    }
}

static void sc_event_handler(void *arg, esp_event_base_t ev_base,
                             int32_t ev_id, void *ev_data)
{
    if (ev_base == SC_EVENT && ev_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "SmartConfig scan done");
    } else if (ev_base == SC_EVENT && ev_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG, "SmartConfig found channel");
    } else if (ev_base == SC_EVENT && ev_id == SC_EVENT_GOT_SSID_PSWD) {
        smartconfig_event_got_ssid_pswd_t *ev = (smartconfig_event_got_ssid_pswd_t *)ev_data;
        wifi_config_t wifi_config = {0};
        memcpy(wifi_config.sta.ssid, ev->ssid, sizeof(ev->ssid));
        memcpy(wifi_config.sta.password, ev->password, sizeof(ev->password));
        wifi_config.sta.bssid_set = ev->bssid_set;
        if (ev->bssid_set) memcpy(wifi_config.sta.bssid, ev->bssid, 6);

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_connect());
        s_sc_done = true;
        ESP_LOGI(TAG, "SmartConfig got SSID/PASS");
    }
}

static void smartconfig_task(void *parm)
{
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    smartconfig_start_config_t cfg = { .enable_log = true };
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    while (!s_sc_done) vTaskDelay(pdMS_TO_TICKS(500));
    esp_smartconfig_stop();
    vTaskDelete(NULL);
}

static void button_monitor_task(void *parm)
{
    gpio_reset_pin(0);
    gpio_set_direction(0, GPIO_MODE_INPUT);
    gpio_set_pull_mode(0, GPIO_PULLUP_ONLY);

    bool last = gpio_get_level(0);
    int64_t press_time = 0;
    while (1) {
        bool cur = gpio_get_level(0);
        int64_t now = esp_timer_get_time();
        if (!cur && last) {
            press_time = now;
        } else if (cur && !last && press_time > 0) {
            int64_t dur = now - press_time;
            if (dur > 50000 && dur < 6000000 && s_btn_cb) {
                s_btn_cb(s_btn_arg);
            }
        }
        last = cur;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

esp_err_t wifi_init(void)
{
    /* Idempotent netif init.
     * NOTE: esp_netif_init() is idempotent (safe to call multiple times).
     * esp_netif_create_default_wifi_sta() is NOT idempotent in IDF v5.3.2.
     * We guard with a flag and return ESP_OK if already created. */
    esp_err_t r = esp_netif_init();
    if (r != ESP_OK && r != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(r));
        return r;
    }
    if (!s_netif_ready) {
        esp_netif_t *sta = esp_netif_create_default_wifi_sta();
        if (sta == NULL) {
            /* Could be already created by another component - just continue */
            ESP_LOGW(TAG, "esp_netif_create_default_wifi_sta returned NULL (may already exist) - continuing");
        }
        s_netif_ready = true;
    }

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               wifi_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    xTaskCreatePinnedToCore(button_monitor_task, "btn_mon", 4096, NULL, 4, NULL, 0);
    return ESP_OK;
}

bool wifi_connect_sta(const char *ssid, const char *pass)
{
    if (!ssid || !pass) {
        ESP_LOGE(TAG, "ssid or pass is NULL");
        return false;
    }
    wifi_config_t sta_config = {0};
    strncpy((char*)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid) - 1);
    strncpy((char*)sta_config.sta.password, pass, sizeof(sta_config.sta.password) - 1);
    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid);
    esp_err_t err;
    err = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_config failed: %s", esp_err_to_name(err));
        return false;
    }
    err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "connect failed: %s", esp_err_to_name(err));
        return false;
    }
    int wait = 0;
    while (!s_connected && wait < 40) {
        vTaskDelay(pdMS_TO_TICKS(500));
        wait++;
    }
    return s_connected;
}

bool wifi_connect_with_smartconfig(void)
{
    s_sc_done = false;
    ESP_LOGI(TAG, "Starting SmartConfig (ESPTOUCH)...");
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID,
                                              sc_event_handler, NULL));
    xTaskCreatePinnedToCore(smartconfig_task, "sc_task", 6144, NULL, 3, NULL, 0);

    int wait = 0;
    while (!s_sc_done && wait < 240) {
        vTaskDelay(pdMS_TO_TICKS(500));
        wait++;
    }
    return s_sc_done;
}

bool wifi_is_connected(void) { return s_connected; }

void wifi_register_button_callback(void (*cb)(void*), void *arg)
{
    s_btn_cb = cb;
    s_btn_arg = arg;
}
