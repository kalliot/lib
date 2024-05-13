#ifndef __OTA__
#define __OTA__ 

#include "mqtt_client.h"

extern char *ota_init(char *prefix, char *myname, uint8_t *chip);
extern void ota_start(char *fname);
extern void ota_status_publish(struct measurement *data, esp_mqtt_client_handle_t client);
extern void ota_cancel_rollback(void);


#endif