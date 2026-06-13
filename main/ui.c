/* ui.c - LVGL 9.x UI for voice frontend
 * Shows: microphone icon, status text, battery indicator
 */
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <lvgl.h>

#include "ui.h"
#include "power.h"

static const char *TAG = "ui";
static lv_obj_t *s_status_label;
static lv_obj_t *s_mic_btn;
static lv_obj_t *s_battery_bar;
static char s_status_text[64] = "初始化中...";

static void mic_click_cb(lv_event_t *ev)
{
    (void)ev;
    ESP_LOGI(TAG, "Mic button clicked");
    /* Trigger audio recording */
    extern void ws_client_send_audio_trigger(void);
    ws_client_send_audio_trigger();
}

esp_err_t ui_init(void)
{
    lv_obj_t *scr = lv_scr_create();

    /* Status label at top - use default font so we don't depend on a
     * specific Montserrat size being enabled in lv_conf.h. */
    s_status_label = lv_label_create(scr);
    lv_label_set_text(s_status_label, s_status_text);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_font(s_status_label, lv_font_default(), 0);

    /* Microphone button in center */
    s_mic_btn = lv_btn_create(scr);
    lv_obj_set_size(s_mic_btn, 80, 80);
    lv_obj_align(s_mic_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(s_mic_btn, 40, 0);
    lv_obj_add_event_cb(s_mic_btn, mic_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *mic_label = lv_label_create(s_mic_btn);
    lv_label_set_text(mic_label, LV_SYMBOL_AUDIO);
    lv_obj_align(mic_label, LV_ALIGN_CENTER, 0, 0);

    /* Battery bar at bottom */
    s_battery_bar = lv_bar_create(scr);
    lv_obj_set_size(s_battery_bar, 100, 8);
    lv_obj_align(s_battery_bar, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_bar_set_range(s_battery_bar, 0, 100);
    lv_bar_set_value(s_battery_bar, power_get_battery_percent(), LV_ANIM_OFF);

    lv_obj_t *bat_label = lv_label_create(scr);
    lv_label_set_text(bat_label, LV_SYMBOL_BATTERY);
    lv_obj_align(bat_label, LV_ALIGN_BOTTOM_MID, 0, -5);

    ESP_LOGI(TAG, "UI init done");
    return ESP_OK;
}

void ui_set_status(const char *status)
{
    strncpy(s_status_text, status, sizeof(s_status_text) - 1);
    if (s_status_label) {
        lv_label_set_text(s_status_label, s_status_text);
    }
}

void ui_task(void)
{
    /* Update battery */
    if (s_battery_bar) {
        lv_bar_set_value(s_battery_bar, power_get_battery_percent(), LV_ANIM_OFF);
    }
    /* LVGL 9.x: lv_timer_handler() (lv_timer_handler_run_in_period is gone). */
    lv_timer_handler();
}