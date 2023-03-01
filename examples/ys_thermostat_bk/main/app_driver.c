/* Fan demo implementation using button and RGB LED

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "sys/time.h"

#include <string.h>
#include <sdkconfig.h>

#include <iot_button.h>
#include <esp_log.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h> 
#include <esp_rmaker_standard_params.h> 

#include <app_reset.h>
#if USE_CHIP_TEMPERATURE_SENSOR
#include "app_chip_temperature.h"
#else
#include "app_adc_temperature.h"
#endif
#include "app_touch_pad.h"
#include "app_priv.h"

/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          CONFIG_EXAMPLE_BOARD_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  0

#define WIFI_RESET_BUTTON_TIMEOUT       3
#define FACTORY_RESET_BUTTON_TIMEOUT    6

#define TEMPERATURE_SETPOINT_MAX        40
#define TEMPERATURE_SETPOINT_MIN        0

static const char *TAG = "app_driver";

const char *mode_list[MODE_CNT] = {"Auto","Cold","Heat","Wind","Dry"};
const char *scene_list[SCENE_CNT] = {"None", "Outdoor", "Sleep"};

thermostat_params_t g_thermostat_params = {
    .display = true,
    .setpoint_blink_cnt = 0,
};

static TimerHandle_t beep_timer;

const char *get_scene_str(LCD_SCENE_E eScene)
{
    return scene_list[(int)eScene];
}

const char *get_workmode_str(LCD_WORK_MODE_E eWorkMode)
{
    return mode_list[(int)eWorkMode];
}

void lcd_beep_once(void)
{
    lcd_beep(1);

    if(xTimerStart(beep_timer, 10) != pdTRUE) {
        ESP_LOGE(TAG, "start beep timer error");
    }
}

esp_err_t app_thermostat_set_setpoint_temperature(float setpoint_temp)
{
    ESP_LOGI(TAG, "set setpoint temperature: %f", setpoint_temp);

    g_thermostat_params.setpoint_temp = setpoint_temp;

    if(g_thermostat_params.setpoint_blink_timer) {
        ESP_LOGW(TAG, "blink timer start");
        g_thermostat_params.setpoint_blink_cnt = 10;
        if(xTimerStart(g_thermostat_params.setpoint_blink_timer, 0) != pdTRUE) {
            ESP_LOGE(TAG, "start setpoint blink timer error");
        }
    }

    return ESP_OK;
}

void get_current_time_second(struct tm *ptimeinfo) 
{
    time_t now;
    time(&now);
    localtime_r(&now, ptimeinfo);
    ptimeinfo->tm_year += 1900;
    ptimeinfo->tm_mon += 1;
    ESP_LOGI(TAG, "TIME[%d-%02d-%02d(%d) %02d:%02d:%02d]",  ptimeinfo->tm_year, ptimeinfo->tm_mon, ptimeinfo->tm_mday, 
                                                            ptimeinfo->tm_wday, ptimeinfo->tm_hour, ptimeinfo->tm_min, ptimeinfo->tm_sec);
    // char strftime_buf[64] = {0};
    // strftime(strftime_buf, sizeof(strftime_buf), "%c %z[%Z]", ptimeinfo);
    // ESP_LOGI(TAG, "[%s]", strftime_buf);
}

float app_thermostat_get_current_temperature_and_time(void)
{
    lcd_time_t lcd_time = {0};
    struct tm timeinfo;

    get_current_time_second(&timeinfo);

    lcd_time.hour = timeinfo.tm_hour;
    lcd_time.min = timeinfo.tm_min;
    lcd_time.wday = timeinfo.tm_wday;
    lcd_set_time(&lcd_time);

#if USE_CHIP_TEMPERATURE_SENSOR
    float local_temp = app_chip_temperature_read();
#else
    float local_temp = app_adc_temperature_read();
#endif
    g_thermostat_params.local_temp = local_temp;

    return local_temp;
}

void app_thermostat_lcd_update(bool beep)
{
    if (g_thermostat_params.display) {
        if(g_thermostat_params.setpoint_blink_cnt) {
            if(g_thermostat_params.setpoint_blink_cnt % 2) {
                lcd_clear_temperature();
            } else {
                lcd_set_temperature(g_thermostat_params.setpoint_temp);
            }
        } else {
            lcd_set_temperature(g_thermostat_params.local_temp);
        }
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

static void beep_callback( TimerHandle_t pxTimer )
{
    lcd_beep(0);

    // // Optionally do something if the pxTimer parameter is NULL.
    // configASSERT( pxTimer );

    // xTimerStop( pxTimer, 0 );
}

static void setpoint_blink_callback( TimerHandle_t pxTimer )
{
    bool beep = 0;

    g_thermostat_params.setpoint_blink_cnt--;
    ESP_LOGD(TAG, "blink timer remain %d count", g_thermostat_params.setpoint_blink_cnt);
    
    // Optionally do something if the pxTimer parameter is NULL.
    configASSERT( pxTimer );

    // If the timer has expired 10 times then stop it from running.
    if(!g_thermostat_params.setpoint_blink_cnt) {
        // Do not use a block time if calling a timer API function from a
        // timer callback function, as doing so could cause a deadlock!
        ESP_LOGD(TAG, "blink timer stop");
        xTimerStop( pxTimer, 0 );
        esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_name(thermostat_device, ESP_RMAKER_DEF_SETPOINT_TEMPERATURE_NAME),
                esp_rmaker_float(g_thermostat_params.setpoint_temp));
        beep = 1;
    }

    app_thermostat_lcd_update(beep);
}

esp_err_t app_thermostat_init(void)
{
    /** hw init **/

    // init adc for temperature sensor
