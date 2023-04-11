#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "soc/soc_caps.h"
#include "esp_log.h"

#include "adc.h"

#include "ssd1306.h"
#include "font8x8_basic.h"
#include <string.h>

#define SAMPLE_TIMES    20

const static char *TAG = "MAIN";
static QueueHandle_t queue_buff;

/*-- ADC General --*/
void adc_task(void*p);
static int adc_raw;
static int voltage;

/*-- SSD General --*/
void ssd_init(void);
void ssd_task(void* p);
SSD1306_t dev;


void app_main(void)
{
    adc_init();
    ssd_init();

    queue_buff = xQueueCreate(5, sizeof(int));

    xTaskCreate(ssd_task, "ssd_task", 2048 * 8, NULL, 5, NULL);
    xTaskCreate(adc_task, "adc_task", 2048 * 8, NULL, 10, NULL);

}

void adc_task(void* p)
{
    while (1) {
        int sensor_voltage = 0;
        for(uint8_t i = 0; i < SAMPLE_TIMES; i++){
            ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN0, &adc_raw));
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage));
            sensor_voltage += voltage;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        sensor_voltage /= SAMPLE_TIMES;

        xQueueSend(queue_buff, (void*)&sensor_voltage, 10);

        ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", 
            ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, sensor_voltage
        );

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


void ssd_init(void)
{
    i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
#if CONFIG_FLIP
	dev._flip = true;
	ESP_LOGW(TAG, "Flip upside down");
#endif
	ESP_LOGI(TAG, "Panel is 128x64");
	ssd1306_init(&dev, 128, 64);

    ssd1306_clear_screen(&dev, false);
    ssd1306_contrast(&dev, 0xff);
    ssd1306_display_text_x3(&dev, 0, "Hello", 5, false);
    vTaskDelay(3000 / portTICK_PERIOD_MS);

}

void ssd_task(void* p)
{
    char lineChar[400];
    int print_data = 0;

	// Scroll Down
	ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);
	ssd1306_display_text(&dev, 0, "--METHANE PPM--", 15, true);
	ssd1306_software_scroll(&dev, 1, (dev._pages - 1) );
	for (int line=0;;line++) {
        if(xQueueReceive(queue_buff, (void*)&print_data, portMAX_DELAY)){
            if(line > 9) line = 0;
            lineChar[0] = 0x02;
            sprintf(&lineChar[1], "%d: %d", line, print_data);
            ssd1306_scroll_text(&dev, lineChar, strlen(lineChar), false);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
	}
}