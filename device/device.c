
#include "device.h"
#include "driver/gpio.h"
#include "mqtt_client.h"
#include "statistics/statistics.h"

char *device_topic(char *prefix, char *buff, uint8_t *chipid)
{
    sprintf(buff,"%s/devices/%x%x%x",
        prefix, chipid[3],chipid[4],chipid[5]);
    return buff;
}

char *device_data(char *buff, uint8_t *chipid, const char *devtype, int alive)
{
    sprintf(jsondata, "{\"dev\":\"%x%x%x\",\"id\":\"device\","
                      "\"type\":\"%s\",\"connected\":%d}",
                chipid[3],chipid[4],chipid[5],
                devtype, alive);
    return buff;                
}

void device_sendstatus(esp_mqtt_client_handle_t client, char *prefix, const char *appname, uint8_t *chipid)
{
    char topic[42];

    gpio_set_level(BLINK_GPIO, true);
    device_data(jsondata, chipid, appname, 1);
    esp_mqtt_client_publish(client, device_topic(prefix, topic, chipid), jsondata , 0, 0, 1);
    statistics_getptr()->sendcnt++;
    gpio_set_level(BLINK_GPIO, false);
}
