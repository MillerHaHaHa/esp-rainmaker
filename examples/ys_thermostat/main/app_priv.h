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

#define TEMPERATURE_REPORTING_PERIOD    60 /* Seconds */

#define DIRECTION_CNT 4
#define MODE_CNT 5
#define SCENE_CNT 3

typedef struct {
    bool power_state;
    bool valve_state;
    bool alarm_state;
    bool display_time;
    bool display_temp;
    bool locker_state;
    bool house_state;
    bool warning_state;
    float local_temp;
    float setpoint_temp;
    int setpoint_blink_cnt;
    TimerHandle_t setpoint_blink_timer;
    uint8_t speed;
    char direction[10];
    LCD_WORK_MODE_E work_mode;
    lcd_time_t time;
    LCD_SCENE_E scene;
} thermostat_params_t;

extern thermostat_params_t g_thermostat_params;

extern const char *direction_list[DIRECTION_CNT];
extern const char *mode_list[MODE_CNT];

extern esp_rmaker_device_t *thermostat_device;

void app_driver_init(void);

esp_err_t app_thermostat_set_power_state(bool bOn);
esp_err_t app_thermostat_set_locker_state(bool bOn);
esp_err_t app_thermostat_set_house_state(bool bOn);
esp_err_t app_thermostat_set_alarm_state(bool bOn);
esp_err_t app_thermostat_set_warning_state(bool bOn);
esp_err_t app_thermostat_set_setpoint_temperature(float setpoint_temp);
esp_err_t app_thermostat_set_display_time(bool bOn);
esp_err_t app_thermostat_set_speed(uint8_t speed);
esp_err_t app_thermostat_set_direction(char* direction);
esp_err_t app_thermostat_set_work_mode(char* work_mode);
esp_err_t app_thermostat_set_valve_state(bool bOn);
esp_err_t app_thermostat_set_scene(char* scene);
float app_thermostat_get_current_temperature(void);
void app_thermostat_lcd_update(bool beep);

char* get_workmode_str(LCD_WORK_MODE_E eWorkMode);
char *get_scene_str(LCD_SCENE_E eScene);
