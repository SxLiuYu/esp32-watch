#pragma once
#include <stdbool.h>
#include <esp_err.h>

esp_err_t wifi_init(void);
bool wifi_connect_with_smartconfig(void);
bool wifi_is_connected(void);
void wifi_register_button_callback(void (*cb)(void*), void *arg);
