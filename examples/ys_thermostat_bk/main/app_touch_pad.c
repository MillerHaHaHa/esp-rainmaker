/**
 * @file app_touch_pad.c
 * @author huangmingle (lelee123a@163.com)
 * @brief 
 * @version 1.0
 * @date 2022-12-30
 * 
 * @copyright Copyright (c) 2022 Leedarson All Rights Reserved.
 * 
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
// #include "esp_timer.h"

#include "driver/touch_pad.h"
#include "soc/rtc_periph.h"
#include "soc/sens_periph.h"
#include "app_touch_pad.h"

static const char *TAG = "TouchPad";

#define TOUCH_THRESH_NO_USE   (0)
#define TOUCH_INTERRUPT_THRESH_PERCENT  (90)
#define TOUCHPAD_FILTER_TOUCH_PERIOD (10)
#define TOUCHPAD_COUNT   6

static app_touch_pad_cfg_t s_pad_cfg = {0};

typedef struct {
    uint8_t num;
    bool activated;
    uint32_t activated_time;
    uint32_t init_val;
    // esp_timer_handle_t timer;
} s_pad_obj_t;

static s_pad_obj_t s_pad_obj[TOUCHPAD_COUNT] = {
    {.num = 0}, // key power
    {.num = 4}, // key mode
    {.num = 5}, // key time
    {.num = 6}, // key speed
    {.num = 8}, // key up
    {.num = 9}, // key down
};

/*
  Read values sensed at all available touch pads.
  Use 2 / 3 of read value as the threshold
  to trigger interrupt when the pad is touched.
  Note: this routine demonstrates a simple way
  to configure activation threshold for the touch pads.
  Do not touch any pads when this routine
  is running (on application start).
 */
static void tp_example_set_thresholds(void)
{
    uint16_t touch_value;
    for (int i = 0; i < TOUCHPAD_COUNT; i++) {
        //read filtered value
        touch_pad_read_filtered(s_pad_obj[i].num, &touch_value);
        s_pad_obj[i].init_val = touch_value;
        ESP_LOGI(TAG, "test init: touch pad [%d] val is %d", s_pad_obj[i].num, touch_value);
        //set interrupt threshold.
        ESP_ERROR_CHECK(touch_pad_set_thresh(s_pad_obj[i].num, touch_value * TOUCH_INTERRUPT_THRESH_PERCENT / 100));
    }
}

/*
  Check if any of touch pads has been activated
  by reading a table updated by rtc_intr()
  If so, then print it out on a serial monitor.
  Clear related entry in the table afterwards

  In interrupt mode, the table is updated in touch ISR.

  In filter mode, we will compare the current filtered value with the initial one.
  If the current filtered value is less than 80% of the initial value, we can
  regard it as a 'touched' event.
  When calling touch_pad_init, a timer will be started to run the filter.
  This mode is designed for the situation that the pad is covered
  by a 2-or-3-mm-thick medium, usually glass or plastic.
  The difference caused by a 'touch' action could be very small, but we can still use
  filter mode to detect a 'touch' event.
 */

