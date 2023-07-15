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

#include "roomMonitoring.h"


RTC_NOINIT_ATTR uint64_t timestampOut,timestampIn;
RTC_NOINIT_ATTR bool inFlag, outFlag;
const int debounceDelay=DEBOUNCING_DELAY;

uint64_t millis(){
	return(esp_timer_get_time()/1000);
}

/**
   This interrupt handler is called whenever the outer photoelectric barrier is broken.
*/
void IRAM_ATTR outISR(void* arg){
	//ets_printf("Interrupt OUT %ld %ld.\n",millis(), timestampOut);
	if (millis()<timestampOut+debounceDelay) return;
	ets_printf("Interrupt OUT\n");
	timestampOut=millis();
	if (timestampIn<millis()-RESET_PERIOD) inFlag=false;
	//ets_printf("Interrupt OUT.\n");
	outFlag=true;
	
	if (inFlag){
		if (count > 0) {
		  count -= 1;
		}
		inFlag=false;
		outFlag=false;
	}
}


void outHandler(){	
	printf("Interrupt OUT\n");
	timestampOut=millis();
	//if (timestampIn<millis()-40000) inFlag=false;
	outFlag=true;
	
	if (inFlag){
		if (count > 0) {
		  count -= 1;
		}
		inFlag=false;
		outFlag=false;
	}
}

/**
   This interrupt handler is called whenever the outer photoelectric barrier is broken.
*/
void IRAM_ATTR inISR(void* arg){
	
	if (millis()<timestampIn+debounceDelay) return;
	ets_printf("Interrupt IN.\n");
	timestampIn=millis();
	if (timestampOut<millis()-RESET_PERIOD) outFlag=false;
	//ets_printf("Interrupt IN.\n");
	inFlag=true;
	
	if (outFlag){
		if (count<CAPACITY_OF_ROOM) {
		  count++;
		} else {
		  count=0;
		}
		inFlag=false;
		outFlag=false;
	}
} 


void IRAM_ATTR inHandler(){
	
	timestampIn=millis();
	//if (timestampOut<millis()-40000) outFlag=false;
	printf("Interrupt IN.\n");
	inFlag=true;
	
	if (outFlag){
		if (count<CAPACITY_OF_ROOM) {
		  count++;
		} else {
		  count=0;
		}
		inFlag=false;
		outFlag=false;
	}
} 
