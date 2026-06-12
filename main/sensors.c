/* sensors.c - QMI8658 IMU + PCF85063 RTC
 * QMI8658: SDA=15, SCL=14, INT=21 (shared I2C)
 * PCF85063: SDA=15, SCL=14, INT=39 (shared I2C)
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

/* QMI8658 registers */
#define QMI_WHOAMI   0x00
#define QMI_CTRL1    0x01
#define QMI_CTRL2    0x02
#define QMI_CTRL3    0x03
#define QMI_DATA     0x00

static i2c_port_t s_i2c_port = I2C_NUM_0;

static esp_err_t qmi_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(s_i2c_port, QMI8658_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t qmi_read_reg(uint8_t reg, uint8_t *val)
{
    return i2c_master_write_read_device(s_i2c_port, QMI8658_ADDR, &reg, 1, val, 1, pdMS_TO_TICKS(100));
}

static esp_err_t pcf_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(s_i2c_port, PCF85063_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t pcf_read_reg(uint8_t reg, uint8_t *val)
{
    return i2c_master_write_read_device(s_i2c_port, PCF85063_ADDR, &reg, 1, val, 1, pdMS_TO_TICKS(100));
}

esp_err_t sensors_init(void)
{
    /* QMI8658 init */
    uint8_t whoami = 0;
    esp_err_t ret = qmi_read_reg(QMI_WHOAMI, &whoami);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "QMI8658 not responding");
    } else {
        ESP_LOGI(TAG, "QMI8658 WHOAMI=0x%02X", whoami);
        /* Enable accel + gyro, 16g/2000dps, 500Hz ODR */
        qmi_write_reg(QMI_CTRL1, 0x60);
        qmi_write_reg(QMI_CTRL2, 0x60);
        ESP_LOGI(TAG, "QMI8658 init done");
    }

    /* PCF85063 RTC init */
    uint8_t rtc_id = 0;
    ret = pcf_read_reg(0x02, &rtc_id);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "PCF85063 not responding");
    } else {
        ESP_LOGI(TAG, "PCF85063 RTC init done");
        pcf_write_reg(0x00, 0x00); /* Control/counter mode */
    }

    return ESP_OK;
}

void sensors_read_imu(float *ax, float *ay, float *az, float *gx, float *gy, float *gz)
{
    uint8_t buf[12];
    /* Read 6-axis raw data - simplified, real impl reads DATA registers */
    (void)buf;
    *ax = *ay = *az = 0;
    *gx = *gy = *gz = 0;
}

void sensors_read_rtc(int *year, int *month, int *day, int *hour, int *min, int *sec)
{
    uint8_t t[6] = {0};
    pcf_read_reg(0x04, &t[0]); /* seconds */
    pcf_read_reg(0x05, &t[1]); /* minutes */
    pcf_read_reg(0x06, &t[2]); /* hours */
    pcf_read_reg(0x07, &t[3]); /* days */
    pcf_read_reg(0x08, &t[4]); /* weekdays */
    pcf_read_reg(0x09, &t[5]); /* months */
    *sec  = (t[0] & 0x7F);
    *min  = (t[1] & 0x7F);
    *hour = (t[2] & 0x3F);
    *day  = (t[3] & 0x3F);
    *month = (t[5] & 0x1F);
    *year = 2000 + (t[4] & 0xFF);
}
