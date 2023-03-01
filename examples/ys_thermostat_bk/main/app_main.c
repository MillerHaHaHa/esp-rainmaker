/* Fan Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <esp_log.h>
#include <esp_event.h>
#include <nvs_flash.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_standard_types.h> 
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_scenes.h>
#include <esp_rmaker_console.h>
#include <esp_rmaker_ota.h>

#include <esp_rmaker_common_events.h>

#include <app_wifi.h>
#include <app_insights.h>

#include "app_test.h"

#include "app_priv.h"

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
        g_thermostat_params.display = val.val.b;
    } else if (strcmp(param_name, ESP_RMAKER_DEF_SETPOINT_TEMPERATURE_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %f for %s - %s",
                val.val.f, device_name, param_name);
        app_thermostat_set_setpoint_temperature(val.val.f);
    } else if (strcmp(param_name, ESP_RMAKER_DEF_SPEED_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %d for %s - %s",
                val.val.i, device_name, param_name);
        lcd_set_wind_speed(val.val.i);
    } else if (strcmp(param_name, ESP_RMAKER_DEF_MODE_CONTROL_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %d for %s - %s",
                val.val.i, device_name, param_name);
        ESP_LOGI(TAG, "set work mode: %s", get_workmode_str(val.val.i));
        lcd_set_work_mode(val.val.i);
    } else if (strcmp(param_name, ESP_RMAKER_DEF_VALVE_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.b? "true" : "false", device_name, param_name);
        lcd_set_valve_state(val.val.b);
    } else {
        /* Silently ignoring invalid params */
        return ESP_OK;
    }
    if(ctx->src != ESP_RMAKER_REQ_SRC_INIT) {
        esp_rmaker_param_update_and_report(param, val);
        app_thermostat_lcd_update(1);
    }
    return ESP_OK;
}

/* Event handler for catching RainMaker events */
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == RMAKER_EVENT) {
        switch (event_id) {
            case RMAKER_EVENT_INIT_DONE:
                ESP_LOGI(TAG, "RainMaker Initialised.");
                break;
            case RMAKER_EVENT_CLAIM_STARTED:
                ESP_LOGI(TAG, "RainMaker Claim Started.");
                break;
            case RMAKER_EVENT_CLAIM_SUCCESSFUL:
                ESP_LOGI(TAG, "RainMaker Claim Successful.");
                break;
            case RMAKER_EVENT_CLAIM_FAILED:
                ESP_LOGI(TAG, "RainMaker Claim Failed.");
                break;
            default:
                ESP_LOGW(TAG, "Unhandled RainMaker Event: %"PRIi32, event_id);
        }
    } else if (event_base == RMAKER_COMMON_EVENT) {
        switch (event_id) {
            case RMAKER_EVENT_REBOOT:
                ESP_LOGI(TAG, "Rebooting in %d seconds.", *((uint8_t *)event_data));
                break;
            case RMAKER_EVENT_WIFI_RESET:
                ESP_LOGI(TAG, "Wi-Fi credentials reset.");
                break;
            case RMAKER_EVENT_FACTORY_RESET:
                ESP_LOGI(TAG, "Node reset to factory defaults.");
                break;
            case RMAKER_MQTT_EVENT_CONNECTED:
                ESP_LOGI(TAG, "MQTT Connected.");
                break;
            case RMAKER_MQTT_EVENT_DISCONNECTED:
                ESP_LOGI(TAG, "MQTT Disconnected.");
                break;
            case RMAKER_MQTT_EVENT_PUBLISHED:
                ESP_LOGI(TAG, "MQTT Published. Msg id: %d.", *((int *)event_data));
                break;
            default:
                ESP_LOGW(TAG, "Unhandled RainMaker Common Event: %"PRIi32, event_id);
        }
    } else if (event_base == APP_WIFI_EVENT) {
        switch (event_id) {
            case APP_WIFI_EVENT_QR_DISPLAY:
                ESP_LOGI(TAG, "Provisioning QR : %s", (char *)event_data);
                break;
            case APP_WIFI_EVENT_PROV_TIMEOUT:
                ESP_LOGI(TAG, "Provisioning Timed Out. Please reboot.");
                break;
            case APP_WIFI_EVENT_PROV_RESTART:
                ESP_LOGI(TAG, "Provisioning has restarted due to failures.");
                break;
            default:
                ESP_LOGW(TAG, "Unhandled App Wi-Fi Event: %"PRIi32, event_id);
                break;
        }
    } else if (event_base == RMAKER_OTA_EVENT) {
        switch(event_id) {
            case RMAKER_OTA_EVENT_STARTING:
                ESP_LOGI(TAG, "Starting OTA.");
                break;
            case RMAKER_OTA_EVENT_IN_PROGRESS:
                ESP_LOGI(TAG, "OTA is in progress.");
                break;
            case RMAKER_OTA_EVENT_SUCCESSFUL:
                ESP_LOGI(TAG, "OTA successful.");
                break;
            case RMAKER_OTA_EVENT_FAILED:
                ESP_LOGI(TAG, "OTA Failed.");
                break;
            case RMAKER_OTA_EVENT_REJECTED:
                ESP_LOGI(TAG, "OTA Rejected.");
                break;
            case RMAKER_OTA_EVENT_DELAYED:
                ESP_LOGI(TAG, "OTA Delayed.");
                break;
            case RMAKER_OTA_EVENT_REQ_FOR_REBOOT:
                ESP_LOGI(TAG, "Firmware image downloaded. Please reboot your device to apply the upgrade.");
                break;
            default:
                ESP_LOGW(TAG, "Unhandled OTA Event: %"PRIi32, event_id);
                break;
        }
    } else {
        ESP_LOGW(TAG, "Invalid event received!");
    }
}

