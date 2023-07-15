#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "roomMonitoring.h"
#include "mqtt.h"
#include "wifiinit.h"

static const char *TAG="MQTT";
const static int CONNECTED_BIT = BIT0;
extern esp_mqtt_client_handle_t mqttClient,mqttIotClient;
static EventGroupHandle_t mqtt_event_group;
esp_mqtt_client_handle_t mqttIotClient;
bool mqttDisconnected=false;

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    static int msg_id = 0;
	
    // your_context_t *context = event->context;
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
				ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
				xEventGroupSetBits(mqtt_event_group, CONNECTED_BIT);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
				mqttDisconnected=true;
				esp_restart();
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        printf("ID=%d, total_len=%d, data_len=%d, current_data_offset=%d\n", event->msg_id, event->total_data_len, event->data_len, event->current_data_offset);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
	case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
        break;
	default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}



void mqttIotInit(void){
	mqtt_event_group = xEventGroupCreate();
	const esp_mqtt_client_config_t mqtt_cfg = {
		.event_handle = mqtt_event_handler,
		.host = "mqtt.caps-platform.live",
		.username = "JWT",
		.password = DEVICE_KEY,
		.port = 1883
	};
	
	ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
	mqttIotClient = esp_mqtt_client_init(&mqtt_cfg);
	// esp_mqtt_client_start(mqttIotClient);
	// xEventGroupWaitBits(mqtt_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
	//mqttDisconnected=false;
	ESP_LOGI(TAG, "After esp_mqtt_client_init()");
}

void publishCount(void){
	time_t now = 0;
	char msg[256];

	//esp_wifi_start();
	esp_mqtt_client_start(mqttIotClient);
	xEventGroupWaitBits(mqtt_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
	
	time(&now);

	sprintf(msg, "{\"sensors\":[{\"name\":\"count\", \"values\":[{\"timestamp\":%lu000,\"people\":%d}]}]}",now,count);
	ESP_LOGI(TAG, "Topic %s: %s\n", DEVICE_TOPIC, msg);
	int msg_id = esp_mqtt_client_publish(mqttIotClient,DEVICE_TOPIC, msg, strlen(msg), 2, 0);

	if (msg_id==-1){
		ESP_LOGE(TAG, "msg_id returned by publish is -1!\n");
	} else {
		ESP_LOGI(TAG, "msg_id returned is %d.\n",msg_id);
	}

	esp_mqtt_client_stop(mqttIotClient);
	//esp_wifi_stop();
}

void publishRoomCount(void * pvParameters){
	mqttIotInit();
	
	for(;;){
		publishCount();
		vTaskDelay(1*30*1000 / portTICK_RATE_MS);
		//ESP_LOGI(TAG,"Task1 running on core %d",xPortGetCoreID());
	}
}


void monitorMQTT( void * pvParameters ){
	int msg_id=0;
	
	while (true){
		if (mqttDisconnected){
  			ESP_LOGI(TAG, "Restarting Wifi and MQTT");
			//ESP_ERROR_CHECK(esp_netif_deinit());
			//ESP_ERROR_CHECK(esp_wifi_stop());
			// wifi_init_sta();
			// ESP_LOGI(TAG, "esp_wifi_disconnect()");
 			// ESP_ERROR_CHECK(esp_wifi_disconnect());
			// ESP_LOGI(TAG, "esp_wifi_connect()");
			// ESP_ERROR_CHECK(esp_wifi_connect());
			// ESP_LOGI(TAG, "MQTT reconnect to event source");
			// ESP_ERROR_CHECK(esp_mqtt_client_reconnect(mqttClient));
			// ESP_LOGI(TAG, "MQTT reconnect to IoT Platform");
			// ESP_ERROR_CHECK(esp_mqtt_client_reconnect(mqttIotClient));
			// msg_id = esp_mqtt_client_subscribe(mqttClient, "ROOM_EVENTS",2);
			mqttDisconnected=false;
			// ESP_LOGI(TAG, "sent subscribe for ROOM_EVENTS successful, msg_id=%d", msg_id);

		}
	vTaskDelay(1000 / portTICK_RATE_MS);
	}
}

