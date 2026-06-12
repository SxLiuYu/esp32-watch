#pragma once
#include <esp_err.h>

esp_err_t power_init(void);
int power_get_battery_percent(void);
bool power_is_charging(void);
void power_shutdown(void);
