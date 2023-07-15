

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "roomMonitoring.h"




#include "esp_sntp.h"
#include "timeMgmt.h"


extern int interrupt;

static const char *TAG = "time management";

//Function to be called when RTC is set
void time_sync_notification_cb(struct timeval *tv)
{
	ESP_LOGI(TAG, "Notification of a time synchronization event");
}



void initialize_sntp(void)
{
	ESP_LOGI(TAG, "Initializing SNTP");
	//Poll SNTP server via unicast. Default every hour
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	//Define SNTP server outside and in the MI building
	sntp_setservername(0, SNTP_SERVER);
	//Define update notification function.
	sntp_set_time_sync_notification_cb(time_sync_notification_cb);
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
	//if update necessary RTC is updated smoothly to reduce error
	sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
#endif
	//Fetches time and keeps updating RTC
	sntp_init();
	//Datatype for time in seconds since 1.1.1970
	time_t now = 0;
	//Linux data structure with calendar date and time broken down into its components. 
	struct tm timeinfo = { 0 };
	//Number of retries waiting for synchronization
	int retry = 0;
	const int retry_count = 10;
	while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
		ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
	if (retry==retry_count){
		ESP_LOGE(TAG,"Could not retrieve time.!\n");
		esp_restart();
	}
	//Retrieve time in seconds
	time(&now);
	char strftime_buf[64];
	
	setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
	tzset();
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI(TAG, "The current date/time in Germany is: %s", strftime_buf);
	
}



