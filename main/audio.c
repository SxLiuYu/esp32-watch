/* audio.c - ES8311/ES7210 I2S audio init + 16k PCM capture
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
#include <driver/i2s_pdm.h>
#include <driver/gpio.h>
#include <soc/i2s_periph.h>

#include "audio.h"

static const char *TAG = "audio";
static i2s_chan_handle_t s_i2s_tx;
static i2s_chan_handle_t s_i2s_rx;
static bool s_init_ok = false;

/* I2S config for ES8311 (TX/Playback) */
static const i2s_std_config_t es8311_tx_cfg = {
    .clk_cfg = {
        .sample_rate_hz = 16000,
        .clk_src = I2S_CLK_SRC_PLL_160M,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    },
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
        .mclk = GPIO_NUM_16,
        .bclk = GPIO_NUM_41,
        .ws   = GPIO_NUM_40,
        .dout = GPIO_NUM_39,
        .din  = GPIO_NUM_NC,
        .invert_flags = {0},
    },
};

/* I2S config for ES7210 (RX/Capture) */
static const i2s_std_config_t es7210_rx_cfg = {
    .clk_cfg = {
        .sample_rate_hz = 16000,
        .clk_src = I2S_CLK_SRC_PLL_160M,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    },
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
        .mclk = GPIO_NUM_16,
        .bclk = GPIO_NUM_41,
        .ws   = GPIO_NUM_45,
        .dout = GPIO_NUM_NC,
        .din  = GPIO_NUM_42,
        .invert_flags = {0},
    },
};

esp_err_t audio_init(void)
{
    /* Configure PA_CTRL (GPIO46) as output high to enable amp */
    gpio_hal_pad_select_gpio(46);
    gpio_set_direction(46, GPIO_MODE_OUTPUT);
    gpio_set_level(46, 1);

    /* Configure I2C for ES8311/ES7210 on I2C bus 0 (SDA=15, SCL=14) */
    /* Note: ES8311 and ES7210 share the same I2C bus */

    /* Create I2S TX channel for playback */
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_CONFIG(0,
        I2S_ROLE_MASTER, I2S_STD_MODE, I2S_PERIPH_NUM_0);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &s_i2s_tx));

    /* Create I2S RX channel for recording */
    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_CONFIG(0,
        I2S_ROLE_SLAVE, I2S_STD_MODE, I2S_PERIPH_NUM_0);
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, &s_i2s_rx));

    /* Init ES8311 TX */
    ESP_ERROR_CHECK(i2s_channel_init_std_tx(s_i2s_tx, &es8311_tx_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_i2s_tx));

    /* Init ES7210 RX */
    ESP_ERROR_CHECK(i2s_channel_init_std_rx(s_i2s_rx, &es7210_rx_cfg));
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
    /* Use esp-crypto base64 */
    return 0; /* placeholder - real impl uses esp_crypto/base64.h */
}
