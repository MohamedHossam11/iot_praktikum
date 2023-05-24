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
#include <stdlib.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define INPUT_PIN_1 27
#define INPUT_PIN_2 5

volatile uint8_t count = 99;
volatile uint8_t prediction = 99;
volatile uint8_t countB1 = 0;
volatile uint8_t countB2 = 0;
volatile unsigned int lastDebounceTimeInput1 = 0;
volatile unsigned int lastDebounceTimeInput2 = 0;
volatile unsigned int debounceDelay = 50;
volatile bool currentStateInput1 = false;
volatile bool previousStateInput1 = false;
volatile bool currentStateInput2 = false;
volatile bool previousStateInput2 = false;

xQueueHandle interputQueue1;
xQueueHandle interputQueue2;
bool flag1 = false;
bool flag2 = false;

static void IRAM_ATTR gpio_interrupt_handler_Input1(void *args)
{
	// struct BarrierEvent e;
	int barrierId = 0;
	currentStateInput1 = gpio_get_level(INPUT_PIN_1);
	int pinNumber = (int)args;
	if (currentStateInput1 != previousStateInput1)
	{
		lastDebounceTimeInput1 = millis();
	}
	if ((millis() - lastDebounceTimeInput1) > debounceDelay)
	{
		// e.barrierID = 1;
		// e.on = currentStateInput1;
		// e.timestamp = millis();
		barrierId = 1;
		xQueueSendFromISR(interputQueue1, &barrierId, NULL);
	}
}

static void IRAM_ATTR gpio_interrupt_handler_Input2(void *args)
{
	// struct BarrierEvent e;
	int barrierId = 0;
	int pinNumber = (int)args;
	currentStateInput2 = gpio_get_level(INPUT_PIN_2);
	if (currentStateInput2 != previousStateInput2)
	{
		lastDebounceTimeInput2 = millis();
	}
	if ((millis() - lastDebounceTimeInput2) > debounceDelay)
	{
		// e.barrierID = 2;
		// e.on = currentStateInput2;
		// e.timestamp = millis();
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
	char number[33];
	char prediction_number[33];
	char clock[64];
	struct tm timeinfo;
	time_t now;
	while (1)
	{
		ssd1306_setFixedFont(ssd1306xled_font8x16);
		ssd1306_printFixed(0, 0, "G10", STYLE_NORMAL);
		ssd1306_setFixedFont(comic_sans_font24x32_123);
		itoa(count, number, 10);
		ssd1306_printFixed(0, 30, number, STYLE_NORMAL);
		time(&now);
		localtime_r(&now, &timeinfo);
		sprintf(clock, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
		ssd1306_setFixedFont(ssd1306xled_font8x16);
		ssd1306_printFixed(70, 0, clock, STYLE_NORMAL);
		ssd1306_setFixedFont(comic_sans_font24x32_123);
		itoa(prediction, prediction_number, 10);
		ssd1306_printFixed(70, 30, prediction_number, STYLE_NORMAL);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

void bufferEvents()
{
	int barrierId = 0;
	while (1)
	{
		if (xQueueReceive(interputQueue1, &barrierId, 0))
		{
			if (barrierId == 1)
			{
				if (xQueueReceive(interputQueue1, &barrierId, 500))
				{
					if (barrierId == 2)
						count++;
				}
			}
			if (barrierId == 2)
			{
				if (xQueueReceive(interputQueue1, &barrierId, 500))
				{
					if (barrierId == 1)
						count--;
				}
			}
			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}
	}
}

void publisherTask()
{
	while (1)
	{
		char msg[256];
		int qos_test = 1;
		sprintf(msg, "{\"sensors\":[{\"name\": \"%s\", \"values\": [{\"timestamp\": %lld, \"count\": %d }]}]}", "group10 sensor", get_timestamp(), count);
		ESP_LOGI("MQTT_SEND", "Topic %s: %s\n", TOPIC, msg);
		int msg_id = esp_mqtt_client_publish(mqttClient, TOPIC, msg, strlen(msg), qos_test, 0);
		if (msg_id == -1)
		{
			ESP_LOGE(TAG, "msg_id returned by publish is -1!\n");
		}
		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}
}

void app_main()
{
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
	gpio_set_intr_type(INPUT_PIN_1, GPIO_INTR_POSEDGE);
	gpio_pad_select_gpio(INPUT_PIN_2);
	gpio_set_direction(INPUT_PIN_2, GPIO_MODE_INPUT);
	gpio_pulldown_en(INPUT_PIN_2);
	gpio_pullup_dis(INPUT_PIN_2);
	gpio_set_intr_type(INPUT_PIN_2, GPIO_INTR_POSEDGE);

	ssd1306_init();
	ssd1306_displayOn();

	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
	wifi_init_sta();
	initSNTP();
	initMQTT();

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

	interputQueue1 = xQueueCreate(1000, sizeof(int));
	interputQueue2 = xQueueCreate(1000, sizeof(struct BarrierEvent *));
	gpio_install_isr_service(0);
	gpio_isr_handler_add(INPUT_PIN_1, gpio_interrupt_handler_Input1, (void *)INPUT_PIN_1);
	gpio_isr_handler_add(INPUT_PIN_2, gpio_interrupt_handler_Input2, (void *)INPUT_PIN_2);
}