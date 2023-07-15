#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "wifiInit.h"
#include "timeMgmt.h"
#include "mqtt.h"
#include "mqtt_client.h"
#include "esp32/rom/rtc.h"
#include "driver/rtc_io.h"

#include "counting.h"
#include "showRoomState.h"
#include "roomMonitoring.h"
#include <driver/adc.h>
#include "esp_pm.h"

#include "esp_sleep.h"
#include "esp_wifi.h"
#include "epaperInterface.h"
#include "wakeup.h"

#ifdef WITH_CALCULATION
#include "referenceWriting.h"
#endif

const uint8_t ledPin = 19; // Onboard LED

RTC_NOINIT_ATTR int count = 0;
int prediction;
RTC_NOINIT_ATTR event_t events[200];
RTC_NOINIT_ATTR int eventCount = 0;
RTC_NOINIT_ATTR uint64_t timeAtStart;
RTC_NOINIT_ATTR int thisIsTheStart = 0;
RTC_NOINIT_ATTR uint64_t lastDebounceTimeInput1 = 0;
RTC_NOINIT_ATTR uint64_t lastDebounceTimeInput2 = 0;

static const char *TAG = "RoomMonitoring";
TaskHandle_t showRoomStateTask, publishRoomCountTask;

esp_mqtt_client_handle_t mqttClient = NULL;

#ifdef WITH_CALCULATION
void calculation()
{
	// Template matching for every text
	int writingTemplateWidth = 200;
	int writingTemplateHeight = 20;
	int writingFieldWidth = 280;
	int writingFieldHeight = 32;
	int minERR = INT_MAX;
	int index = 0;
	unsigned char *writing = (uint8_t *)calloc(writingFieldWidth * writingFieldHeight, sizeof(uint8_t));

	for (int t = 0; t < 16; t++)
	{
		for (int y = 0; y <= writingFieldHeight - writingTemplateHeight; y++)
		{
			for (int x = 0; x <= writingFieldWidth - writingTemplateWidth; x++)
			{
				int ERR = 0;
				for (int i = 0; i < writingTemplateHeight; i++)
				{
					for (int j = 0; j < writingTemplateWidth; j++)
					{
						int searchPix = writing[y * writingFieldWidth + i * writingFieldWidth + j + x];
						int templatePix = references[t][i * writingTemplateWidth + j];
						ERR += abs(searchPix - templatePix);
					}
				}

				if (minERR > ERR)
				{
					minERR = ERR;
					index = t;
				}
			}
		}
	}
	ESP_LOGI(TAG, "Best match: %d", index);
}
#endif

#ifdef WITH_DISPLAY
void epaperDemo()
{
	epaperClear();
	epaperShow(40, 40, "Hallo", 0);
	epaperShow(40, 60, "Hallo", 1);
	epaperShow(40, 80, "Hallo", 2);
	epaperShow(40, 100, "Hallo", 3);
	epaperShow(40, 120, "Hallo", 4);
	epaperUpdate();
}
#endif

void app_main(void)
{
	esp_log_level_set("*", ESP_LOG_ERROR);
	esp_log_level_set("*", ESP_LOG_INFO);
	// esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
	// esp_log_level_set("wifi", ESP_LOG_VERBOSE);

	if (rtc_get_reset_reason(0) == DEEPSLEEP_RESET)
	{
		printf("Wake up from deep sleep\n");
		// ESP_LOGI(TAG, "count %d",count);
		printf("count %d\n", count);
		printf("Event Count: %d \n", eventCount);
		printf("TimeStamp of the last event: %llu \n", events[eventCount - 1].timeStamp);
		int counter = 0;
		while (counter < eventCount)
		{
			if (events[counter].outerBarrier == 1)
			{
				counter++;
				if (counter == eventCount)
					break;
				if (events[counter].outerBarrier == 0 && ((events[counter].timeStamp - events[counter - 1].timeStamp) < 500000))
				{
					count++;
				}
			}
			else
			{
				if (events[counter].outerBarrier == 0)
				{
					if (count < CAPACITY_OF_ROOM)
						counter++;
					if (counter == eventCount)
						break;
					if (events[counter].outerBarrier == 1 && ((events[counter].timeStamp - events[counter - 1].timeStamp) < 500000))
					{
						if (count > 0)
							count--;
					}
				}
			}
		}
		eventCount = 0;
		timeAtStart = 0;
		thisIsTheStart = 0;
		lastDebounceTimeInput1 = 0;
		lastDebounceTimeInput2 = 0;
	}
	else
	{
		printf("Not a deep sleep wake up\n");
		count = 0;
		inFlag = false;
		outFlag = false;
		timestampOut = 0;
		timestampIn = 0;
		eventCount = 0;
		timeAtStart = 0;
		thisIsTheStart = 0;
		// ESP_LOGI(TAG, "Init: count %d",count);
		printf("Init: count %d\n", count);
	}

	// Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

#ifdef DEEP_SLEEP
#ifndef WAKE_STUP
	if (rtc_get_reset_reason(0) == DEEPSLEEP_RESET)
	{
		uint64_t pinMask = esp_sleep_get_ext1_wakeup_status();
		printf("Status: %llx\n", pinMask);
		if (pinMask & (uint64_t)1 << interruptPinIn)
		{
			ESP_LOGI(TAG, "Woken up by inner barrier");
			inHandler();
		}
		if (pinMask & (uint64_t)1 << interruptPinOut)
		{
			ESP_LOGI(TAG, "Woken up by outer barrier");
			outHandler();
		}
	}
#endif
#endif

#ifdef WITH_DISPLAY
	// Power up ePaper display via IO pin
	ESP_ERROR_CHECK(gpio_set_direction(DISPLAY_POWER, GPIO_MODE_OUTPUT));
	ESP_ERROR_CHECK(gpio_set_level(DISPLAY_POWER, 1));

	epaperInit();
	// epaperDemo();
	// vTaskDelay(2000 / portTICK_RATE_MS);
#endif

#ifndef DEEP_SLEEP
	ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));
	ESP_ERROR_CHECK(gpio_set_direction(interruptPinOut, GPIO_MODE_INPUT));
	ESP_ERROR_CHECK(gpio_pulldown_en(interruptPinOut));
	ESP_ERROR_CHECK(gpio_set_intr_type(interruptPinOut, GPIO_INTR_POSEDGE));
	ESP_ERROR_CHECK(gpio_isr_handler_add(interruptPinOut, outISR, NULL));

	ESP_ERROR_CHECK(gpio_set_direction(interruptPinIn, GPIO_MODE_INPUT));
	ESP_ERROR_CHECK(gpio_pulldown_en(interruptPinIn));
	ESP_ERROR_CHECK(gpio_set_intr_type(interruptPinIn, GPIO_INTR_POSEDGE));
	ESP_ERROR_CHECK(gpio_isr_handler_add(interruptPinIn, inISR, NULL));
