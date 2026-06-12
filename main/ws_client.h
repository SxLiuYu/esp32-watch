#pragma once
#include <stdbool.h>
#include <esp_err.h>

esp_err_t ws_client_init(const char *host, int port, const char *path);
bool ws_client_is_connected(void);
void ws_client_send_audio_trigger(void);
void ws_client_send_text(const char *text);
void ws_client_send_audio(const char *b64_pcm, int pcm_len);
void ws_client_task(void);
void ws_client_deinit(void);
