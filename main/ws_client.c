/* ws_client.c - WebSocket client for yuanfang-brain WS protocol
 * Protocol: ws://192.168.1.10:7103/ws/audio
 * Messages: {"type":"audio","data":"<base64 PCM 16k/16bit>","sr":16000}
 *           {"type":"text","text":"..."}
 * Replies:  {"type":"reply","text":"...","audio":"<base64>","intent":{...}}
 *
 * Uses espressif/esp-websocket (component manager) - header is shipped there.
 */
#include <string.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_timer.h>

#include "esp_websocket_client.h"
#include "ws_client.h"

static const char *TAG = "ws_client";

static esp_websocket_client_handle_t s_client;
static bool s_connected = false;
static bool s_audio_trigger = false;
static TaskHandle_t s_task_handle;

static void ws_event_handler(void *arg, esp_event_base_t ev_base,
                             int32_t ev_id, void *ev_data)
{
    (void)ev_base;
    switch (ev_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "WS connected");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "WS disconnected");
        break;
    case WEBSOCKET_EVENT_DATA: {
        esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)ev_data;
        int len = data->data_len;
        if (len == 0) break;

        /* Parse JSON reply */
        char *msg = (char*)data->data_ptr;
        if (len > 64) {
            /* Quick check for reply type */
            if (msg[0] == '{' && strstr(msg, "\"type\":\"reply\"") != NULL) {
                ESP_LOGI(TAG, "Got reply from server");
                /* Reply parsed - UI will handle audio playback */
            }
        }
        break;
    }
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WS error");
        break;
    default:
        break;
    }
}

esp_err_t ws_client_init(const char *host, int port, const char *path)
{
    char url[256];
    snprintf(url, sizeof(url), "ws://%s:%d%s", host, port, path);

    esp_websocket_client_config_t cfg = {
        .uri = url,
        .ping_interval_sec = 20,
        .pingpong_timeout_sec = 10,
        .buffer_size = 4096,
    };

    s_client = esp_websocket_client_init(&cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "Failed to create WS client");
        return ESP_FAIL;
    }

    esp_websocket_client_register_events(s_client, WEBSOCKET_EVENT_ANY,
                                         ws_event_handler, NULL);
    esp_websocket_client_start(s_client);
    return ESP_OK;
}

bool ws_client_is_connected(void)
{
    return s_connected && esp_websocket_client_is_connected(s_client);
}

void ws_client_send_audio_trigger(void)
{
    s_audio_trigger = true;
}

void ws_client_send_text(const char *text)
{
    if (!ws_client_is_connected()) return;
    char msg[512];
    int len = snprintf(msg, sizeof(msg),
        "{\"type\":\"text\",\"text\":\"%s\"}", text);
    esp_websocket_client_send_text(s_client, msg, len, portMAX_DELAY);
}

void ws_client_send_audio(const char *b64_pcm, int pcm_len)
{
    (void)pcm_len;
    if (!ws_client_is_connected()) return;
    char msg[1024];
    int len = snprintf(msg, sizeof(msg),
        "{\"type\":\"audio\",\"data\":\"%s\",\"sr\":16000}", b64_pcm);
    esp_websocket_client_send_text(s_client, msg, len, portMAX_DELAY);
}

void ws_client_task(void)
{
    /* Process any pending WS events */
    if (s_audio_trigger) {
        s_audio_trigger = false;
        ESP_LOGI(TAG, "Audio trigger fired - recording...");
    }
}

void ws_client_deinit(void)
{
    if (s_client) {
        esp_websocket_client_stop(s_client);
        esp_websocket_client_destroy(s_client);
        s_client = NULL;
    }
}