#endif

	// configPM();

	ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
	ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

#ifdef WITH_NETWORK
	// Creates a station network interface object
	ESP_LOGI(TAG, "Initializing wifi");
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	wifi_init_sta();
	initialize_sntp();
#endif

#ifdef WITH_CALCULATION
	ESP_LOGI(TAG, "Starting calculation");
	// toggleEnergyMeasurement(); //start measurement
	calculation();
	// toggleEnergyMeasurement();     //stop measurement
	ESP_LOGI(TAG, "calculation done");
#endif

#ifdef WITH_DISPLAY
	xTaskCreatePinnedToCore(
		showRoomState,		/* Task function. */
		"showRoomState",	/* name of task. */
		2000,				/* Stack size of task */
		NULL,				/* parameter of the task */
		1,					/* priority of the task */
		&showRoomStateTask, /* Task handle to keep track of created task */
		0);					/* pin task to core 0 */
#endif

#ifdef WITH_PUBLICATION
	xTaskCreatePinnedToCore(
		publishRoomCount,	   /* Task function. */
		"publishRoomCount",	   /* name of task. */
		8000,				   /* Stack size of task */
		NULL,				   /* parameter of the task */
		1,					   /* priority of the task */
		&publishRoomCountTask, /* Task handle to keep track of created task */
		0);					   /* pin task to core 0 */
#endif

#ifdef WAKEUP_STUB
	// Set the wake stub function
	esp_set_deep_sleep_wake_stub(&wake_stub);
#endif

#ifdef DEEP_SLEEP
	ESP_ERROR_CHECK(gpio_set_direction(interruptPinOut, GPIO_MODE_INPUT));
	ESP_ERROR_CHECK(gpio_set_direction(interruptPinIn, GPIO_MODE_INPUT));

	const uint64_t ext_wakeup_pin_1_mask = 1ULL << interruptPinOut;
	const uint64_t ext_wakeup_pin_2_mask = 1ULL << interruptPinIn;

	printf("Enabling EXT1 wakeup on pins GPIO%d, GPIO%d\n", interruptPinOut, interruptPinIn);
	esp_sleep_enable_ext1_wakeup(ext_wakeup_pin_1_mask | ext_wakeup_pin_2_mask, ESP_EXT1_WAKEUP_ANY_HIGH);

	/* If there are no external pull-up/downs, tie wakeup pins to inactive level with internal pull-up/downs via RTC IO
	 * during deepsleep. However, RTC IO relies on the RTC_PERIPH power domain. Keeping this power domain on will
	 * increase some power comsumption. */
	esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
	rtc_gpio_pullup_dis(interruptPinIn);  // disable
	rtc_gpio_pulldown_en(interruptPinIn); // enable
	rtc_gpio_pullup_dis(interruptPinOut);
	rtc_gpio_pulldown_en(interruptPinOut);

	vTaskDelay(5000 / portTICK_RATE_MS);
	printf("count: %d\n", count);
	printf("Going to sleep\n");
#ifdef WITH_DISPLAY
	epaperSleep();
	ESP_ERROR_CHECK(gpio_set_level(DISPLAY_POWER, 0));
#endif
	esp_deep_sleep_start();
#endif

#ifdef LIGHT_SLEEP
	esp_sleep_enable_gpio_wakeup();
	// ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON));

	gpio_wakeup_enable(interruptPinIn, GPIO_INTR_HIGH_LEVEL);
	gpio_wakeup_enable(interruptPinOut, GPIO_INTR_HIGH_LEVEL);
	// ESP_ERROR_CHECK(gpio_pulldown_en(interruptPinIn));
	// ESP_ERROR_CHECK(gpio_pulldown_en(interruptPinOut));
	// gpio_wakeup_enable(interruptPinIn, GPIO_INTR_HIGH_LEVEL);
	// gpio_wakeup_enable(interruptPinOut, GPIO_INTR_HIGH_LEVEL);

	while (true)
	{
		printf("count: %d\n", count);
		printf("Going to light sleep\n");
		vTaskDelay(2000 / portTICK_RATE_MS);
		esp_light_sleep_start();
		vTaskDelay(10000 / portTICK_RATE_MS);
	}
#endif
}
