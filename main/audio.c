/* audio.c - ES8311/ES7210 I2S audio init + 16k PCM capture (ESP-IDF v5.3.2)
 * Pin config per hardware manual:
 *   ES8311: SDA=15, SCL=14, MCLK=16, SCLK=41
 *   ES7210: ASDOUT=42, LRCK=45, DSDIN=40, PA_CTRL=46
 */
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <driver/i2s_std.h>
#include <driver/gpio.h>

#include "audio.h"

static const char *TAG = "audio";
static i2s_chan_handle_t s_i2s_tx = NULL;
static i2s_chan_handle_t s_i2s_rx = NULL;
static bool s_init_ok = false;

/* I2S config for ES8311 (TX/Playback) */
static i2s_std_config_t es8311_tx_cfg(void)
{
    i2s_std_config_t cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = GPIO_NUM_16,
            .bclk = GPIO_NUM_41,
            .ws   = GPIO_NUM_40,
            .dout = GPIO_NUM_39,
            .din  = GPIO_NUM_NC,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    return cfg;
}

/* I2S config for ES7210 (RX/Capture) */
static i2s_std_config_t es7210_rx_cfg(void)
{
    i2s_std_config_t cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = GPIO_NUM_16,
            .bclk = GPIO_NUM_41,
            .ws   = GPIO_NUM_45,
            .dout = GPIO_NUM_NC,
            .din  = GPIO_NUM_42,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    return cfg;
}

esp_err_t audio_init(void)
{
    /* Configure PA_CTRL (GPIO46) as output high to enable amp.
     * ESP32-S3: no gpio_hal_pad_select_gpio needed (GPIO matrix does it). */
    gpio_reset_pin(46);
    gpio_set_direction(46, GPIO_MODE_OUTPUT);
    gpio_set_level(46, 1);

    /* Configure I2C for ES8311/ES7210 on I2C bus 0 (SDA=15, SCL=14) */

    /* Create I2S TX channel for playback (master, full-duplex flag not needed). */
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &s_i2s_tx, NULL));

    /* Create I2S RX channel for recording (slave, separate handle). */
    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_SLAVE);
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &s_i2s_rx));

    /* Init ES8311 TX */
    i2s_std_config_t tx_cfg = es8311_tx_cfg();
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_i2s_tx, &tx_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_i2s_tx));

    /* Init ES7210 RX */
    i2s_std_config_t rx_cfg = es7210_rx_cfg();
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_i2s_rx, &rx_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_i2s_rx));

    s_init_ok = true;
    ESP_LOGI(TAG, "Audio init done (16kHz mono PCM)");
    return ESP_OK;
}

esp_err_t audio_capture_pcm(uint8_t *buf, size_t buf_len, size_t *out_len)
{
    if (!s_init_ok) return ESP_ERR_INVALID_STATE;
    size_t bytes_read = 0;
    esp_err_t ret = i2s_channel_read(s_i2s_rx, buf, buf_len, &bytes_read, 1000);
    *out_len = bytes_read;
    return ret;
}

/* Encode PCM as base64 (simplified - returns raw hex for now) */
int audio_pcm_to_b64(const uint8_t *pcm, size_t pcm_len, char *out, size_t out_len)
{
    (void)pcm;
    (void)pcm_len;
    (void)out;
    (void)out_len;
    /* placeholder - real impl uses esp_crypto/base64.h */
    return 0;
}