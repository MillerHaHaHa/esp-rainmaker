/**
 * @file lcd.h
 * @author miller.huang
 * @brief 
 * @version 0.1
 * @date 2022-11-24
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _LCD_H_
#define _LCD_H_   //防止重复包含

#include <stdbool.h>

typedef struct {
	char wday;
	char hour;
	char min;
} lcd_time_t;

typedef enum {
	E_WIND_MODE_AUTO = 0,
	E_WIND_MODE_LOW,
	E_WIND_MODE_MID,
	E_WIND_MODE_HIGH,
} LCD_WIND_SPEED_E;

typedef enum {
    E_WORK_MODE_AUTO = 0,
    E_WORK_MODE_COLD,
    E_WORK_MODE_HEAT,
    E_WORK_MODE_WIND,
    E_WORK_MODE_DRY,
} LCD_WORK_MODE_E;

typedef enum {
    E_SCENE_NONE,
	E_SCENE_OUTDOOR,
    E_SCENE_SLEEP,
} LCD_SCENE_E;

int lcd_init(void);

void lcd_set_temperature(float temp);
void lcd_clear_temperature(void);
void lcd_set_time(lcd_time_t *ptime);
void lcd_clear_time(void);
void lcd_set_wind_speed(LCD_WIND_SPEED_E speed);
void lcd_set_work_mode(LCD_WORK_MODE_E eMode);
void lcd_set_valve_state(bool bOn);
void lcd_set_power_state(bool bOn);
void lcd_set_backlight(bool bOn);
void lcd_set_display_time(bool bOn);
void lcd_set_scene(LCD_SCENE_E eScene);
void lcd_set_alarm_state(bool bOn);
void lcd_set_locker_state(bool bOn);
void lcd_set_house_state(bool bOn);
void lcd_set_warning_state(bool bOn);

void lcd_beep(bool bOn);
void lcd_update(void);
void lcd_clear(void);

#endif // _LCD_H_
