#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"


#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "roomMonitoring.h"

#include "esp_sntp.h"


#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, 
   but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1



static const char *TAG = "wifi station";

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
int32_t event_id, void* event_data){
	// START and DISCONNECTED events
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		} else {
			//Signal failed 
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGI(TAG,"connect to the AP fail");
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		//Signal connection
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

void wifi_init_sta(void)
{
	//Creates an event group with 24 bits
	//Stores event flags in individual bits
	//Tasks can wait for the flag to be raised and are then released
	s_wifi_event_group = xEventGroupCreate();
	//Initialize the network interface
	ESP_ERROR_CHECK(esp_netif_init());
	//Creates the system event loop which handles all events
	esp_netif_create_default_wifi_sta();
	//Obtain a default configuration
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	//Create a Wifi driver task and initialize the driver
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	//Register the event handler to the default event loop
	//Events of the specified base id WIFI_EVENT are forwarded
	//to the given event handler.
	//Event handler instance is only needed to deregister the handler
	ESP_ERROR_CHECK(esp_event_handler_instance_register(
				WIFI_EVENT,
				ESP_EVENT_ANY_ID,
				&event_handler,
				NULL,
				&instance_any_id));
				
	ESP_ERROR_CHECK(esp_event_handler_instance_register(
				IP_EVENT,
				IP_EVENT_STA_GOT_IP,
				&event_handler,
				NULL,
				&instance_got_ip));
	//Create a wifi configuration object
	//.threshold.authmode define the minimum authentication mode
	//pmf_cfg: extend privacy protection to network management frames
	wifi_config_t wifi_config = {
		.sta = {
			.ssid = EXAMPLE_ESP_WIFI_SSID,
			.password = EXAMPLE_ESP_WIFI_PASS,
			.listen_interval = 10, //factor of intervals between beacons
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,
			.pmf_cfg = {
				.capable = true,
				.required = false
			},
		},
	};
	ESP_LOGI(TAG, "esp_wifi_set_ps().");
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM); //Required to specify list_interval

	//Determines station mode
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	//Sets the configuration of the station
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	//Starts the station
	ESP_ERROR_CHECK(esp_wifi_start() );
	ESP_LOGI(TAG, "wifi_init_sta finished.");

	//Blocking until connected or failed
	EventBits_t bits = xEventGroupWaitBits(
			s_wifi_event_group,
			WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
			pdFALSE,
			pdFALSE,
			portMAX_DELAY);

		if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
		EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
		EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}
	//Unclear why unregistered
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
	vEventGroupDelete(s_wifi_event_group);
}



