#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "soc/soc_caps.h"
#include "esp_log.h"

#include "adc.h"
#include "gas_sensor.h"

#define SAMPLE_TIMES    20

enum IDX_SENSORS{
    IDX_SENSOR_TGS2611 = 0,
    IDX_SENSOR_MQ4,
    IDX_SENSOR_MAX
};

const static char *TAG = "MAIN";
static QueueHandle_t voltage_queue;

/*-- ADC General --*/
void adc_task(void*p);
static int adc_raw[IDX_SENSOR_MAX];
static int voltage[IDX_SENSOR_MAX];

/*-- GAS General --*/
void gas_task(void* p);
gas_sensor_t gas_sensor[IDX_SENSOR_MAX] = {
    [IDX_SENSOR_TGS2611] = {
        .name = "TGS2611-ETH",
        .analog_pin = ADC1_TGS_CHANNEL,
        .r_load = 6000,
        .v_circuit = 5,
        .rsr0_clean = 8.9,

        .curve.data_1 = {300, 6.1},
        .curve.data_2 = {10000, 1.5},
    },

    [IDX_SENSOR_MQ4] = {
        .name = "MQ4-ETH",
        .analog_pin = ADC1_MQ4_CHANNEL,
        .r_load = 975,
        .v_circuit = 5,
        .rsr0_clean = 4.4,

        .curve.data_1 = {200, 4},
        .curve.data_2 = {10000, 3},
    },
};


void app_main(void)
{
    adc_init();

    voltage_queue = xQueueCreate(5, IDX_SENSOR_MAX*sizeof(int));

    xTaskCreate(adc_task, "adc_task", 2048 * 8, NULL, 10, NULL);
    xTaskCreate(gas_task, "gas_task", 2048 * 8, NULL, 8, NULL);

}

void adc_task(void* p)
{
    while (1) {
        int sensor_voltage[IDX_SENSOR_MAX] = {0};
        for(uint8_t i = 0; i < SAMPLE_TIMES; i++){
            for(uint8_t idx_sens = 0; idx_sens < IDX_SENSOR_MAX; idx_sens++){
                ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, gas_sensor[idx_sens].analog_pin, &adc_raw[idx_sens]));
                ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw[idx_sens], &voltage[idx_sens]));
                sensor_voltage[idx_sens] += voltage[idx_sens];
            }

            vTaskDelay(pdMS_TO_TICKS(5));
        }

        for(uint8_t idx_sens = 0; idx_sens < IDX_SENSOR_MAX; idx_sens++){
            sensor_voltage[idx_sens] /= SAMPLE_TIMES;
            ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", 
                ADC_UNIT_1 + 1, gas_sensor[idx_sens].analog_pin, sensor_voltage[idx_sens]
            );
        }

        xQueueSend(voltage_queue, (void*)&sensor_voltage, 10);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void gas_task(void* p)
{
    int v_out[IDX_SENSOR_MAX];
    double ppm[IDX_SENSOR_MAX];
    if(xQueueReceive(voltage_queue, (void*)&v_out, portMAX_DELAY)){
        for(uint8_t idx_sens = 0; idx_sens < IDX_SENSOR_MAX; idx_sens++){
            GS_Init(&gas_sensor[idx_sens], (double)v_out[idx_sens]/1000);            
        }
    }

    while(1){
        if(xQueueReceive(voltage_queue, (void*)&v_out, portMAX_DELAY)){
            for(uint8_t idx_sens = 0; idx_sens < IDX_SENSOR_MAX; idx_sens++){
                ppm[idx_sens] = GS_voltToPPM(&gas_sensor[idx_sens], (double)v_out[idx_sens]/1000);    
            }

            // xQueueSend(ppm_queue, (void*)&ppm, 100);
            ESP_LOGW(TAG, "ETHANOL: TGS=%lfppm\tMQ4=%lfppm", ppm[IDX_SENSOR_TGS2611], ppm[IDX_SENSOR_MQ4]);
        }
    }
}
