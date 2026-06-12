#pragma once
#include <esp_err.h>

esp_err_t sensors_init(void);
void sensors_read_imu(float *ax, float *ay, float *az, float *gx, float *gy, float *gz);
void sensors_read_rtc(int *year, int *month, int *day, int *hour, int *min, int *sec);
