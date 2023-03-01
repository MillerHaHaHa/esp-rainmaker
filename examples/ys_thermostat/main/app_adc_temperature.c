/**
 * @file app_adc_temperature.c
 * @author huangmingle (lelee123a@163.com)
 * @brief 
 * @version 1.0
 * @date 2022-12-30
 * 
 * @copyright Copyright (c) 2022 Leedarson All Rights Reserved.
 * 
 */
#include <math.h>
#include <esp_log.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "ADCTemp";

#define ADC1_CHAN   ADC_CHANNEL_0
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
    ESP_LOGD(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, ADC1_CHAN, adc_raw);
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage));
    ESP_LOGD(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, ADC1_CHAN, voltage);

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
    ESP_LOGD(TAG, "output Rt = %f Ω", Rt);

    float temp = Rt_to_temp(Rt, b, Rt_default, temp_default);
    ESP_LOGI(TAG, "read temp = %f ℃", temp);

    return temp;
}

void app_adc_temperature_init(void)
{
    ESP_LOGI(TAG, "Initializing adc temperature");
    adc_init();
}

float app_adc_temperature_read(void)
{
    return Vt_to_temp(adc_read_voltage_mv());
}
