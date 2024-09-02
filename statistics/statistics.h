#ifndef __STATISTICS__
#define __STATISTICS__

#include <time.h>
#include "mqtt_client.h"


struct statistics
{
    time_t started;
    uint16_t sensorerrors;
    uint16_t sendcnt;
    uint16_t connectcnt;
    uint16_t disconnectcnt;
    uint16_t maxQElements;
    uint8_t *chipid;
    char *statisticsTopic;
};


extern struct statistics *statistics_getptr(void);
extern void               statistics_send(esp_mqtt_client_handle_t client);
extern struct statistics *statistics_init(const char *prefix, const char *name, uint8_t *chip);
extern void               statistics_close(void);

#endif