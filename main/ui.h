#pragma once
#include <esp_err.h>

esp_err_t ui_init(void);
void ui_set_status(const char *status);
void ui_task(void);
