#ifndef MQTT_H
#define MQTT_H

void mqttInit(void);
void mqttIotInit(void);
void publishRoomCount(void * pvParameters);
void publishCount(void);
void monitorMQTT( void * pvParameters );

#endif
