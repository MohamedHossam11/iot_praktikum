#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "ssd1306.h"
#include "queue.h"
#include <stdlib.h>

#define SDA_PIN 21
#define SCL_PIN 22

volatile uint8_t count = 0;

void showRoomState()
{
	char number[33];
	while (1)
	{
		itoa(count, number, 10);
		ssd1306_printFixed(0, 8, number, STYLE_NORMAL);
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}

void analyse()
{
}

void pickupEvents()
{
	while (1)
	{
		if (gpio_get_level(27) == 1)
		{
			count += 1;
			// vTaskDelay(500 / portTICK_PERIOD_MS);
		}
		if (gpio_get_level(5) == 1)
		{
			count += 1;
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
	gpio_set_direction(5, GPIO_MODE_INPUT);
	gpio_pulldown_en(5);
	gpio_set_direction(27, GPIO_MODE_INPUT);
	gpio_pulldown_en(27);

	struct BarrierEvent barrierEvent;

	barrierEvent.timestamp = 1;

	enqueue(barrierEvent);
	enqueue(barrierEvent);
	show();

	ssd1306_init();
	ssd1306_displayOn();
	ssd1306_setFixedFont(ssd1306xled_font8x16);
	ssd1306_clearScreen();

	xTaskCreate(showRoomState,
				"showRoomState",
				4096,
				NULL,
				10,
				NULL);
	// xTaskCreate(analyse,
	// 			"analyse",
	// 			4096,
	// 			NULL,
	// 			9,
	// 			NULL);
	xTaskCreate(pickupEvents,
				"pickupEvents",
				4096,
				NULL,
				9,
				NULL);
}