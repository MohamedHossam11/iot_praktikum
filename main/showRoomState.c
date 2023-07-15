#include "esp_event.h"
#include "timeMgmt.h"

#include "counting.h"
#include "showRoomState.h"
#include <stdio.h>
#include "esp_log.h"


#include "roomMonitoring.h"

#include <driver/adc.h>
#include <math.h>
#include "esp_pm.h"
#include "epaperInterface.h"

// void epaperShow(int x, int y, char* text, int fontsize);
// void epaperClear();
// void epaperUpdate();


void showRoomState( void * pvParameters ){
	int oldCount= -1;
	int lastMinute = 0;
	
	for(;;){

		//ESP_LOGI(TAG,"Task1 running on core %d",xPortGetCoreID());
		struct tm timeinfo = { 0 };
		time_t now = 0;
		time(&now);
		localtime_r(&now, &timeinfo);

		if (oldCount!=count || lastMinute!=timeinfo.tm_min){

			epaperClear();

			oldCount=count;
			lastMinute=timeinfo.tm_min;
			
			char buf[64];
			printf("count: %d\n",count);
			sprintf(buf, "GE    %.2d:%.2d",timeinfo.tm_hour,timeinfo.tm_min);
			epaperShow(0,  0, buf,4);
			
			sprintf(buf, "Count  : %.2d",count);
			epaperShow(0,  80, buf, 4);
			
			sprintf(buf, "Predict: %.2d",prediction);
			epaperShow(0, 120, buf, 4);
			epaperUpdate();

		}
		vTaskDelay(500 / portTICK_RATE_MS);
	} 
}

