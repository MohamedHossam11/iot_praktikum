#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "ssd1306.h"
#include "esp_log.h"
#include "queue.h"
#include "wifi_module.h"
#include "timeMgmt.h"
#include "mqtt.h"
#include "time.h"
#include "caps_ota.h"
#include "platform_api.h"
#include <stdlib.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define INPUT_PIN_1 27
#define INPUT_PIN_2 5

// RTC_NOINIT_ATTR uint8_t inMemory;
nvs_handle_t my_handle;
volatile uint8_t count = 0;
volatile uint8_t prediction = 0;
volatile unsigned int lastDebounceTimeInput1 = 0;
volatile unsigned int lastDebounceTimeInput2 = 0;
volatile unsigned int debounceDelay = 50;
volatile bool currentStateInput1 = false;
volatile bool previousStateInput1 = false;
volatile bool currentStateInput2 = false;
volatile bool previousStateInput2 = false;

xQueueHandle interputQueue1;
bool flag1 = false;
bool flag2 = false;

static void IRAM_ATTR gpio_interrupt_handler_Input1(void *args)
{
	int barrierId = 0;
	currentStateInput1 = gpio_get_level(INPUT_PIN_1);
	int pinNumber = (int)args;
	if (currentStateInput1 != previousStateInput1)
	{
		lastDebounceTimeInput1 = millis();
	}
	if ((millis() - lastDebounceTimeInput1) > debounceDelay)
	{
		barrierId = 1;
		xQueueSendFromISR(interputQueue1, &barrierId, NULL);
	}
}

static void IRAM_ATTR gpio_interrupt_handler_Input2(void *args)
{
	int barrierId = 0;
	int pinNumber = (int)args;
	currentStateInput2 = gpio_get_level(INPUT_PIN_2);
	if (currentStateInput2 != previousStateInput2)
	{
		lastDebounceTimeInput2 = millis();
	}
	if ((millis() - lastDebounceTimeInput2) > debounceDelay)
	{
		barrierId = 2;
		xQueueSendFromISR(interputQueue1, &barrierId, NULL);
	}
}

int64_t get_timestamp(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec) * 1000LL + (tv.tv_usec / 1000LL);
}

