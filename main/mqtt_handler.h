#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAGMQTT = "MQTT_EXAMPLE";

uint32_t MQTT_CONNECTED = 0;

static void mqtt_app_start(void);

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAGMQTT, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAGMQTT, "MQTT_EVENT_CONNECTED");
        MQTT_CONNECTED = 1;

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAGMQTT, "MQTT_EVENT_DISCONNECTED");
        MQTT_CONNECTED = 0;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAGMQTT, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAGMQTT, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAGMQTT, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAGMQTT, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAGMQTT, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAGMQTT, "Other event id:%d", event->event_id);
        break;
    }
}

esp_mqtt_client_handle_t client = NULL;
static void mqtt_app_start(void)
{
    ESP_LOGI(TAGMQTT, "STARTING MQTT");
    esp_mqtt_client_config_t mqttConfig = {
        .host = "mqtt.caps-platform.live",
        .port = 1883,
        .username = "JWT",
        .password = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpYXQiOjE2ODQ1MDM5ODMsImlzcyI6ImlvdHBsYXRmb3JtIiwic3ViIjoiMzYvODYifQ.UspMi_MuAj7LIdNnflk4F3LTZXAark2-0qbJAsplaidWTstABjiXFgtOiKbxHMsxXD2BfQ1eevl6KqBbsgFgD3Y_-2tRxrTh3kDGdPibVCoabgu947Z006tNBzUDAN4jSnexQ-Q6rfJM2jGfyOs2yJbvyr7SP4_FVMQTx2MA6twV0KITvfgHgq76qV8jBsDWKTIgBtv1wNZtj4lbN2o8hND-IHo9KOntw35nqU8JmjwqF0VZ_bQZrpwJmSIAJTACldN3KKGRrXpIx_0gperDykNn7Ka9__ci3odxsFk4GjFhWImpr4o82BzWQz4OajfH9_rPwHOJpLIjLFyExpsZhbGgpI418bqmVeXe08CvRccCx44acn21VNe4aIsiPqIKh5A_msErAm_x7A11QvWhw8PA1R_Sk7gyKSjfBUQesnXgSW41jk8cFz4WtVUz9hSimjqPjJ26phQhCgr1rJymxhP9n5yduc7MUpRp3uqp6eWRk2reLcneJP5gfytuU2SkQSk1_eB4p3YLa1JZqM5IXuvQi-ErJY29v6ERXGbIKTu0zBctXf1wuEHzBdazxzXX6_USmmBzWBYPhUJ5pITJ-rWHaKW51mTNpvjEBLT9XHu4KodgAaSl0v9nIT2NKNhMja8Wtpo5SXxn1xynktwbbTjnMAw5PquHKpLw1-LYE0U"

    };
    client = esp_mqtt_client_init(&mqttConfig);

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

int64_t get_timestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec) * 1000LL + (tv.tv_usec / 1000LL);
}

void publisherTask(void *params)
{
    int count_sent;
    while (true)
    {
        if (MQTT_CONNECTED)
        {
            count_sent = (int)params;
            printf("Count now %d \n", count_sent);
            char *mqtt_msg;
            int bytes = asprintf(&mqtt_msg, "{\"sensors\":[{\"name\": \"%s\", \"values\": [{\"timestamp\": %lld, \"count\": %d }]}]", "group10 sensor", get_timestamp(), count_sent);
            ESP_ERROR_CHECK(esp_mqtt_client_publish(client, "36/86/data", mqtt_msg, bytes, 1, 0));
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
    }
}