static void app_temperature_update(TimerHandle_t handle)
{
    esp_rmaker_param_update_and_report(esp_rmaker_device_get_param_by_type(thermostat_device, ESP_RMAKER_PARAM_TEMPERATURE), 
                                        esp_rmaker_float(app_thermostat_get_current_temperature_and_time()));
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
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("app_wifi", ESP_LOG_DEBUG);
    esp_log_level_set("esp_rmaker_user_mapping", ESP_LOG_DEBUG);
    esp_log_level_set("protocomm_nimble", ESP_LOG_DEBUG);
    esp_log_level_set("protocomm", ESP_LOG_DEBUG);
    esp_log_level_set("WiFiProvConfig", ESP_LOG_DEBUG);
    esp_log_level_set("proto_wifi_scan", ESP_LOG_DEBUG);

    /* Initialize Application specific hardware drivers and
     * set initial state.
     */
    esp_rmaker_console_init();
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

    /* Register an event handler to catch RainMaker events */
    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_COMMON_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(APP_WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_OTA_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

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
     * 4. mode
     * 5. temperature
     * 6. valve
     */
#if USE_YS_MQTT_BROKER
    thermostat_device = esp_rmaker_thermostat_device_create("thermostat", NULL, g_thermostat_params.power_state);
#else
    thermostat_device = esp_rmaker_thermostat_device_create("Thermostat", NULL, 1);
#endif
    esp_rmaker_device_add_cb(thermostat_device, write_cb, NULL);
    esp_rmaker_device_add_param(thermostat_device, esp_rmaker_setpoint_temperature_param_create(ESP_RMAKER_DEF_SETPOINT_TEMPERATURE_NAME, 0));
    esp_rmaker_device_add_param(thermostat_device, esp_rmaker_speed_param_create(ESP_RMAKER_DEF_SPEED_NAME, (int) E_WIND_MODE_AUTO));
    esp_rmaker_device_add_param(thermostat_device, esp_rmaker_mode_param_create(ESP_RMAKER_DEF_MODE_CONTROL_NAME, (int) E_WORK_MODE_AUTO));
    esp_rmaker_device_add_param(thermostat_device, esp_rmaker_temperature_param_create(ESP_RMAKER_DEF_TEMPERATURE_NAME, app_thermostat_get_current_temperature_and_time()));
    esp_rmaker_device_add_param(thermostat_device, esp_rmaker_power_param_create(ESP_RMAKER_DEF_VALVE_NAME, 0));
    esp_rmaker_node_add_device(node, thermostat_device);

    /* Enable test */
    // app_test_init();

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

    /* Update lcd after restore backup data */
    app_thermostat_lcd_update(1);

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
