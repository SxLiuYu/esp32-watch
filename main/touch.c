/* touch.c - FT3168 capacitive touch I2C init (ESP-IDF v5.3.2 legacy I2C API)
 * I2C pins: SDA=15, SCL=14, TP_INT=38, TP_RESET=9
 */
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <driver/i2c.h>
#include <driver/gpio.h>

#include "touch.h"

static const char *TAG = "touch";
static i2c_port_t s_i2c_port = I2C_NUM_0;
#define FT3168_ADDR 0x38

static esp_err_t i2c_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(s_i2c_port, FT3168_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t i2c_read_reg(uint8_t reg, uint8_t *val)
{
    return i2c_master_write_read_device(s_i2c_port, FT3168_ADDR, &reg, 1, val, 1, pdMS_TO_TICKS(100));
}

esp_err_t touch_init(void)
{
    /* ESP32-S3: GPIO matrix routes all pins - no gpio_hal_pad_select_gpio. */
    gpio_reset_pin(38);
    gpio_set_direction(38, GPIO_MODE_INPUT);
    gpio_reset_pin(9);
    gpio_set_direction(9, GPIO_MODE_OUTPUT);

    /* Reset FT3168 */
    gpio_set_level(9, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(9, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    /* v5.3 legacy i2c_config_t field renames: scl_io_num/sda_io_num, clk_flags. */
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_15,
        .scl_io_num = GPIO_NUM_14,
        .sda_pullup_en = false,
        .scl_pullup_en = false,
        .master.clk_speed = 400000,
        .clk_flags = 0,
    };
    ESP_ERROR_CHECK(i2c_param_config(s_i2c_port, &cfg));
    ESP_ERROR_CHECK(i2c_driver_install(s_i2c_port, cfg.mode, 0, 0, 0));

    /* Check FT3168 chip ID */
    uint8_t id = 0;
    esp_err_t ret = i2c_read_reg(0xA8, &id);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "FT3168 not responding at 0x38");
    } else {
        ESP_LOGI(TAG, "FT3168 ID=0x%02X", id);
    }

    /* Enable touch interrupt on GPIO38 */
    ESP_ERROR_CHECK(i2c_write_reg(0xD0, 0x01)); /* enable interrupt */
    ESP_LOGI(TAG, "Touch init done (FT3168 I2C)");
    return ESP_OK;
}

void touch_process(void)
{
    /* Read touch points from FT3168 */
    uint8_t status = 0;
    if (i2c_read_reg(0x02, &status) != ESP_OK) return;
    if ((status & 0x80) == 0) return; /* no touch */

    uint8_t num_points = status & 0x0F;
    if (num_points == 0 || num_points > 5) return;

    uint8_t buf[5 * 4];
    /* Read touch data registers - simplified */
    (void)buf;
    /* Process touch for UI interaction */
}