/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include <stdint.h>
#include <stdbool.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include "lcd.h"

#define TEMPERATURE_REPORTING_PERIOD    15 /* Seconds */

#define MODE_CNT 5
#define SCENE_CNT 3

typedef struct {
    bool display;
    float local_temp;
    float setpoint_temp;
    int setpoint_blink_cnt;
    TimerHandle_t setpoint_blink_timer;
} thermostat_params_t;

extern thermostat_params_t g_thermostat_params;

extern const char *mode_list[MODE_CNT];
extern const char *scene_list[SCENE_CNT];

extern esp_rmaker_device_t *thermostat_device;

void app_driver_init(void);

esp_err_t app_thermostat_set_setpoint_temperature(float setpoint_temp);
float app_thermostat_get_current_temperature_and_time(void);
void app_thermostat_lcd_update(bool beep);

const char* get_workmode_str(LCD_WORK_MODE_E eWorkMode);
const char *get_scene_str(LCD_SCENE_E eScene);
