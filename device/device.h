#ifndef __HOMEDEVICE__
#define __HOMEDEVICE__

#include <stdint.h>
#include "mqtt_client.h"
#include "homeapp.h"


extern char *device_topic(char *prefix, char *buff, uint8_t *chipid);
extern char *device_data(char *buff, uint8_t *chipid, const char *devtype, int alive);
extern void device_sendstatus(esp_mqtt_client_handle_t client, char *prefix, const char *appname, uint8_t *chipid);


#endif