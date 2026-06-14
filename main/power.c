/* power.c - AXP2101 I2C power management + battery
 * I2C pins: SDA=15, SCL=14 (shared with FT3168/QMI8658)
 * AXP2101 I2C address: 0x34
 *
 * Modified 2026-06-14: soft-fail if I2C driver not installed
 * (this board variant may not have AXP2101 PMIC)
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
static bool s_i2c_ready = false;

/* Check if I2C driver is installed on the port */
static bool i2c_is_ready(void)
{
    /* Try a no-op probe: if driver not installed i2c_master_* will fail with
     * ESP_ERR_INVALID_STATE. We use a quick write of 0 bytes as a probe. */
    uint8_t dummy = 0;
    esp_err_t r = i2c_master_write_to_device(s_i2c_port, AXP2101_ADDR, &dummy, 0, pdMS_TO_TICKS(20));
    if (r == ESP_ERR_INVALID_STATE) return false;
    return true;
}

static esp_err_t axp_write_reg(uint8_t reg, uint8_t val)
{
    if (!s_i2c_ready) return ESP_ERR_INVALID_STATE;
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(s_i2c_port, AXP2101_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t axp_read_reg(uint8_t reg, uint8_t *val)
{
    if (!s_i2c_ready) return ESP_ERR_INVALID_STATE;
    return i2c_master_write_read_device(s_i2c_port, AXP2101_ADDR, &reg, 1, val, 1, pdMS_TO_TICKS(100));
}

esp_err_t power_init(void)
{
    /* Detect I2C bus readiness */
    s_i2c_ready = i2c_is_ready();
    if (!s_i2c_ready) {
        ESP_LOGW(TAG, "I2C driver not installed on port %d - skipping AXP2101 init (this board may not have AXP2101)", s_i2c_port);
        return ESP_OK;  /* soft fail, allow system to continue */
    }

    /* Verify AXP2101 is present */
    uint8_t id = 0;
    esp_err_t ret = axp_read_reg(0x00, &id);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "AXP2101 not responding at 0x34 - continuing without PMIC");
        s_i2c_ready = false;  /* don't keep trying to talk to missing chip */
        return ESP_OK;  /* soft fail */
    }
    ESP_LOGI(TAG, "AXP2101 ID=0x%02X", id);

    /* Enable charging, set default targets (best-effort) */
    esp_err_t err;
    #define TRY_WRITE(reg, val) do { err = axp_write_reg((reg), (val)); if (err != ESP_OK) ESP_LOGW(TAG, "AXP reg 0x%02X write failed: %s", (reg), esp_err_to_name(err)); } while(0)
    TRY_WRITE(0x08, 0xC0);  /* CHG current */
    TRY_WRITE(0x10, 0xE0);  /* DC-DC1 3300mV */
    TRY_WRITE(0x12, 0xC0);  /* DC-DC2 3300mV */
    TRY_WRITE(0x14, 0xC0);  /* LDO1  3300mV */
    TRY_WRITE(0x15, 0xC0);  /* LDO2  3300mV */
    TRY_WRITE(0x16, 0xC0);  /* LDO3  3300mV */
    #undef TRY_WRITE

    ESP_LOGI(TAG, "Power init done (AXP2101)");
    return ESP_OK;
}

int power_get_battery_percent(void)
{
    if (!s_i2c_ready) return -1;
    uint8_t soc = 0;
    if (axp_read_reg(0x70, &soc) != ESP_OK) return -1;
    return soc & 0x7F;
}

bool power_is_charging(void)
{
    if (!s_i2c_ready) return false;
    uint8_t status = 0;
    if (axp_read_reg(0x01, &status) != ESP_OK) return false;
    return (status & 0x04) != 0;
}

void power_shutdown(void)
{
    /* Send power-off command to AXP2101 (no-op if PMIC absent) */
    axp_write_reg(0x30, 0x80);
}
