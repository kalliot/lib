#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "homeapp.h"
#include "statistics/statistics.h"
#include "driver/gpio.h"
#include "ds18b20.h"
#include "mqtt_client.h"

#define SENSOR_NAMELEN 17
#define FRIENDLY_NAMELEN 20
#define NO_CHANGE_INTERVAL 900

static int tempSensorCnt;
static uint8_t *chipid;
static const char *myName;
static int max_sensors = 0;
static DeviceAddress *tempSensors;
static char temperatureTopic[80];

static const char *TAG = "TEMPERATURE";

static struct oneWireSensor {
    float prev;
    float lastValid;
    time_t lastValidTs;
    time_t prevsend;
    char sensorname[SENSOR_NAMELEN];
    char friendlyName[FRIENDLY_NAMELEN];
    int err;
    DeviceAddress addr;
} *sensors;            


static bool isDuplicate(DeviceAddress addr, int currentCnt)
{
    for (int i = 0; i < currentCnt; i++)
    {
        if (!memcmp(tempSensors[i],addr,sizeof(DeviceAddress)))
        {
            return true;
        }
    }
    return false;
}


static int temp_getaddresses(DeviceAddress *tempSensorAddresses) {
	unsigned int numberFound = 0;
    
    reset_search();
    for (int i = 0; i < max_sensors * 3; i++) // average 3 retries for each sensor.
    {
        gpio_set_level(BLINK_GPIO, true);
        ESP_LOGI(TAG,"searching address %d", numberFound);
        if (search(tempSensorAddresses[numberFound], true))
        {
            if (numberFound > 0 && isDuplicate(tempSensorAddresses[numberFound], numberFound))
            {
                ESP_LOGI(TAG,"duplicate address, rejecting\n");
            }
            else
            {
                ESP_LOGI(TAG,"found");
                numberFound++;
            }
        }
        gpio_set_level(BLINK_GPIO, false);
        if (numberFound == max_sensors)
        {
            return numberFound;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    return numberFound;
}


char *temperature_getsensor(int index)
{
    if (index >= tempSensorCnt)
        return NULL;
    return sensors[index].sensorname;
}


bool temperature_send(char *prefix, struct measurement *data, esp_mqtt_client_handle_t client)
{
    time_t now;
    int retain = 1;

    time(&now);
    gpio_set_level(BLINK_GPIO, true);

    if (now < MIN_EPOCH)
    {
        now = 0;
        retain = 0;
    }

    static char *datafmt = "{\"dev\":\"%x%x%x\",\"sensor\":\"%s\",\"name\":\"%s\",\"id\":\"temperature\",\"value\":%.02f,\"ts\":%jd,\"err\":%d}";
    sprintf(temperatureTopic,"%s/%s/%x%x%x/parameters/temperature/%s", prefix, myName, chipid[3], chipid[4], chipid[5], sensors[data->gpio].sensorname);

    sprintf(jsondata, datafmt,
                chipid[3],chipid[4],chipid[5],
                sensors[data->gpio].sensorname,
                sensors[data->gpio].friendlyName,
                data->data.temperature,
                now,
                sensors[data->gpio].err);
    esp_mqtt_client_publish(client, temperatureTopic, jsondata , 0, 0, retain);
    statistics_getptr()->sendcnt++;
    gpio_set_level(BLINK_GPIO, false);
    return true;
}



static void getFirstTemperatures()
{
    float temperature;
    int success_cnt = 0;

    for (int k=0; k < 5; k++)
    {
        ds18b20_requestTemperatures();
        for (int i=0; i < tempSensorCnt; i++) {
            if (sensors[i].prev != 0.0)
            {
                continue;
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            temperature = ds18b20_getTempC((DeviceAddress *) sensors[i].addr);
            if (temperature < -30.0 || temperature > 95.0) {
                ESP_LOGI(TAG,"%s failed with initial value %f, reading again", sensors[i].sensorname, temperature);
            }
            else {
                time(&sensors[i].lastValidTs);
                sensors[i].lastValid = temperature;
                sensors[i].prev = temperature;
                sensors[i].err = 0;
                success_cnt++;
            }
        }
        if (success_cnt == tempSensorCnt) return;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void sendMeasurement(int index, float value)
{
    struct measurement meas;
    meas.id = TEMPERATURE;
    meas.gpio = index;
    meas.data.temperature = value;
    xQueueSend(evt_queue, &meas, 0);
}

char *temperature_get_friendlyname(int index)
{
    if (index >= tempSensorCnt)
        return NULL;
    return sensors[index].friendlyName;
}

bool temperature_set_friendlyname(char *sensorName, char *friendlyName)
{
    for (int i = 0; i < tempSensorCnt; i++)
    {
        if (!strcmp(sensorName,sensors[i].sensorname))
        {
            strcpy(sensors[i].friendlyName, friendlyName);
            return true;
        }
    }
    return false; // not found
}

void temperature_sendall(void)
{
    for (int i = 0; i < tempSensorCnt; i++)
    {
        sendMeasurement(i, sensors[i].lastValid);
    }
}


static void temp_reader(void* arg)
{
    float temperature;
    time_t now;

    for(time_t now = 0; now < MIN_EPOCH; time(&now))
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    for (;;)
    {
        ds18b20_requestTemperatures();
        for (int i = 0; i < tempSensorCnt; i++)
        {
            time(&now);
            temperature = ds18b20_getTempC((DeviceAddress *) sensors[i].addr);
            float diff = fabs(sensors[i].prev - temperature);

            if (temperature < -30.0 || temperature > 95.0 || (sensors[i].prev !=0 && diff > 20.0))
            {
                ESP_LOGI(TAG,"BAD reading from ds18b20 index %d, value %f", i, temperature);
                statistics_getptr()->sensorerrors++;
            }
            else
            {
                sensors[i].lastValid = temperature;
                time(&sensors[i].lastValidTs);
                if ((diff) >= 0.10)
                {
                    sendMeasurement(i, temperature);
                    sensors[i].prev = temperature;
                    sensors[i].prevsend = now;
                    sensors[i].err = 0;
                }
            }
            // Difference was not big enough.
            // Send because of timeout
            if ((now - sensors[i].prevsend) > NO_CHANGE_INTERVAL)
            {
                int err = 0;
                if ((now - sensors[i].lastValidTs) >= NO_CHANGE_INTERVAL)
                {
                    err = 1;
                }
                sendMeasurement(i, sensors[i].lastValid);
                sensors[i].prev = sensors[i].lastValid;
                sensors[i].prevsend = now;
                sensors[i].err = err;
            }
        }
        vTaskDelay(10 * 1000 / portTICK_PERIOD_MS);
    }
}


int temperature_init(int gpio, const char *name, uint8_t *chip, int sensorcnt)
{
    char buff[3];

    chipid = chip;
    myName = name;
    max_sensors = sensorcnt;

    ESP_LOGI(TAG,"my name is %s", myName);
    tempSensors = (DeviceAddress *) malloc(max_sensors * sizeof(DeviceAddress));
    if (tempSensors == NULL)
    {
        ESP_LOGE(TAG,"failed in malloc of tempsensors array");
        return 0;
    }
    memset(tempSensors, 0, max_sensors * sizeof(DeviceAddress));
    ds18b20_init(gpio);
    tempSensorCnt = temp_getaddresses(tempSensors);
    if (!tempSensorCnt) return 0;

    ds18b20_setResolution(tempSensors,tempSensorCnt,12);
    sensors = malloc(sizeof(struct oneWireSensor) * tempSensorCnt);
    if (sensors == NULL) {
        ESP_LOGD(TAG,"malloc failed when allocating sensors");
        return false;
    }
    memset(sensors, 0 ,sizeof(struct oneWireSensor) * tempSensorCnt);

    ESP_LOGI(TAG,"found %d temperature sensors", tempSensorCnt);
    for (int i = 0; i < tempSensorCnt; i++) {
        memcpy(sensors[i].addr,tempSensors[i],sizeof(DeviceAddress));
        sensors[i].prev = 0.0;
        sensors[i].prevsend = 0;
        sensors[i].lastValid = 0;
        sensors[i].sensorname[0]= '\0';
        for (int j = 3; j < 8; j++)  // shorten the name, big name does not fit as a key to nvs flash storage
        {
            sprintf(buff,"%x",tempSensors[i][j]);
            strcat(sensors[i].sensorname, buff);
            strcat(sensors[i].friendlyName, buff);
        }
        ESP_LOGI(TAG,"sensorname index %d = %s done", i, sensors[i].sensorname);
    }
    getFirstTemperatures();
    if (tempSensorCnt)
    {
        xTaskCreate(temp_reader, "temperature reader", 2048, NULL, 10, NULL);
    }
    free(tempSensors);
    return tempSensorCnt;
}