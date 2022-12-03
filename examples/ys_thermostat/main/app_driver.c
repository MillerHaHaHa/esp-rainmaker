/* Fan demo implementation using button and RGB LED

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <sdkconfig.h>

#include <iot_button.h>
#include <esp_log.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h> 
#include <esp_rmaker_standard_params.h> 

#include <app_reset.h>
#include "driver/temperature_sensor.h"
#include <ws2812_led.h>
#include "app_priv.h"

/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          CONFIG_EXAMPLE_BOARD_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  0

#define DEFAULT_HUE         180
#define DEFAULT_SATURATION  100
#define DEFAULT_BRIGHTNESS  ( 20 * 3)

#define WIFI_RESET_BUTTON_TIMEOUT       3
#define FACTORY_RESET_BUTTON_TIMEOUT    10

static const char *TAG = "app_driver";

static temperature_sensor_handle_t g_temp_sensor = NULL;

static uint16_t g_hue = DEFAULT_HUE;
static uint16_t g_saturation = DEFAULT_SATURATION;
static uint16_t g_value = DEFAULT_BRIGHTNESS;

const char *direction_list[DIRECTION_CNT] = {"Auto","Low","Medinum","High"};
const char *mode_list[MODE_CNT] = {"Auto","Cold","Heat","Wind","Dry"};
const char *scene_list[SCENE_CNT] = {"None", "Outdoor", "Sleep"};

thermostat_params_t g_thermostat_params = {
    .power_state = true,
    .valve_state = true,
    .alarm_state = false,
    .display_time = true,
    .display_temp = true,
    .locker_state = false,
    .house_state = false,
    .warning_state = false,
    .local_temp = 26.0,
    .setpoint_temp = 16.0,
    .setpoint_blink_cnt = 0,
    .speed = 0,
    .direction = "Auto",
    .work_mode = E_WORK_MODE_AUTO,
    .time = {0},
    .scene = E_SCENE_NONE,
};

static TimerHandle_t beep_timer;

void lcd_beep_once(void)
{
    lcd_beep(1);

    if(xTimerStart(beep_timer, 0) != pdTRUE) {
        ESP_LOGE(TAG, "start beep timer error");
    }
}

char *get_scene_str(LCD_SCENE_E eScene)
{
    return scene_list[(int)eScene];
}

char *get_workmode_str(LCD_WORK_MODE_E eWorkMode)
{
    return mode_list[(int)eWorkMode];
}

float app_temperature_sensor_get_value(void)
{
    if (g_temp_sensor) {
        ESP_LOGI(TAG, "Read temperature");
        float tsens_value;
        ESP_ERROR_CHECK(temperature_sensor_get_celsius(g_temp_sensor, &tsens_value));
        ESP_LOGI(TAG, "Temperature value %.02f ℃", tsens_value);
        return tsens_value;
    } else {
        return 0;
    }
}

temperature_sensor_handle_t app_temperature_sensor_init(void)
{
    temperature_sensor_handle_t temp_sensor = NULL;
    temperature_sensor_config_t temp_sensor_config = TEMPERAUTRE_SENSOR_CONFIG_DEFAULT(10, 50);
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));

    ESP_LOGI(TAG, "Enable temperature sensor");
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));
    return temp_sensor;
}

esp_err_t app_thermostat_set_power_state(bool bOn)
{
    ESP_LOGI(TAG, "set power: %d", bOn);

    g_thermostat_params.power_state = bOn;
    
    // if (power) {
    //     ws2812_led_set_hsv(g_hue, g_saturation, g_value);
    // } else {
    //     ws2812_led_clear();
    // }

    return ESP_OK;
}

esp_err_t app_thermostat_set_setpoint_temperature(float setpoint_temp)
{
    ESP_LOGI(TAG, "set setpoint temperature: %f", setpoint_temp);

    g_thermostat_params.setpoint_temp = setpoint_temp;

    // g_hue = setpoint_temp * 360 / 40;
    // ws2812_led_set_hsv(g_hue, g_saturation, g_value);

    if(g_thermostat_params.setpoint_blink_timer) {
        ESP_LOGW(TAG, "blink timer start");
        g_thermostat_params.display_temp = 0;
        g_thermostat_params.setpoint_blink_cnt = 10;
        if(xTimerStart(g_thermostat_params.setpoint_blink_timer, 0) != pdTRUE) {
            ESP_LOGE(TAG, "start setpoint blink timer error");
        }
    }

    return ESP_OK;
}

esp_err_t app_thermostat_set_speed(uint8_t speed)
{
    ESP_LOGI(TAG, "set speed: %d", speed);

    g_thermostat_params.speed = speed;

    // g_value = 20 * speed;
    // return ws2812_led_set_hsv(g_hue, g_saturation, g_value);

    return ESP_OK;
}

esp_err_t app_thermostat_set_direction(char* direction)
{ 
    ESP_LOGI(TAG, "set direction: %s", direction);

    memset(g_thermostat_params.direction, 0, sizeof(g_thermostat_params.direction));
    strcpy(g_thermostat_params.direction, direction);

    // for(int i = 0; i < DIRECTION_CNT; i++) {
    //     if (strcmp(direction, direction_list[i]) == 0) {
    //         ESP_LOGI(TAG, "set direction to %s", direction_list[i]);
    //         break;
    //     }
    // }

    return ESP_OK;
}

esp_err_t app_thermostat_set_work_mode(char* work_mode)
{
    ESP_LOGI(TAG, "set work mode: %s", work_mode);

    // for(int i = 0; i < MODE_CNT; i++) {
    //     if (strcmp(mode, mode_list[i]) == 0) {
    //         ESP_LOGI(TAG, "set mode to %s", mode_list[i]);
    //         break;
    //     }
    // }

    for(int i = 0; i < MODE_CNT; i++) {
        if(0 == strcmp(work_mode, mode_list[i])) {
            ESP_LOGI(TAG, "set work mode: %d", i);
            g_thermostat_params.work_mode = i;
            break;
        }
    }

    return ESP_OK;
}

float app_thermostat_get_current_temperature(void)
{
    g_thermostat_params.local_temp = app_temperature_sensor_get_value();

    return g_thermostat_params.local_temp;
}

void app_thermostat_lcd_update(bool beep)
{
	if (g_thermostat_params.power_state) {
        if(g_thermostat_params.setpoint_blink_cnt) {
            if(g_thermostat_params.display_temp) {
                lcd_set_temperature(g_thermostat_params.setpoint_temp);
            } else {
                lcd_clear_temperature();
            }
        } else {
            lcd_set_temperature(g_thermostat_params.local_temp);
        }
		lcd_set_wind_speed(g_thermostat_params.speed);
		lcd_set_valve_state(g_thermostat_params.valve_state);
		if(g_thermostat_params.display_time) {
			lcd_set_time(&g_thermostat_params.time);
		} else {
			lcd_clear_time();
		}
		lcd_set_work_mode(g_thermostat_params.work_mode);
		lcd_set_alarm_state(g_thermostat_params.alarm_state);
		lcd_set_locker_state(g_thermostat_params.locker_state);
		lcd_set_house_state(g_thermostat_params.house_state);
		lcd_set_scene(g_thermostat_params.scene);
		lcd_set_warning_state(g_thermostat_params.warning_state);
		lcd_update();
        ESP_LOGD(TAG, "lcd update");
	} else {
		lcd_clear();
        ESP_LOGD(TAG, "lcd clear");
	}
    if(beep) {
        lcd_beep_once();
    }
}

esp_err_t app_thermostat_set_scene(char* scene)
{
    ESP_LOGI(TAG, "set scene: %s", scene);

    for(int i = 0; i < MODE_CNT; i++) {
        if(0 == strcmp(scene, scene_list[i])) {
            ESP_LOGI(TAG, "set scene: %d", i);
            g_thermostat_params.scene = i;
            break;
        }
    }

    return ESP_OK;
}

esp_err_t app_thermostat_set_warning_state(bool bOn)
{
    ESP_LOGI(TAG, "set warning state: %d", bOn);

    g_thermostat_params.warning_state = bOn;

    return ESP_OK;
}

esp_err_t app_thermostat_set_display_time(bool bOn)
{
    ESP_LOGI(TAG, "set display time: %d", bOn);

    g_thermostat_params.display_time = bOn;

    return ESP_OK;
}

esp_err_t app_thermostat_set_alarm_state(bool bOn)
{
    ESP_LOGI(TAG, "set alarm state: %d", bOn);

    g_thermostat_params.alarm_state = bOn;

    return ESP_OK;
}

esp_err_t app_thermostat_set_house_state(bool bOn)
{
    ESP_LOGI(TAG, "set house state: %d", bOn);

    g_thermostat_params.house_state = bOn;

    return ESP_OK;
}

esp_err_t app_thermostat_set_locker_state(bool bOn)
{
    ESP_LOGI(TAG, "set locker state: %d", bOn);

    g_thermostat_params.locker_state = bOn;

    return ESP_OK;
}

esp_err_t app_thermostat_set_valve_state(bool bOn)
{
    ESP_LOGI(TAG, "set valve state: %d", bOn);

    g_thermostat_params.valve_state = bOn;

    return ESP_OK;
}

esp_err_t app_thermostat_restore_params(thermostat_params_t *pParams)
{
    // restore param


    // set params to hw
    app_thermostat_set_power_state(g_thermostat_params.power_state);
    // app_thermostat_set_locker_state(g_thermostat_params.locker_state);
    // app_thermostat_set_house_state(g_thermostat_params.house_state);
    // app_thermostat_set_alarm_state(g_thermostat_params.alarm_state);
    // app_thermostat_set_warning_state(g_thermostat_params.warning_state);
    app_thermostat_set_setpoint_temperature(g_thermostat_params.setpoint_temp);
    // app_thermostat_set_display_time(g_thermostat_params.display_time);
    app_thermostat_set_speed(g_thermostat_params.speed);
    char default_direction[10] = {0};
    strcpy(default_direction, g_thermostat_params.direction);
    app_thermostat_set_direction(default_direction);
    app_thermostat_set_work_mode(get_workmode_str(g_thermostat_params.work_mode));
    app_thermostat_set_valve_state(g_thermostat_params.valve_state);
    // app_thermostat_set_scene(get_scene_str(g_thermostat_params.scene));
    app_thermostat_get_current_temperature();

    return ESP_OK;
}

static void beep_callback( TimerHandle_t pxTimer )
{
    lcd_beep(0);

    // // Optionally do something if the pxTimer parameter is NULL.
    // configASSERT( pxTimer );

    // xTimerStop( pxTimer, 0 );
}

static void setpoint_blink_callback( TimerHandle_t pxTimer )
{
    g_thermostat_params.setpoint_blink_cnt--;
    ESP_LOGW(TAG, "blink timer remain %d count", g_thermostat_params.setpoint_blink_cnt);
    
    // Optionally do something if the pxTimer parameter is NULL.
    configASSERT( pxTimer );

    // If the timer has expired 10 times then stop it from running.
    if(!g_thermostat_params.setpoint_blink_cnt) {
        // Do not use a block time if calling a timer API function from a
        // timer callback function, as doing so could cause a deadlock!
        ESP_LOGW(TAG, "blink timer stop");
        g_thermostat_params.display_temp = 1;
        xTimerStop( pxTimer, 0 );
    } else {
        g_thermostat_params.display_temp = !g_thermostat_params.display_temp;
        ESP_LOGW(TAG, "blink timer display %d", g_thermostat_params.display_temp);
    }

    app_thermostat_lcd_update(0);
}

esp_err_t app_thermostat_init(void)
{
    // hw init
    // esp_err_t err = ws2812_led_init();
    // if (err != ESP_OK) {
    //     return err;
    // }

    lcd_init();

    g_temp_sensor = app_temperature_sensor_init();
    if (!g_temp_sensor) {
        ESP_LOGE(TAG, "init temperature sensor error !!!");
        return -1;
    }

    // restore saved params
    app_thermostat_restore_params(&g_thermostat_params);

    // create beep timer
    beep_timer = xTimerCreate("beep_tm", (100) / portTICK_PERIOD_MS,
                            pdFALSE, NULL, beep_callback);

    // create setpoint blink timer
    g_thermostat_params.setpoint_blink_timer = xTimerCreate("setpoint_blink_tm", (500) / portTICK_PERIOD_MS,
                            pdTRUE, NULL, setpoint_blink_callback);

    // update lcd
    app_thermostat_lcd_update(1);

    return ESP_OK;
}

static void push_btn_cb(void *arg)
{
    char *str = (char*)arg;
    ESP_LOGI(TAG, "tap %s", str);

    if(0 == strcmp(str, "power")) {
        app_thermostat_set_power_state(!g_thermostat_params.power_state);
        app_thermostat_lcd_update(1);
    }
}

void button_init(void)
{
#define K1_POWER_GPIO       BUTTON_GPIO
// #define K2_M_GPIO           BUTTON_GPIO
// #define K3_TIME_GPIO        BUTTON_GPIO
// #define K4_WIND_GPIO        BUTTON_GPIO
// #define K5_UP_GPIO          BUTTON_GPIO
// #define K6_DOWN_GPIO        BUTTON_GPIO

    button_handle_t k1_power_handle = iot_button_create(K1_POWER_GPIO, BUTTON_ACTIVE_LEVEL);
    if (k1_power_handle) {
        /* Register a callback for a button tap (short press) event */
        iot_button_set_evt_cb(k1_power_handle, BUTTON_CB_TAP, push_btn_cb, "power");
        /* Register Wi-Fi reset and factory reset functionality on same button */
        app_reset_button_register(k1_power_handle, WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
    }

