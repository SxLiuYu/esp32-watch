/* power.c - AXP2101 I2C power management + battery
 * I2C pins: SDA=15, SCL=14 (shared with FT3168/QMI8658)
 * AXP2101 I2C address: 0x34
 */
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <driver/i2c.h>
#include <driver/gpio.h>

#include "power.h"

static const char *TAG = "power";
#define AXP2101_ADDR 0x34
static i2c_port_t s_i2c_port = I2C_NUM_0;

static esp_err_t axp_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(s_i2c_port, AXP2101_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t axp_read_reg(uint8_t reg, uint8_t *val)
{
    return i2c_master_write_read_device(s_i2c_port, AXP2101_ADDR, &reg, 1, val, 1, pdMS_TO_TICKS(100));
}

esp_err_t power_init(void)
{
    /* AXP2101 shares I2C0 bus with FT3168/QMI8658 - use same port already init'd */
    /* Verify AXP2101 is present */
    uint8_t id = 0;
    esp_err_t ret = axp_read_reg(0x00, &id);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "AXP2101 not responding at 0x34");
    } else {
        ESP_LOGI(TAG, "AXP2101 ID=0x%02X", id);
    }

    /* Enable charging, set default targets */
    ESP_ERROR_CHECK(axp_write_reg(0x08, 0xC0)); /* CHG current */
    ESP_ERROR_CHECK(axp_write_reg(0x10, 0xE0)); /* DC-DC1 3300mV */
    ESP_ERROR_CHECK(axp_write_reg(0x12, 0xC0)); /* DC-DC2 3300mV */
    ESP_ERROR_CHECK(axp_write_reg(0x14, 0xC0)); /* LDO1  3300mV */
    ESP_ERROR_CHECK(axp_write_reg(0x15, 0xC0)); /* LDO2  3300mV */
    ESP_ERROR_CHECK(axp_write_reg(0x16, 0xC0)); /* LDO3  3300mV */

    ESP_LOGI(TAG, "Power init done (AXP2101)");
    return ESP_OK;
}

int power_get_battery_percent(void)
{
    uint8_t soc = 0;
    axp_read_reg(0x70, &soc); /* State of Charge register */
    return soc & 0x7F;
}

bool power_is_charging(void)
{
    uint8_t status = 0;
    axp_read_reg(0x01, &status);
    return (status & 0x04) != 0;
}

void power_shutdown(void)
{
    /* Send power-off command to AXP2101 */
    axp_write_reg(0x30, 0x80);
}
