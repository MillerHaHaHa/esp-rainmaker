/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/* It is recommended to copy this code in your example so that you can modify as
 * per your application's needs, especially for the indicator calbacks,
 * wifi_reset_indicate() and factory_reset_indicate().
 */
#include <esp_log.h>
#include <esp_err.h>
#include <iot_button.h>
#include <json_generator.h>
#include <esp_rmaker_utils.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_mqtt.h>
#include <user_debug_header.h>

static const char *TAG = "app_reset";

#define REBOOT_DELAY        2
#define RESET_DELAY         2

static void wifi_reset_trigger(void *arg)
{
    esp_rmaker_wifi_reset(RESET_DELAY, REBOOT_DELAY);
}

static void wifi_reset_indicate(void *arg)
{
    ESP_LOGI(TAG, "Release button now for Wi-Fi reset. Keep pressed for factory reset.");
}

void factory_reset_trigger(void *arg)
{
    char publish_payload[200];
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, publish_payload, sizeof(publish_payload), NULL, NULL);
    json_gen_start_object(&jstr);
    char *node_id = esp_rmaker_get_node_id();
    json_gen_obj_set_string(&jstr, "node_id", node_id);
    json_gen_end_object(&jstr);
    json_gen_str_end(&jstr);

    char publish_topic[USER_MQTT_TOPIC_BUFFER_SIZE];
    esp_rmaker_create_mqtt_topic(publish_topic, USER_MQTT_TOPIC_BUFFER_SIZE, LOCAL_RESET_TOPIC_SUFFIX, LOCAL_RESET_TOPIC_RULE);

    esp_err_t err = esp_rmaker_mqtt_publish(publish_topic, publish_payload, strlen(publish_payload), RMAKER_MQTT_QOS1, NULL);
    ESP_LOGI(TAG, "MQTT Publish Topic: %s", publish_topic);
    ESP_LOGI(TAG, "MQTT Publish: %s", publish_payload);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MQTT Publish Error %d", err);
    }

    esp_rmaker_factory_reset(RESET_DELAY, REBOOT_DELAY);
}

static void factory_reset_indicate(void *arg)
{
    ESP_LOGI(TAG, "Release button to trigger factory reset.");
}

esp_err_t app_reset_button_register(button_handle_t btn_handle, uint8_t wifi_reset_timeout,
        uint8_t factory_reset_timeout)
{
    if (!btn_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (wifi_reset_timeout) {
        iot_button_add_on_release_cb(btn_handle, wifi_reset_timeout, wifi_reset_trigger, NULL);
        iot_button_add_on_press_cb(btn_handle, wifi_reset_timeout, wifi_reset_indicate, NULL);
    }
    if (factory_reset_timeout) {
        if (factory_reset_timeout <= wifi_reset_timeout) {
            ESP_LOGW(TAG, "It is recommended to have factory_reset_timeout > wifi_reset_timeout");
        }
        iot_button_add_on_release_cb(btn_handle, factory_reset_timeout, factory_reset_trigger, NULL);
        iot_button_add_on_press_cb(btn_handle, factory_reset_timeout, factory_reset_indicate, NULL);
    }
    return ESP_OK;
}

button_handle_t app_reset_button_create(gpio_num_t gpio_num, button_active_t active_level)
{
    return iot_button_create(gpio_num, active_level);
}
