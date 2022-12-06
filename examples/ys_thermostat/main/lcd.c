/**
 * @file lcd.c
 * @author miller.huang
 * @brief 
 * @version 0.1
 * @date 2022-11-24
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include <esp_log.h>

#include "ht1621.h"
#include "lcd.h"

static const char *TAG = "lcd";

// function of set ht1621 buffer 
static uint8_t number_to_data(char num)
{
	switch(num)
	{
		case '0':
			return 0xFA;
		case '1':
			return 0x60;
		case '2':
			return 0xD6;
		case '3':
			return 0xF4;
		case '4':
			return 0x6C;
		case '5':
			return 0xBC;
		case '6':
			return 0xBE;
		case '7':
			return 0xE0;
		case '8':
			return 0xFE;
		case '9':
			return 0xFC;
		default:
			return 0;
	}
}

void lcd_clear_time(void)
{
	uint8_t data5 = 0, data15 = 0;

	data5 = ht1621_get_buffer(5) & 0x0E;
	ht1621_set_buffer(5, data5);
	ht1621_set_buffer(6, 0);
	ht1621_set_buffer(7, 0);
	ht1621_set_buffer(8, 0);
	ht1621_set_buffer(14, 0);
	data15 = ht1621_get_buffer(15) & 0xF0;
	ht1621_set_buffer(15, data15);
}

void lcd_set_time(lcd_time_t *ptime)
{
	uint8_t data5 = 0, data6 = 0, data7 = 0, data8 = 0, data14 = 0, data15 = 0;

	// clock logo
	data6 = ht1621_get_buffer(6) | 0x02;
	ht1621_set_buffer(6, data6);

	// minute
	char min_str[4] = {0};
	snprintf(min_str, 4, "%02u", ptime->min);
	ESP_LOGD(TAG, "min_str: %s", min_str);

	data7 = 0x01 | number_to_data(min_str[0]);
	ht1621_set_buffer(7, data7);

	data8 = 0x01 | number_to_data(min_str[1]);
	ht1621_set_buffer(8, data8);

	// hour
	char hour_str[4] = {0};
	snprintf(hour_str, 4, "%02u", ptime->hour);
	ESP_LOGD(TAG, "hour_str: %s", hour_str);

	uint8_t hour_dataH = number_to_data(hour_str[0]);
	uint8_t hour_dataL = number_to_data(hour_str[1]);

	data6 = ht1621_get_buffer(6) & 0x0F;
	data6 |= (hour_dataL & 0xF0);
	ht1621_set_buffer(6, data6);

	data14 = (hour_dataL & 0x0E) | (hour_dataH & 0xF0);
	ht1621_set_buffer(14, data14);

	data15 = ht1621_get_buffer(15) & 0xF1;
	data15 |= (hour_dataH & 0x0E);
	ht1621_set_buffer(15, data15);

	// wday
	data5 = ht1621_get_buffer(5) & 0x0E;
	data6 = ht1621_get_buffer(6) & 0xF3;

	if(ptime->wday >= 6) {
		// Sat, Sun
		if(ptime->wday == 6) {
			data6 |= 0x08;
		} else {
			data6 |= 0x04;
		}
	} else {
		if(ptime->wday == 1) {
			data5 |= 0x01;
		} else if(ptime->wday == 2) {
			data5 |= 0x10;
		} else if(ptime->wday == 3) {
			data5 |= 0x20;
		} else if(ptime->wday == 4) {
			data5 |= 0x40;
		} else if(ptime->wday == 5) {
			data5 |= 0x80;
		}
	}
	ht1621_set_buffer(5, data5);
	ht1621_set_buffer(6, data6);
}

void lcd_set_warning_state(bool bOn)
{
	uint8_t data = 0;

	data = ht1621_get_buffer(1) & 0xFE;
	if(bOn) {
		data |= 0x01;
	}

	ht1621_set_buffer(1, data);
}

void lcd_set_locker_state(bool bOn)
{
	uint8_t data = 0;

	data = ht1621_get_buffer(15) & 0xEF;
	if(bOn) {
		data |= 0x10;
	}

	ht1621_set_buffer(15, data);
}

void lcd_set_house_state(bool bOn)
{
	uint8_t data = 0;

	data = ht1621_get_buffer(5) & 0xF9;
	if(bOn) {
		data |= 0x06;
	}

	ht1621_set_buffer(5, data);
}

void lcd_set_alarm_state(bool bOn)
{
	uint8_t data = 0;

	data = ht1621_get_buffer(5) & 0xF7;
	if(bOn) {
		data |= 0x08;
	}

	ht1621_set_buffer(5, data);
}

void lcd_set_work_mode(LCD_WORK_MODE_E eMode)
{
	uint8_t data1 = 0, data15 = 0, data16 = 0;

	data1 = ht1621_get_buffer(1) & 0xDD;
	data15 = ht1621_get_buffer(15) & 0xDF;
	data16 = ht1621_get_buffer(16) & 0xED;

	switch (eMode) {
		case E_WORK_MODE_AUTO:
			data1 |= 0x02;
			break;
		case E_WORK_MODE_COLD:
			data16 |= 0x10;
			break;
		case E_WORK_MODE_HEAT:
			data16 |= 0x02;
			break;
		case E_WORK_MODE_WIND:
			data15 |= 0x20;
			break;
		case E_WORK_MODE_DRY:
			data1 |= 0x20;
			break;
		default:
			break;
	}

	ht1621_set_buffer(1, data1);
	ht1621_set_buffer(15, data15);
	ht1621_set_buffer(16, data16);
}

void lcd_set_scene(LCD_SCENE_E eScene)
{
	uint8_t data2 = 0, data3 = 0;

	data2 = ht1621_get_buffer(2) & 0xFE;
	data3 = ht1621_get_buffer(3) & 0xFE;

	switch (eScene)
	{
		case E_SCENE_OUTDOOR:
			data2 |= 0x01;
			break;
		
		case E_SCENE_SLEEP:
			data3 |= 0x01;
			break;

		default:
			break;
	}

	ht1621_set_buffer(2, data2);
	ht1621_set_buffer(3, data3);
}

void lcd_set_valve_state(bool bOn)
{
	uint8_t data = 0;

	if(bOn) {
		data = ht1621_get_buffer(1) | 0x10;
	} else {
		data = ht1621_get_buffer(1) & 0xEF;
	}

	ht1621_set_buffer(1, data);
}

void lcd_set_wind_speed(LCD_WIND_SPEED_E speed)
{
	uint8_t data15 = 0, data16 = 0;
	
	data15 = ht1621_get_buffer(15);
	data16 = ht1621_get_buffer(16);

	if(speed == E_WIND_MODE_HIGH) {
		data15 |= 0x40;
		data16 &= 0x7F;
		data16 |= 0x44;
	} else {
		data15 &= 0xBF;
		data16 &= 0x3B;

		if(speed == E_WIND_MODE_AUTO) {
			data16 |= 0x80;
		} else if(speed == E_WIND_MODE_MID) {
			data16 |= 0x44;
		} else if(speed == E_WIND_MODE_LOW) {
			data16 |= 0x04;
		}
	}

	ht1621_set_buffer(15, data15);
	ht1621_set_buffer(16, data16);
}

void lcd_clear_temperature(void)
{
	ht1621_set_buffer(2, ht1621_get_buffer(2) & 0x01);
	ht1621_set_buffer(3, ht1621_get_buffer(3) & 0x01);
	ht1621_set_buffer(4, 0);
}

void lcd_set_temperature(float temp)
{
	if(temp >= 100 || temp <= -100) {
		ESP_LOGE(TAG, "error tempature param: %f", temp);
	}

	int16_t ingegerpart = 0;
	ingegerpart = ((int16_t)(temp * 10));

	ESP_LOGD(TAG, "ingegerpart: %d", ingegerpart);

	char temp_str[7] = {0};
	snprintf(temp_str, 7, "%03d", ingegerpart);
	ESP_LOGD(TAG, "temp_str: %s", temp_str);

	uint8_t addr = 0;
	uint8_t data = 0;

	addr = 2;
	data = ht1621_get_buffer(addr) & 0x01;
	data |= number_to_data(temp_str[0]);
	ht1621_set_buffer(addr, data);

	addr = 3;
	data = ht1621_get_buffer(addr) & 0x01;
	data |= number_to_data(temp_str[1]);
	ht1621_set_buffer(addr, data);

	addr = 4;
	data = ht1621_get_buffer(addr) & 0x01;
	data |= number_to_data(temp_str[2]);
	ht1621_set_buffer(addr, data);
}

static void lcd_set_default_display(void)
{
	uint8_t data = 0;
	
#if 1
	// addr 1, default set ℃
	data = ht1621_get_buffer(1) | 0x08;
	ht1621_set_buffer(1, data);
#else
	addr 1, default set ℉
	data = ht1621_get_buffer(1) | 0x04;
	ht1621_set_buffer(addr, data);
#endif

	// addr 4, default set tempature decimal point
	data = ht1621_get_buffer(4) | 0x01;
	ht1621_set_buffer(4, data);

	// addr 16, default set wind logo
	data = ht1621_get_buffer(16) | 0x20;
	ht1621_set_buffer(16, data);
}

void lcd_beep(bool bOn)
{
	ht1621_beep(bOn);
}

void lcd_update(void)
{
	ht1621_update();
}

void lcd_clear(void)
{
	ht1621_clear();
}

int lcd_init(void)
{
	ht1621_begin_noBacklight(CONFIG_HT1621_PIN_CS, CONFIG_HT1621_PIN_WR, CONFIG_HT1621_PIN_DATA);
	// ht1621_begin(CONFIG_HT1621_PIN_CS, CONFIG_HT1621_PIN_WR, CONFIG_HT1621_PIN_DATA, CONFIG_HT1621_PIN_BACKLIGHT);

	lcd_clear();
	lcd_set_default_display();

	return 0;
}