static void tp_example_read_task(void *pvParameter)
{
    static int show_message;

    while (1) {

        //interrupt mode, enable touch interrupt
        touch_pad_intr_enable();
        for (int i = 0; i < TOUCHPAD_COUNT; i++) {
            if (s_pad_obj[i].activated == true) {
                if(s_pad_obj[i].activated_time){
                    uint32_t interval = esp_log_timestamp() - s_pad_obj[i].activated_time;
                    ESP_LOGI(TAG, "T%d activating continuous %ld ms", s_pad_obj[i].num, interval);
                }else{
                    s_pad_obj[i].activated_time = esp_log_timestamp();
                    ESP_LOGI(TAG, "T%d activated %ld ms!", s_pad_obj[i].num, s_pad_obj[i].activated_time);
                }
                // Clear information on pad activation
                s_pad_obj[i].activated = false;
                // Wait a while for the pad being released
                vTaskDelay(190 / portTICK_PERIOD_MS);
                // timer check released
                // esp_timer_start_once(s_pad_obj[i].timer, 200 * 1000);
                // Reset the counter triggering a message
                // that application is running
                show_message = 1;
            } else {
                if(s_pad_obj[i].activated_time){
                    uint32_t total = esp_log_timestamp() - s_pad_obj[i].activated_time - 200;
                    ESP_LOGI(TAG, "T%d total on activated %ld ms", s_pad_obj[i].num, total);
                    s_pad_cfg.press_cb(s_pad_obj[i].num, total);
                    s_pad_obj[i].activated_time = 0;
                }
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);

        // If no pad is touched, every couple of seconds, show a message
        // that application is running
        if (show_message++ % 500 == 0) {
            ESP_LOGD(TAG, "Waiting for any pad being touched...");
        }
    }
}

/*
  Handle an interrupt triggered when a pad is touched.
  Recognize what pad has been touched and save it in a table.
 */
static void tp_example_rtc_intr(void *arg)
{
    uint32_t pad_intr = touch_pad_get_status();
    //clear interrupt
    touch_pad_clear_status();
    for (int i = 0; i < TOUCHPAD_COUNT; i++) {
        if ((pad_intr >> i) & 0x01) {
            s_pad_obj[i].activated = true;
        }
    }
}

/*
 * Before reading touch pad, we need to initialize the RTC IO.
 */
static void tp_example_touch_pad_init(void)
{
    for (int i = 0; i < TOUCHPAD_COUNT; i++) {
        //init RTC IO and mode for touch pad.
        touch_pad_config(s_pad_obj[i].num, TOUCH_THRESH_NO_USE);
    }
}

// static void pad_timer_callback(void* arg)
// {
//     s_pad_obj_t *p_pad_obj = (s_pad_obj_t*) arg;
//     ESP_LOGI(TAG, "T%d timer called", p_pad_obj->num);
//     esp_timer_handle_t timer_handle = p_pad_obj->timer;
//     /* To start the timer which is running, need to stop it first */
//     ESP_ERROR_CHECK(esp_timer_stop(timer_handle));

// }

void app_touch_pad_init(app_touch_pad_cfg_t *pCfg)
{
    // check and save user cfg
    if(!pCfg){
        ESP_LOGE(TAG, "init fail, null config !!!");
        return;
    }
    if(!pCfg->press_cb){
        ESP_LOGE(TAG, "init fail, null press_cb !!!");
        return;
    }
    memcpy(&s_pad_cfg, pCfg, sizeof(app_touch_pad_cfg_t));
    // Initialize touch pad peripheral, it will start a timer to run a filter
    ESP_LOGI(TAG, "Initializing touch pad");
    ESP_ERROR_CHECK(touch_pad_init());
    // If use interrupt trigger mode, should set touch sensor FSM mode at 'TOUCH_FSM_MODE_TIMER'.
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    // Set reference voltage for charging/discharging
    // For most usage scenarios, we recommend using the following combination:
    // the high reference valtage will be 2.7V - 1V = 1.7V, The low reference voltage will be 0.5V.
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    // Init touch pad IO
    tp_example_touch_pad_init();
    // Initialize and start a software filter to detect slight change of capacitance.
    touch_pad_filter_start(TOUCHPAD_FILTER_TOUCH_PERIOD);
    // Set thresh hold
    tp_example_set_thresholds();
    // Register touch interrupt ISR
    touch_pad_isr_register(tp_example_rtc_intr, NULL);
    // Register touch timer
    // esp_timer_create_args_t pad_timer_args = {
    //         .callback = &pad_timer_callback,
    // };
    // for (int i = 0; i < TOUCHPAD_COUNT; i++) {
    //     ESP_LOGI(TAG, "Initializing T%d obj", s_pad_obj[i].num);
    //     pad_timer_args.arg = (void*) &s_pad_obj[i];
    //     ESP_ERROR_CHECK(esp_timer_create(&pad_timer_args, &s_pad_obj[i].timer));
    // }
    // Start a task to show what pads have been touched
    xTaskCreate(&tp_example_read_task, "touch_pad_read_task", 4096, NULL, 5, NULL);
}
