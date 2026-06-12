/* display.c - CO5300 QSPI AMOLED 410x502 init + LVGL rendering
 * QSPI pins: SDIO0=4, SDIO1=5, SDIO2=6, SDIO3=7, SCLK=11, CS=12, RESET=8, TE=13
 */
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <driver/gpio.h>
#include <esp_lcd_common.h>
#include <esp_lcd_panel_co5300.h>
#include <esp_lcd_panel_ops.h>
#include <lvgl.h>

#include "display.h"

static const char *TAG = "display";
static lv_disp_t *s_disp;
static lv_disp_draw_buf_t s_disp_buf;
static uint16_t s_fb[410 * 502] __attribute__((aligned(4)));

esp_err_t display_init(void)
{
    /* GPIO init for QSPI LCD */
    gpio_hal_pad_select_gpio(4);
    gpio_hal_pad_select_gpio(5);
    gpio_hal_pad_select_gpio(6);
    gpio_hal_pad_select_gpio(7);
    gpio_hal_pad_select_gpio(11);
    gpio_hal_pad_select_gpio(12);
    gpio_hal_pad_select_gpio(8);
    gpio_hal_pad_select_gpio(13);

    gpio_set_direction(8, GPIO_MODE_OUTPUT); /* RESET */
    gpio_set_direction(12, GPIO_MODE_OUTPUT); /* CS */
    gpio_set_direction(13, GPIO_MODE_INPUT); /* TE */

    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = 8,
        .cs_gpio_num = 12,
        .te_gpio_num = 13,
        .flags = {
            .swap_color_bytes = 0,
        },
        .bus_format = LCD_BUS_TYPE_QSPI,
    };

    /* QSPI bus config */
    esp_lcd_qspi_bus_config_t bus_cfg = {
        .data_gpio_nums = {4, 5, 6, 7},
        .clk_gpio_num = 11,
        .bus_width = 4,
    };

    ESP_ERROR_CHECK(esp_lcd_new_qspi_bus(&bus_cfg, &s_disp));
    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(s_disp, &panel_cfg, &panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

    /* LVGL init */
    lv_init();
    lv_disp_draw_buf_init(&s_disp_buf, s_fb, NULL, sizeof(s_fb) / sizeof(uint16_t));
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 410;
    disp_drv.ver_res = 502;
    disp_drv.draw_buf = &s_disp_buf;
    disp_drv.flush_cb = NULL; /* Direct DMA flush - impl depends on board */
    s_disp = lv_disp_register_drv(&disp_drv);

    ESP_LOGI(TAG, "Display init OK (CO5300 QSPI 410x502)");
    return ESP_OK;
}

void display_flush(void)
{
    /* Trigger display refresh */
}
