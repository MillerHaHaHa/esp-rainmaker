/* Fan Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_standard_types.h> 
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_scenes.h>

#include <app_wifi.h>
#include <app_insights.h>

#include "app_test.h"

#include "app_priv.h"

#define ESP_RMAKER_DEF_VALVE_NAME    "Valve"

static const char *TAG = "app_main";

esp_rmaker_device_t *thermostat_device;

static TimerHandle_t temperature_update_timer;

/* Callback to handle commands received from the RainMaker cloud */
static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
            const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    char *device_name = esp_rmaker_device_get_name(device);
    char *param_name = esp_rmaker_param_get_name(param);

    if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.b? "true" : "false", device_name, param_name);
        app_thermostat_set_power_state(val.val.b);
    } else if (strcmp(param_name, ESP_RMAKER_DEF_SETPOINT_TEMPERATURE_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %f for %s - %s",
                val.val.f, device_name, param_name);
        app_thermostat_set_setpoint_temperature(val.val.f);
    } else if (strcmp(param_name, ESP_RMAKER_DEF_SPEED_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %d for %s - %s",
                val.val.i, device_name, param_name);
        app_thermostat_set_speed(val.val.i);
    } else if (strcmp(param_name, ESP_RMAKER_DEF_DIRECTION_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.s, device_name, param_name);
        app_thermostat_set_direction(val.val.s);
    } else if (strcmp(param_name, ESP_RMAKER_DEF_MODE_CONTROL_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.s, device_name, param_name);
        app_thermostat_set_work_mode(val.val.s);
    } else if (strcmp(param_name, ESP_RMAKER_DEF_VALVE_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.b? "true" : "false", device_name, param_name);
        app_thermostat_set_valve_state(val.val.b);
    } else {
        /* Silently ignoring invalid params */
        return ESP_OK;
    }
    app_thermostat_lcd_update(1);
    esp_rmaker_param_update_and_report(param, val);
    return ESP_OK;
}

static void app_temperature_update(TimerHandle_t handle)
{
    ESP_LOGI(TAG, "timer update temperature");
    esp_rmaker_param_update_and_report(esp_rmaker_device_get_param_by_type(thermostat_device, ESP_RMAKER_PARAM_TEMPERATURE), 
                                        esp_rmaker_float(app_thermostat_get_current_temperature()));
    app_thermostat_lcd_update(0);
}

static esp_err_t app_sensor_update_timer_enable(void)
{
    temperature_update_timer = xTimerCreate("app_temperature_update_tm", (TEMPERATURE_REPORTING_PERIOD * 1000) / portTICK_PERIOD_MS,
                            pdTRUE, NULL, app_temperature_update);
    if (temperature_update_timer) {
        xTimerStart(temperature_update_timer, 0);
        return ESP_OK;
    }
    return ESP_FAIL;
}

void app_main()
{
    esp_log_level_set("*", ESP_LOG_DEBUG);

    /* Initialize Application specific hardware drivers and
     * set initial state.
     */
    app_driver_init();

    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    /* Initialize Wi-Fi. Note that, this should be called before esp_rmaker_node_init()
     */
    app_wifi_init();

    /* Initialize the ESP RainMaker Agent.
     * Note that this should be called after app_wifi_init() but before app_wifi_start()
     * */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Thermostat");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    /* Create a device and add the relevant parameters to it */
    /** thermostat attribute
     * 1. power
     * 2. setpoint temperature
     * 3. speed
     * 4. direction
     * 5. mode
     * 7. temperature
     * 8. valve
     */
    thermostat_device = esp_rmaker_thermostat_device_create("Thermostat", NULL, g_thermostat_params.power_state);
    esp_rmaker_device_add_cb(thermostat_device, write_cb, NULL);
    esp_rmaker_device_add_param(thermostat_device, esp_rmaker_setpoint_temperature_param_create(ESP_RMAKER_DEF_SETPOINT_TEMPERATURE_NAME, g_thermostat_params.setpoint_temp));
    esp_rmaker_device_add_param(thermostat_device, esp_rmaker_speed_param_create(ESP_RMAKER_DEF_SPEED_NAME, g_thermostat_params.speed));
    esp_rmaker_device_add_param(thermostat_device, esp_rmaker_direction_param_custom_create(ESP_RMAKER_DEF_DIRECTION_NAME, g_thermostat_params.direction, direction_list, DIRECTION_CNT));
    esp_rmaker_device_add_param(thermostat_device, esp_rmaker_mode_param_create(ESP_RMAKER_DEF_MODE_CONTROL_NAME, get_workmode_str(g_thermostat_params.work_mode), mode_list, MODE_CNT));
    esp_rmaker_device_add_param(thermostat_device, esp_rmaker_temperature_param_create(ESP_RMAKER_DEF_TEMPERATURE_NAME, g_thermostat_params.local_temp));
    esp_rmaker_device_add_param(thermostat_device, esp_rmaker_power_param_create(ESP_RMAKER_DEF_VALVE_NAME, g_thermostat_params.valve_state));
    esp_rmaker_node_add_device(node, thermostat_device);

    /* Enable test */
    app_test_init();

    /* Enable OTA */
    esp_rmaker_ota_enable_default();

    /* Enable timezone service which will be require for setting appropriate timezone
     * from the phone apps for scheduling to work correctly.
     * For more information on the various ways of setting timezone, please check
     * https://rainmaker.espressif.com/docs/time-service.html.
     */
    esp_rmaker_timezone_service_enable();

    /* Enable scheduling. */
    esp_rmaker_schedule_enable();

    /* Enable Scenes */
    esp_rmaker_scenes_enable();

    /* Enable Insights. Requires CONFIG_ESP_INSIGHTS_ENABLED=y */
    app_insights_enable();

    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();

    /* enable params update timer */
    app_sensor_update_timer_enable();

    /* Start the Wi-Fi.
     * If the node is provisioned, it will start connection attempts,
     * else, it will start Wi-Fi provisioning. The function will return
     * after a connection has been successfully established
     */
    err = app_wifi_start(POP_TYPE_RANDOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }
}