#if USE_CHIP_TEMPERATURE_SENSOR
    app_chip_temperature_init();
#else
    app_adc_temperature_init();
#endif

    // create beep timer
    beep_timer = xTimerCreate("beep_tm", (100) / portTICK_PERIOD_MS,
                            pdFALSE, NULL, beep_callback);

    // init lcd display
    lcd_init();

    // create setpoint blink timer
    g_thermostat_params.setpoint_blink_timer = xTimerCreate("setpoint_blink_tm", (500) / portTICK_PERIOD_MS,
                            pdTRUE, NULL, setpoint_blink_callback);

    return ESP_OK;
}

static void push_btn_cb(void *arg)
{
    char *str = (char*)arg;
    ESP_LOGI(TAG, "tap %s", str);
}

void button_init(void)
{
    button_handle_t boot_key_handle = iot_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
    if (boot_key_handle) {
        /* Register a callback for a button tap (short press) event */
        iot_button_set_evt_cb(boot_key_handle, BUTTON_CB_TAP, push_btn_cb, "boot");
        /* Register Wi-Fi reset and factory reset functionality on same button */
        app_reset_button_register(boot_key_handle, WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
    }
}

static void tp_press_long_3s_handle(uint8_t num)
{
    switch(num){
        case 0: // key power
            break;
        case 4: // key mode
            break;
        case 5: // key time
            break;
        case 6: // key speed
            break;
        case 8: // key up
            break;
        case 9: // key down
            break;
        default:
            break;
    }
}

static void tp_press_once_handle(uint8_t num)
{
    esp_rmaker_param_t *param = NULL;
    esp_rmaker_param_val_t *val = NULL;
    esp_rmaker_param_val_t new_val = {0};
    bool bOk = false;
    
    switch(num){
        case 0: { // key power
            param = esp_rmaker_device_get_param_by_name(thermostat_device, ESP_RMAKER_DEF_POWER_NAME);
            val = esp_rmaker_param_get_val(param);
            new_val = esp_rmaker_bool(val->val.b);

            new_val.val.b = !new_val.val.b;
            if (new_val.val.b) {
                ESP_LOGI(TAG, "power local change to [ON]");
            } else {
                ESP_LOGI(TAG, "power local change to [OFF]");
            }
            g_thermostat_params.display = new_val.val.b;
            bOk = true;
        }
            break;
        case 4: { // key mode
            param = esp_rmaker_device_get_param_by_name(thermostat_device, ESP_RMAKER_DEF_MODE_CONTROL_NAME);
            val = esp_rmaker_param_get_val(param);
            new_val = esp_rmaker_int(val->val.i);

            if(++new_val.val.i >= E_WORK_MODE_MAX) {
                new_val.val.i = 0;
            }
            ESP_LOGI(TAG, "workmode local change to [%s]", get_workmode_str(new_val.val.i));
            lcd_set_work_mode(new_val.val.i);
            bOk = true;
        }
            break;
        case 5: // key time
            break;
        case 6: { // key speed
            param = esp_rmaker_device_get_param_by_name(thermostat_device, ESP_RMAKER_DEF_SPEED_NAME);
            val = esp_rmaker_param_get_val(param);
            new_val = esp_rmaker_int(val->val.i);

            if(++new_val.val.i >= E_WIND_MODE_MAX) {
                new_val.val.i = 0;
            }
            ESP_LOGI(TAG, "speed local change to [%d]", new_val.val.i);
            lcd_set_wind_speed(new_val.val.i);
            bOk = true;
        }
            break;
        case 8: { // key up
            float will_set_temp = g_thermostat_params.setpoint_temp;
            will_set_temp = (will_set_temp + 0.1f >= TEMPERATURE_SETPOINT_MAX) ? TEMPERATURE_SETPOINT_MAX : will_set_temp + 0.1f;
            app_thermostat_set_setpoint_temperature(will_set_temp);
        }
            break;
        case 9: { // key down
            float will_set_temp = g_thermostat_params.setpoint_temp;
            will_set_temp = (will_set_temp - 0.1f <= TEMPERATURE_SETPOINT_MIN) ? TEMPERATURE_SETPOINT_MIN : will_set_temp - 0.1f;
            app_thermostat_set_setpoint_temperature(will_set_temp);
        }
            break;
        default:
            break;
    }

    if(bOk){
        esp_rmaker_param_update_and_report(param, new_val);
        app_thermostat_lcd_update(1);
    }
}

static const char* get_key_str(uint8_t num)
{
    switch(num){
        case 0: // key power
            return "power";
        case 4: // key mode
            return "mode";
        case 5: // key time
            return "time";
        case 6: // key speed
            return "speed";
        case 8: // key up
            return "up";
        case 9: // key down
            return "down";
        default:
            return "unknown";
    }
}

static void tp_press_handle(uint8_t num, uint32_t press_time)
{
    ESP_LOGI(TAG, "T%d press %ld ms", num, press_time);

    if(press_time <= 600){
        ESP_LOGI(TAG, "T%d-%s once press", num, get_key_str(num));
        tp_press_once_handle(num);
    }else if(press_time >= 3000){
        ESP_LOGI(TAG, "T%d-%s long press", num, get_key_str(num));
        tp_press_long_3s_handle(num);
    }
}

void app_driver_init()
{
    app_thermostat_init();

    button_init();

    app_touch_pad_cfg_t s_tp_cfg = {0};
    s_tp_cfg.press_cb = tp_press_handle;
    app_touch_pad_init(&s_tp_cfg);
}
