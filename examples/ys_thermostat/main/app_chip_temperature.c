/**
 * @file app_chip_temperature.c
 * @author huangmingle (lelee123a@163.com)
 * @brief 
 * @version 1.0
 * @date 2022-12-30
 * 
 * @copyright Copyright (c) 2022 Leedarson All Rights Reserved.
 * 
 */
#include <esp_log.h>
#include "driver/temperature_sensor.h"

static const char *TAG = "ChipTemp";

static temperature_sensor_handle_t g_temp_sensor = NULL;

int app_chip_temperature_init(void)
{
    temperature_sensor_handle_t temp_sensor = NULL;
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));

    ESP_LOGI(TAG, "Enable temperature sensor");
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));

    if (!temp_sensor) {
        ESP_LOGE(TAG, "init temperature sensor error !!!");
        return -1;
    }
    g_temp_sensor = temp_sensor;
    return 0;
}

float app_chip_temperature_read(void)
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
