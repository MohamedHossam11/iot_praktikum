#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "ssd1306.h"
#include "esp_log.h"
#include "queue.h"
#include <stdlib.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define INPUT_PIN_1 27
#define INPUT_PIN_2 5

static const char *TAG = "BLINK";
volatile uint8_t count = 0;
volatile unsigned int lastDebounceTimeInput1 = 0;
volatile unsigned int lastDebounceTimeInput2 = 0;
volatile unsigned int debounceDelay = 250;
volatile bool currentStateInput1 = false;
volatile bool previousStateInput1 = false;
volatile bool currentStateInput2 = false;
volatile bool previousStateInput2 = false;

xQueueHandle interputQueue;
bool flag1 = false;
bool flag2 = false;

static void IRAM_ATTR gpio_interrupt_handler_Input1(void *args)
{
	currentStateInput1 = gpio_get_level(INPUT_PIN_1);
	int pinNumber = (int)args;
	if (currentStateInput1 != previousStateInput1)
	{
		lastDebounceTimeInput1 = millis();
	}
	if ((millis() - lastDebounceTimeInput1) > debounceDelay)
	{
		xQueueSendFromISR(interputQueue, &pinNumber, NULL);
	}
}

static void IRAM_ATTR gpio_interrupt_handler_Input2(void *args)
{
	int pinNumber = (int)args;
	currentStateInput2 = gpio_get_level(INPUT_PIN_2);
	if (currentStateInput2 != previousStateInput2)
	{
		lastDebounceTimeInput2 = millis();
	}
	if ((millis() - lastDebounceTimeInput2) > debounceDelay)
	{
		xQueueSendFromISR(interputQueue, &pinNumber, NULL);
	}
}

void showRoomState()
{
	char number[33];
	while (1)
	{
		itoa(count, number, 10);
		ssd1306_printFixed(0, 8, number, STYLE_NORMAL);
		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}
}

void analyse()
{
	while (1)
	{
		struct BarrierEvent front = dequeue();
		struct BarrierEvent front2 = top();
		if (front.barrierID == 1 && front2.barrierID == 2)
		{
			count++;
			dequeue();
		}
		else if (front.barrierID == 1 && front2.barrierID == 1)
		{
			continue;
		}
		else if (front.barrierID == 2 && front2.barrierID == 1)
		{
			count--;
			dequeue();
		}
		else if (front.barrierID == 2 && front2.barrierID == 2)
		{
			continue;
		}
	}
}

void bufferEvents()
{
	int pinNumber = 0;
	while (1)
	{
		if (xQueuePeekFromISR(interputQueue, &pinNumber) == pdFALSE)
		{
			continue;
		}
		struct BarrierEvent barrierEventDequeued;
		struct BarrierEvent barrierEventPeek;
		if (xQueueReceiveFromISR(interputQueue, &pinNumber, NULL))
		{
			barrierEventDequeued.barrierID = pinNumber == INPUT_PIN_1 ? 1 : 2;
			barrierEventDequeued.on = gpio_get_level(pinNumber);
			barrierEventDequeued.timestamp = millis();
		}
		if (xQueuePeekFromISR(interputQueue, &pinNumber))
		{
			barrierEventPeek.barrierID = pinNumber == INPUT_PIN_1 ? 1 : 2;
			barrierEventPeek.on = gpio_get_level(pinNumber);
			barrierEventPeek.timestamp = millis();
		}
		// if (barrierEventDequeued.barrierID == INPUT_PIN_1 && barrierEventPeek)
		// {
		// }
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
	ssd1306_setFixedFont(ssd1306xled_font8x16);
	ssd1306_clearScreen();

	xTaskCreate(showRoomState,
				"showRoomState",
				4096,
				NULL,
				5,
				NULL);
	xTaskCreatePinnedToCore(bufferEvents,
							"bufferEvents",
							4096,
							NULL,
							2,
							NULL,
							1);
	// xTaskCreate(analyse,
	// 			"analyse",
	// 			4096,
	// 			NULL,
	// 			8,
	// 			NULL);
	interputQueue = xQueueCreate(10, sizeof(int));
	gpio_install_isr_service(0);
	gpio_isr_handler_add(INPUT_PIN_1, gpio_interrupt_handler_Input1, (void *)INPUT_PIN_1);
	gpio_isr_handler_add(INPUT_PIN_2, gpio_interrupt_handler_Input2, (void *)INPUT_PIN_2);
}