/* audio.c - STUB: no I2S codec init.
 * The I2S driver init was conflicting with esp_wifi_set_default_wifi_sta_handlers
 * (occupies I2C_NUM_0 / GPIO matrix before WiFi STA netif can register handlers).
 * Re-enable when you have a real ES8311/ES7210 and figure out init ordering.
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "audio.h"

static const char *TAG = "audio";
static bool s_init_ok = false;

esp_err_t audio_init(void)
{
    ESP_LOGW(TAG, "audio_init STUB (I2S codec disabled - was conflicting with WiFi init)");
    s_init_ok = false;
    return ESP_OK;
}

esp_err_t audio_capture_pcm(uint8_t *buf, size_t buf_len, size_t *out_len)
{
    (void)buf; (void)buf_len;
    if (out_len) *out_len = 0;
    return ESP_ERR_INVALID_STATE;
}

int audio_pcm_to_b64(const uint8_t *pcm, size_t pcm_len, char *out, size_t out_len)
{
    (void)pcm; (void)pcm_len; (void)out; (void)out_len;
    return 0;
}