#if 0
    button_handle_t k2_m_handle = iot_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
    if (k2_m_handle) {
        /* Register a callback for a button tap (short press) event */
        iot_button_set_evt_cb(k2_m_handle, BUTTON_CB_TAP, push_btn_cb, NULL);
        /* Register Wi-Fi reset and factory reset functionality on same button */

    }

    button_handle_t k3_time_handle = iot_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
    if (k3_time_handle) {
        /* Register a callback for a button tap (short press) event */
        iot_button_set_evt_cb(k3_time_handle, BUTTON_CB_TAP, push_btn_cb, NULL);
        /* Register Wi-Fi reset and factory reset functionality on same button */

    }

    button_handle_t k4_wind_handle = iot_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
    if (k4_wind_handle) {
        /* Register a callback for a button tap (short press) event */
        iot_button_set_evt_cb(k4_wind_handle, BUTTON_CB_TAP, push_btn_cb, NULL);
        /* Register Wi-Fi reset and factory reset functionality on same button */

    }

    button_handle_t k5_up_handle = iot_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
    if (k5_up_handle) {
        /* Register a callback for a button tap (short press) event */
        iot_button_set_evt_cb(k5_up_handle, BUTTON_CB_TAP, push_btn_cb, NULL);
        /* Register Wi-Fi reset and factory reset functionality on same button */
    }

    button_handle_t k6_down_handle = iot_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
    if (k6_down_handle) {
        /* Register a callback for a button tap (short press) event */
        iot_button_set_evt_cb(k6_down_handle, BUTTON_CB_TAP, push_btn_cb, NULL);
        /* Register Wi-Fi reset and factory reset functionality on same button */
    }
#endif
}

void app_driver_init()
{
    app_thermostat_init();

    button_init();
}
