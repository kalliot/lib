#ifndef __TEMPERATURES__
#define __TEMPERATURES__

#include "mqtt_client.h"

extern bool temperature_send(char *prefix, struct measurement *data, esp_mqtt_client_handle_t client);
extern int temperature_init(int gpio, const char *name, uint8_t *chip, int sensorcnt);
extern char *temperature_getsensor(int index);
extern void temperature_sendall(void);
extern bool temperature_set_friendlyname(char *sensorName, char *friendlyName);
extern char *temperature_get_friendlyname(int index);

#endif