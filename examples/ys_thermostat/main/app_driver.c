/* Fan demo implementation using button and RGB LED

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "sys/time.h"

#include <string.h>
#include <sdkconfig.h>
#include <math.h>

#include <iot_button.h>
#include <esp_log.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h> 
#include <esp_rmaker_standard_params.h> 

#include <app_reset.h>
#include "driver/temperature_sensor.h"
#include <ws2812_led.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "app_priv.h"

/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          CONFIG_EXAMPLE_BOARD_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  0

#define WIFI_RESET_BUTTON_TIMEOUT       3
#define FACTORY_RESET_BUTTON_TIMEOUT    6

static const char *TAG = "app_driver";

#if USE_CHIP_TEMPERATURE_SENSOR
static temperature_sensor_handle_t g_temp_sensor = NULL;
#endif

const char *mode_list[MODE_CNT] = {"Auto","Cold","Heat","Wind","Dry"};
const char *scene_list[SCENE_CNT] = {"None", "Outdoor", "Sleep"};

thermostat_params_t g_thermostat_params = {
    .display = true,
    .setpoint_blink_cnt = 0,
};

static TimerHandle_t beep_timer;

void lcd_beep_once(void)
{
    lcd_beep(1);

    if(xTimerStart(beep_timer, 10) != pdTRUE) {
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

#define ADC1_CHAN   ADC_CHANNEL_2
static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle = NULL;

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool example_adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}

void adc_init(void)
{
    //-------------ADC1 Init---------------//
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC1_CHAN, &config));

    //-------------ADC1 Calibration Init---------------//
    example_adc_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_11, &adc1_cali_handle);
}

int adc_read_voltage_mv(void)
{
    int adc_raw = 0;
    int voltage = 0;

    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_CHAN, &adc_raw));
    ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, ADC1_CHAN, adc_raw);
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage));
    ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, ADC1_CHAN, voltage);

    return voltage;
}

float Vt_to_Rt(int Vt_val, int Vcc_val, int R1_val) {
    float Rt = R1_val * (float)Vt_val / (Vcc_val - (float)Vt_val);
    return Rt;
}

float Rt_to_temp(float Rt_val, int b_val, int Rt_default, int temp_default) {
    float temp = (1 / ((log(Rt_val / Rt_default) / b_val) + 1 / (temp_default + 273.15))) - 273.15 + 0.5;
    return temp;
}

float Vt_to_temp(int input_Vt_mv)
{
    int Vcc = 4900;                     // mV
    int R1 = 4800;                      // Ω
    int b = 4000;                     
    int Rt_default = 4800;              // Ω
    int temp_default = 18;              // ℃

    float Rt = Vt_to_Rt(input_Vt_mv, Vcc, R1);
    printf("output Rt = %f Ω\n", Rt);

    float temp = Rt_to_temp(Rt, b, Rt_default, temp_default);
    printf("output temp = %f ℃\n", temp);

    return temp;
}

#if USE_CHIP_TEMPERATURE_SENSOR
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
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));

    ESP_LOGI(TAG, "Enable temperature sensor");
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));
    return temp_sensor;
}
#endif

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
    float local_temp = app_temperature_sensor_get_value();
#else
    float local_temp = Vt_to_temp(adc_read_voltage_mv());
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
    g_thermostat_params.setpoint_blink_cnt--;
    ESP_LOGW(TAG, "blink timer remain %d count", g_thermostat_params.setpoint_blink_cnt);
    
    // Optionally do something if the pxTimer parameter is NULL.
    configASSERT( pxTimer );

    // If the timer has expired 10 times then stop it from running.
    if(!g_thermostat_params.setpoint_blink_cnt) {
        // Do not use a block time if calling a timer API function from a
        // timer callback function, as doing so could cause a deadlock!
        ESP_LOGW(TAG, "blink timer stop");
        xTimerStop( pxTimer, 0 );
    }

    app_thermostat_lcd_update(0);
}

esp_err_t app_thermostat_init(void)
{
    /** hw init **/

    // init adc for temperature sensor
#if USE_CHIP_TEMPERATURE_SENSOR
    g_temp_sensor = app_temperature_sensor_init();
    if (!g_temp_sensor) {
        ESP_LOGE(TAG, "init temperature sensor error !!!");
        return -1;
    }
#else
    adc_init();
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
    esp_rmaker_param_t *param = NULL;
    esp_rmaker_param_val_t *val = NULL;
    esp_rmaker_param_val_t new_val = {0};
    char *str = (char*)arg;
    ESP_LOGI(TAG, "tap %s", str);

    if(0 == strcmp(str, "power")) {
        param = esp_rmaker_device_get_param_by_name(thermostat_device, ESP_RMAKER_DEF_POWER_NAME);
        val = esp_rmaker_param_get_val(param);
        new_val = esp_rmaker_bool(val->val.b);

        new_val.val.b = !new_val.val.b;
        g_thermostat_params.display = new_val.val.b;
    } else if(0 == strcmp(str, "speed")) {
        param = esp_rmaker_device_get_param_by_name(thermostat_device, ESP_RMAKER_DEF_SPEED_NAME);
        val = esp_rmaker_param_get_val(param);
        new_val = esp_rmaker_int(val->val.i);

        if(new_val.val.i++ > 3) {
            new_val.val.i = 0;
        }
        lcd_set_wind_speed(new_val.val.i); 
    } else if(0 == strcmp(str, "up")) {

    } else if(0 == strcmp(str, "down")) {

    }

    esp_rmaker_param_update_and_report(param, new_val);
    app_thermostat_lcd_update(1);
}

void button_init(void)
{
#define K1_POWER_GPIO       BUTTON_GPIO
// #define K2_M_GPIO           3
// #define K3_TIME_GPIO        4
#define K4_SPEED_GPIO       3
#define K5_UP_GPIO          4
#define K6_DOWN_GPIO        5

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
#endif
    button_handle_t k4_speed_handle = iot_button_create(K4_SPEED_GPIO, BUTTON_ACTIVE_LEVEL);
    if (k4_speed_handle) {
        /* Register a callback for a button tap (short press) event */
        iot_button_set_evt_cb(k4_speed_handle, BUTTON_CB_TAP, push_btn_cb, "speed");
        /* Register Wi-Fi reset and factory reset functionality on same button */

    }

    button_handle_t k5_up_handle = iot_button_create(K5_UP_GPIO, BUTTON_ACTIVE_LEVEL);
    if (k5_up_handle) {
        /* Register a callback for a button tap (short press) event */
        iot_button_set_evt_cb(k5_up_handle, BUTTON_CB_TAP, push_btn_cb, "up");
        /* Register Wi-Fi reset and factory reset functionality on same button */
    }

    button_handle_t k6_down_handle = iot_button_create(K6_DOWN_GPIO, BUTTON_ACTIVE_LEVEL);
    if (k6_down_handle) {
        /* Register a callback for a button tap (short press) event */
        iot_button_set_evt_cb(k6_down_handle, BUTTON_CB_TAP, push_btn_cb, "down");
        /* Register Wi-Fi reset and factory reset functionality on same button */
    }
}

void app_driver_init()
{
    app_thermostat_init();

    button_init();
}
