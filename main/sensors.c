/* sensors.c - QMI8658 IMU + PCF85063 RTC
 * QMI8658: SDA=15, SCL=14, INT=21 (shared I2C)
 * PCF85063: SDA=15, SCL=14, INT=39 (shared I2C)
 *
 * Modified 2026-06-14: soft-fail when I2C not installed
 * (this board may not have QMI8658 or PCF85063)
 */
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <driver/i2c.h>
#include <driver/gpio.h>

#include "sensors.h"

static const char *TAG = "sensors";
#define QMI8658_ADDR 0x68
#define PCF85063_ADDR 0x51
static bool s_sensors_ready = false;
static i2c_port_t s_i2c_port = I2C_NUM_0;

static esp_err_t qmi_write_reg(uint8_t reg, uint8_t val)
{
    if (!s_sensors_ready) return ESP_ERR_INVALID_STATE;
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(s_i2c_port, QMI8658_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t qmi_read_reg(uint8_t reg, uint8_t *val)
{
    if (!s_sensors_ready) return ESP_ERR_INVALID_STATE;
    return i2c_master_write_read_device(s_i2c_port, QMI8658_ADDR, &reg, 1, val, 1, pdMS_TO_TICKS(100));
}

static esp_err_t pcf_write_reg(uint8_t reg, uint8_t val)
{
    if (!s_sensors_ready) return ESP_ERR_INVALID_STATE;
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(s_i2c_port, PCF85063_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t pcf_read_reg(uint8_t reg, uint8_t *val)
{
    if (!s_sensors_ready) return ESP_ERR_INVALID_STATE;
    return i2c_master_write_read_device(s_i2c_port, PCF85063_ADDR, &reg, 1, val, 1, pdMS_TO_TICKS(100));
}

esp_err_t sensors_init(void)
{
    /* Check if I2C driver is installed (use QMI8658 as probe) */
    uint8_t dummy = 0;
    esp_err_t probe = i2c_master_write_to_device(s_i2c_port, QMI8658_ADDR, &dummy, 0, pdMS_TO_TICKS(20));
    if (probe == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "I2C driver not installed - sensors disabled");
        return ESP_OK;
    }
    s_sensors_ready = true;

    /* QMI8658 init */
    uint8_t whoami = 0;
    esp_err_t ret = qmi_read_reg(QMI_WHOAMI, &whoami);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "QMI8658 not responding at 0x68 - continuing");
    } else {
        ESP_LOGI(TAG, "QMI8658 WHOAMI=0x%02X", whoami);
        qmi_write_reg(QMI_CTRL1, 0x60);
        qmi_write_reg(QMI_CTRL2, 0x60);
        ESP_LOGI(TAG, "QMI8658 init done");
    }

    /* PCF85063 RTC init */
    uint8_t rtc_id = 0;
    ret = pcf_read_reg(0x02, &rtc_id);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "PCF85063 not responding at 0x51 - continuing");
    } else {
        ESP_LOGI(TAG, "PCF85063 RTC init done");
        pcf_write_reg(0x00, 0x00);
    }

    return ESP_OK;
}

void sensors_read_imu(float *ax, float *ay, float *az, float *gx, float *gy, float *gz)
{
    if (!s_sensors_ready) {
        *ax = *ay = *az = 0;
        *gx = *gy = *gz = 0;
        return;
    }
    uint8_t buf[12];
    (void)buf;
    *ax = *ay = *az = 0;
    *gx = *gy = *gz = 0;
}

void sensors_read_rtc(int *year, int *month, int *day, int *hour, int *min, int *sec)
{
    if (!s_sensors_ready) {
        *year = 2026; *month = 1; *day = 1;
        *hour = 0; *min = 0; *sec = 0;
        return;
    }
    uint8_t t[6] = {0};
    pcf_read_reg(0x04, &t[0]);
    pcf_read_reg(0x05, &t[1]);
    pcf_read_reg(0x06, &t[2]);
    pcf_read_reg(0x07, &t[3]);
    pcf_read_reg(0x08, &t[4]);
    pcf_read_reg(0x09, &t[5]);
    *sec  = (t[0] & 0x7F);
    *min  = (t[1] & 0x7F);
    *hour = (t[2] & 0x3F);
    *day  = (t[3] & 0x3F);
    *month = (t[5] & 0x1F);
    *year = 2000 + (t[4] & 0xFF);
}
