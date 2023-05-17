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
volatile uint8_t count = 100;
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

void showRoomState()
{
	char number[33];
	while (1)
	{
		itoa(count, number, 10);
		ssd1306_printFixed(0, 8, number, STYLE_NORMAL);
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
			// if (barrierId == 2)
			// {
			// 	countB2++;
			// }
			// if(xQueueReceiveFromISR(interputQueue1, &e1, NULL) == pdTRUE && xQueueReceiveFromISR(interputQueue1, &e2, NULL) == pdTRUE) {
			// 	count++;
			// }

			// else if (e1.barrierID == 0)
			// {
			// 	xQueueSendFromISR(interputQueue1, &e2, NULL);
			// }
			// else if (e2.barrierID == 0)
			// {
			// 	xQueueSendFromISR(interputQueue1, &e1, NULL);
			// }
			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}
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
	interputQueue1 = xQueueCreate(1000, sizeof(int));
	interputQueue2 = xQueueCreate(1000, sizeof(struct BarrierEvent *));
	gpio_install_isr_service(0);
	gpio_isr_handler_add(INPUT_PIN_1, gpio_interrupt_handler_Input1, (void *)INPUT_PIN_1);
	gpio_isr_handler_add(INPUT_PIN_2, gpio_interrupt_handler_Input2, (void *)INPUT_PIN_2);
}