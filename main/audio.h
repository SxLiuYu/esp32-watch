#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

esp_err_t audio_init(void);
esp_err_t audio_capture_pcm(uint8_t *buf, size_t buf_len, size_t *out_len);
int audio_pcm_to_b64(const uint8_t *pcm, size_t pcm_len, char *out, size_t out_len);
