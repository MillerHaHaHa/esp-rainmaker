
#include <string.h>
#include <ht1621.h>
#include <json_parser.h>
#include <esp_log.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_work_queue.h>
#include "esp_rmaker_mqtt.h"
#include "app_priv.h"

#define APP_TEST_TOPIC_SUFFIX "test"
#define APP_TEST_MQTT_QOS 1

static const char *TAG = "app_test";

static void app_test_handler(const char *topic, void *payload, size_t payload_len, void *priv_data)
{
    /* Starting Firmware Upgrades */
    ESP_LOGI(TAG, "Test Handler got:%.*s on %s topic\n", (int) payload_len, (char *)payload, topic);

/***
{
    "addr": 0,
    "data": 0
}
 ***/
#define HT1621_ADDR_STR   "addr"
#define HT1621_DATA_STR   "data"

    jparse_ctx_t jctx;
    int addr = 0;
    bool bvalue = 0;
    int ivalue = 0;
    float fvalue = 0;
    char svalue[10] = {0};

    int ret = json_parse_start(&jctx, (char *)payload, (int) payload_len);
    if (ret != 0) {
        ESP_LOGE(TAG, "Invalid JSON received: %s", (char *)payload);
        return;
    }
    ESP_LOGI(TAG, "JSON receive: \n%s\n", (char*)jctx.js);

    ret = json_obj_get_int(&jctx, HT1621_ADDR_STR, &addr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error get int %s", HT1621_ADDR_STR);
    } else {
        ESP_LOGI(TAG, "%s: %d", HT1621_ADDR_STR, addr);

        ret = json_obj_get_int(&jctx, HT1621_DATA_STR, &ivalue);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error get int %s", HT1621_DATA_STR);
            goto end;
        }
        ESP_LOGI(TAG, "%s: 0x%02x", HT1621_DATA_STR, ivalue);
        ht1621_set_buffer(addr, ivalue);
        ht1621_update();
        goto end;
    }

    app_thermostat_lcd_update(1);

end:
    json_parse_end(&jctx);
    return;
}

static esp_err_t app_test_subscribe(void *priv_data)
{
    char subscribe_topic[100];

    snprintf(subscribe_topic, sizeof(subscribe_topic),"node/%s/%s", esp_rmaker_get_node_id(), APP_TEST_TOPIC_SUFFIX);

    ESP_LOGI(TAG, "Subscribing to: %s", subscribe_topic);
    /* First unsubscribe, in case there is a stale subscription */
    esp_rmaker_mqtt_unsubscribe(subscribe_topic);
    esp_err_t err = esp_rmaker_mqtt_subscribe(subscribe_topic, app_test_handler, APP_TEST_MQTT_QOS, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Test Subscription Error %d", err);
    }
    return err;
}

static void app_test_work_fn(void *priv_data)
{
    /* If the firmware was rolled back, indicate that first */
    app_test_subscribe(priv_data);
}

esp_err_t app_test_enable_using_topics(void)
{
    esp_err_t err = esp_rmaker_work_queue_add_task(app_test_work_fn, NULL);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "app test enabled with Topics");
    }
    return err;
}

void app_test_init(void)
{
    ESP_LOGE(TAG, "Init test module");
    app_test_enable_using_topics();
}
