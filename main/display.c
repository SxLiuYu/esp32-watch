/* display.c - CO5300 QSPI AMOLED 410x502 (ESP-IDF v5.3.2 + LVGL 9.x)
 *
 * NOTE: The CO5300 panel driver is a third-party component (e.g.
 *       esp_lcd_co5300) that we don't depend on. This module now only
 *       sets up GPIO + LVGL display scaffolding so the firmware links.
 *       Real panel init will be wired once the CO5300 driver is added.
 *
 * QSPI pins: SDIO0=4, SDIO1=5, SDIO2=6, SDIO3=7, SCLK=11, CS=12, RESET=8, TE=13
 */
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <driver/gpio.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_io.h>
#include <lvgl.h>

#include "display.h"

static const char *TAG = "display";
static lv_display_t *s_disp;
/* Partial-mode framebuffer: only need ~10% of full (40 rows) for QSPI partial refresh.
 * 410 * 40 * 2 = 32KB - fits comfortably in internal DRAM. */
static uint16_t s_fb[410 * 40] __attribute__((aligned(4)));

/* LVGL 9.x flush callback - this is the single point where pixels get pushed
 * to the panel. With no CO5300 driver yet we just log; the panel can be
 * wired later without touching the rest of the UI. */
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    (void)disp;
    (void)area;
    (void)px_map;
    lv_display_flush_ready(disp);
}

esp_err_t display_init(void)
{
    /* GPIO setup for QSPI LCD - ESP32-S3 uses GPIO matrix, no pad select needed. */
    gpio_reset_pin(4);
    gpio_reset_pin(5);
    gpio_reset_pin(6);
    gpio_reset_pin(7);
    gpio_reset_pin(8);   /* RESET */
    gpio_reset_pin(11);  /* SCLK */
    gpio_reset_pin(12);  /* CS */
    gpio_reset_pin(13);  /* TE */

    gpio_set_direction(8, GPIO_MODE_OUTPUT);  /* RESET */
    gpio_set_direction(12, GPIO_MODE_OUTPUT); /* CS */
    gpio_set_direction(13, GPIO_MODE_INPUT);  /* TE */

    /* LVGL 9.x init - new API replaces lv_disp_drv_t + lv_disp_register_drv. */
    lv_init();
    s_disp = lv_display_create(410, 502);
    if (!s_disp) {
        ESP_LOGE(TAG, "lv_display_create failed");
        return ESP_FAIL;
    }
    lv_display_set_flush_cb(s_disp, lvgl_flush_cb);
    lv_display_set_buffers(s_disp, s_fb, NULL,
                           sizeof(s_fb), LV_DISPLAY_RENDER_MODE_PARTIAL);

    ESP_LOGI(TAG, "Display init OK (CO5300 QSPI 410x502 - panel driver stub)");
    return ESP_OK;
}

void display_flush(void)
{
    /* Trigger display refresh - handled via lv_display flush_cb. */
    if (s_disp) {
        /* nothing to do here; LVGL timer handler drives refresh */
    }
}