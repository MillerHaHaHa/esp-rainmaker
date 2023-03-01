#ifndef _USER_DEBUG_HEADER_H_
#define _USER_DEBUG_HEADER_H_

#define USE_YS_MQTT_BROKER 1

#define USER_SKIP_PROVISION 0
#define USER_SKIP_PROVISION_WIFI_SSID   "lgyyyds"
#define USER_SKIP_PROVISION_WIFI_PASSWD "Lelee123a940913"
#define USER_SKIP_PROVISION_USER_ID     "3253ead7-f007-4509-80cb-91750661e623"
#define USER_SKIP_PROVISION_SECRET_KEY  "95bf5a0a-0770-4838-b1bf-2e46a668bf1d"

#define LOCAL_RESET_TOPIC_RULE                  "esp_local_reset"
#define REMOTE_RESET_TOPIC_RULE                 "esp_remote_reset"

#define LOCAL_RESET_TOPIC_SUFFIX                "reset"
#define REMOTE_RESET_TOPIC_SUFFIX               "remove"

#define USER_MQTT_TOPIC_BUFFER_SIZE 150

void user_debug_header_func(void);

#endif // _USER_DEBUG_HEADER_H_