void showRoomState()
{
	ssd1306_clearScreen();
	ssd1306_setFixedFont(ssd1306xled_font8x16);
	ssd1306_printFixed(0, 0, "G10", STYLE_NORMAL);
	char number[33];
	char prediction_number[33];
	char clock[64];
	struct tm timeinfo;
	time_t now;
	while (1)
	{
		ssd1306_setFixedFont(comic_sans_font24x32_123);
		sprintf(number, "%d", count);
		ssd1306_printFixed(0, 30, "  ", STYLE_NORMAL);
		ssd1306_printFixed(0, 30, number, STYLE_NORMAL);
		time(&now);
		localtime_r(&now, &timeinfo);
		sprintf(clock, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
		ssd1306_setFixedFont(ssd1306xled_font8x16);
		ssd1306_printFixed(70, 0, clock, STYLE_NORMAL);
		ssd1306_setFixedFont(comic_sans_font24x32_123);
		sprintf(prediction_number, "%d", prediction);
		ssd1306_printFixed(70, 30, "  ", STYLE_NORMAL);
		ssd1306_printFixed(70, 30, prediction_number, STYLE_NORMAL);
		vTaskDelay(3000 / portTICK_PERIOD_MS);
	}
}
void getPredictionFromIot()
{
	while (1)
	{
		esp_err_t err_platform = platform_api_init("http://caps-platform.live:3000/api/users/36/config/device/fetch");
		if (err_platform == ESP_OK)
		{
			ESP_LOGI("PREDICTION_HTTP", "ALL GOOD");
			platform_api_set_query_string("type", "device");
			platform_api_set_query_string("deviceId", "86");
			platform_api_set_query_string("keys", "prediction");
			platform_api_set_token(JWT_TOKEN);
			ESP_ERROR_CHECK(platform_api_perform_request());
			char *variable;
			ESP_ERROR_CHECK(platform_api_retrieve_val("prediction", STRING, true, NULL, (void **)&variable));
			ESP_ERROR_CHECK(platform_api_cleanup());
			ESP_LOGI("Prediction Value", "%s", variable);
			prediction = atoi(variable) < 0 ? 0 : atoi(variable);
			
		}
		else
		{
			ESP_LOGI("PREDICTION_HTTP", "ERROR");
		}

		vTaskDelay(50000 / portTICK_PERIOD_MS);
	}
}
void setValueNVS()
{
	// Write
	nvs_set_i32(my_handle, "count", count);

	// Commit written value.
	// After setting any values, nvs_commit() must be called to ensure changes are written
	// to flash storage. Implementations may write to storage at other times,
	// but this is not guaranteed.
	printf("Committing updates in NVS ... ");
	nvs_commit(my_handle);
}
void setValueNVSWithValue(uint8_t value)
{
	// Write
	nvs_set_i32(my_handle, "count", value);

	// Commit written value.
	// After setting any values, nvs_commit() must be called to ensure changes are written
	// to flash storage. Implementations may write to storage at other times,
	// but this is not guaranteed.
	printf("Committing zeroing updates in NVS ... ");
	nvs_commit(my_handle);
}
void bufferEvents()
{
	int barrierId = 0;
	while (1)
	{
		if (xQueueReceive(interputQueue1, &barrierId, 0))
		{
			ESP_LOGI("TESTING", "Inside first receive %d", barrierId);
			if (barrierId == 1)
			{
				if (xQueueReceive(interputQueue1, &barrierId, 500))
				{
					ESP_LOGI("TESTING", "Inside second receive %d", barrierId);
					if (barrierId == 2)
						count++;
				}
			}
			else if (barrierId == 2)
			{
				ESP_LOGI("TESTING", "Inside third receive %d", barrierId);
				if (xQueueReceive(interputQueue1, &barrierId, 500))
				{
					ESP_LOGI("TESTING", "Inside fourth receive %d", barrierId);
					if (barrierId == 1)
					{
						if (count > 0)
							count--;
					}
				}
			}
			// inMemory = count;
			setValueNVS();
		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

void publisherTask()
{
	while (1)
	{
		char msg[256];
		char msg_log[1064];
		int qos_test = 1;
		sprintf(msg, "{\"sensors\":[{\"name\": \"%s\", \"values\": [{\"timestamp\": %lld, \"count\": %d }]}]}", "group10 sensor", get_timestamp(), count);
		ESP_LOGI("MQTT_SEND", "Topic %s: %s\n", TOPIC, msg);
		int msg_id = esp_mqtt_client_publish(mqttClient, TOPIC, msg, strlen(msg), qos_test, 0);
		if (msg_id == -1)
		{
			ESP_LOGE(TAG, "msg_id returned by publish is -1!\n");
		}
		sprintf(msg_log, "{\"sensors\":[{\"name\": \"%s\", \"values\": [{\"timestamp\": %lld, \"logMessage\": \"%s\" }]}]}", "Logging", get_timestamp(), "I am still alive!");
		ESP_LOGI("MQTT_SEND", "Topic %s: %s\n", TOPIC, msg_log);
		msg_id = esp_mqtt_client_publish(mqttClient, TOPIC, msg_log, strlen(msg_log), qos_test, 0);
		if (msg_id == -1)
		{
			ESP_LOGE(TAG, "msg_id returned by publish is -1!\n");
		}
		vTaskDelay(25000 / portTICK_PERIOD_MS);
	}
}

void update_ota()
{
	while (1)
	{
		vTaskDelay(30000 / portTICK_PERIOD_MS);
		if (ota_update() == ESP_OK)
		{
			esp_restart();
		}
		vTaskDelay(30000 / portTICK_PERIOD_MS);
	}
}

void checkTimeAndSetNvsVariable()
{
	struct tm timeinfo;
	time_t now;

	time(&now);
	localtime_r(&now, &timeinfo);
	if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0 && count > 0)
	{
		setValueNVSWithValue(0);
	}
}

void app_main()
{
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		// NVS partition was truncated and needs to be erased
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);
	err = nvs_open("storage", NVS_READWRITE, &my_handle);
	if (err != ESP_OK)
	{
		printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
	}
	else
	{
		printf("Done\n");
		// Read
		printf("counter from NVS ... ");
		err = nvs_get_i32(my_handle, "count", &count);
		switch (err)
		{
		case ESP_OK:
			printf("Done\n");
			printf("counter = %" PRIu32 "\n", count);
			break;
		case ESP_ERR_NVS_NOT_FOUND:
			printf("The value is not initialized yet!\n");
			break;
		default:
			printf("Error (%s) reading!\n", esp_err_to_name(err));
		}

		// Close
		// nvs_close(my_handle);
	}
	i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = SDA_PIN,
		.scl_io_num = SCL_PIN,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = 1000000};
	i2c_param_config(I2C_NUM_0, &i2c_config);
	i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

	gpio_pad_select_gpio(INPUT_PIN_1);
	gpio_set_direction(INPUT_PIN_1, GPIO_MODE_INPUT);
	gpio_pulldown_en(INPUT_PIN_1);
	gpio_pullup_dis(INPUT_PIN_1);
	gpio_set_intr_type(INPUT_PIN_1, GPIO_INTR_NEGEDGE);
	gpio_pad_select_gpio(INPUT_PIN_2);
	gpio_set_direction(INPUT_PIN_2, GPIO_MODE_INPUT);
	gpio_pulldown_en(INPUT_PIN_2);
	gpio_pullup_dis(INPUT_PIN_2);
	gpio_set_intr_type(INPUT_PIN_2, GPIO_INTR_NEGEDGE);
	ssd1306_init();
	ssd1306_displayOn();
	ssd1306_clearScreen();
	ssd1306_setFixedFont(ssd1306xled_font8x16);
	ssd1306_printFixed(0, 0, "G10", STYLE_NORMAL);
	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
	ssd1306_setFixedFont(ssd1306xled_font8x16);
	ssd1306_printFixed(20, 20, "WIFI INIT...", STYLE_NORMAL);
	wifi_init_sta();
	ssd1306_printFixed(20, 20, "SNTP INIT...", STYLE_NORMAL);
	initSNTP();
	initMQTT();
	interputQueue1 = xQueueCreate(2000, sizeof(int));
	xTaskCreate(showRoomState,
				"showRoomState",
				4096,
				NULL,
				5,
				NULL);
	xTaskCreate(publisherTask,
				"publisherTask",
				4096,
				count,
				5,
				NULL);
	xTaskCreatePinnedToCore(bufferEvents,
							"bufferEvents",
							4096,
							NULL,
							2,
							NULL,
							1);

	xTaskCreatePinnedToCore(update_ota,
							"otaUpdate",
							4096,
							NULL,
							2,
							NULL,
							1);
	xTaskCreatePinnedToCore(getPredictionFromIot,
							"getPredictionFromIot",
							4096,
							NULL,
							3,
							NULL,
							1);
	TimerHandle_t timer = xTimerCreate("time_timer", pdMS_TO_TICKS(10000), pdTRUE, NULL, checkTimeAndSetNvsVariable);
	if (timer == NULL) {
        printf("Error creating timer\n");
        return;
    }

    // Start the timer
    if (xTimerStart(timer, 0) != pdPASS) {
        printf("Error starting timer\n");
        return;
    }
	gpio_install_isr_service(0);
	gpio_isr_handler_add(INPUT_PIN_1, gpio_interrupt_handler_Input1, (void *)INPUT_PIN_1);
	gpio_isr_handler_add(INPUT_PIN_2, gpio_interrupt_handler_Input2, (void *)INPUT_PIN_2);
	while (1)
	{
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}