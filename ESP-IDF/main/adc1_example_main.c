/* ADC1 Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "gas_sensor.h"
#include "ssd1306.h"
#include "font8x8_basic.h"
#include <string.h>

#define tag "example"

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling
#define GPIO_BIT_MASK  ((1ULL<<GPIO_NUM_14))

#define         GS__PIN                       (GPIO_NUM_2)    //define which analog input channel you are going to use
#define         RL_VALUE                     (5)     //define the load resistance on the board, in kilo ohms
#define         RO_CLEAN_AIR_FACTOR          (9.83)  //RO_CLEAR_AIR_FACTOR=(Sensor resistance in clean air)/RO,
                                                     //which is derived from the chart in datasheet
 


SSD1306_t dev;
float methane;
 
/*********************Application Related Macros*********************************/
enum{
    GAS_LPG = 0,
    GAS_CO,
    GAS_SMOKE,
    GAS_ALCOHOL,
    GAS_METHANE,
    GAS_TOLUENE,
    GAS_TGS_METHANE,
    GAS_TGS_HYDROGEN,
};

 
/****************************Globals**********************************************/
float           LPGCurve[3]  =  {2.3,0.43,-0.32};   //two points are taken from the curve. 
                                                    //with these two points, a line is formed which is "approximately equivalent"
                                                    //to the original curve. 
                                                    //data format:{ x, y, slope}; point1: (lg200, 0.43), point2: (lg10000, -0.12) 
float           COCurve[3]  =  {2.3,0.64,-0.06};    //two points are taken from the curve. 
                                                    //with these two points, a line is formed which is "approximately equivalent" 
                                                    //to the original curve.
                                                    //data format:{ x, y, slope}; point1: (lg200, 0.64), point2: (lg10000,  0.54) 
float           SmokeCurve[3] ={2.3,0.53,-0.44};    //two points are taken from the curve. 
                                                    //with these two points, a line is formed which is "approximately equivalent" 
                                                    //to the original curve.
                                                    //data format:{ x, y, slope}; point1: (lg200, 0.53), point2: (lg10000,  -0.22)                                                                                                         
float           AlcoholCurve[3] ={2.3,0.60,-0.07};  //two points are taken from the curve. 
                                                    //with these two points, a line is formed which is "approximately equivalent" 
                                                    //to the original curve.
                                                    //data format:{ x, y, slope}; point1: (lg200, 0.60), point2: (lg10000,  0.48)                                                     
float           MethaneCurve[3] ={2.3,0.26,-0.36};  //two points are taken from the curve. 
                                                    //with these two points, a line is formed which is "approximately equivalent" 
                                                    //to the original curve.
                                                    //data format:{ x, y, slope}; point1: (lg200, 0.26), point2: (lg10000,    -0.36)                                                     
float           TolueneCurve[3] ={1.0,-0.1343,-0.41};
                                                    //with these two points, a line is formed which is "approximately equivalent" 
                                                    //to the original curve.
                                                    //data format:{ x, y, slope}; point1: (lg10, -0.1343), point2: (lg1000,    -0.95) 
float           TGS_MethaneCurve[3] = {2.47,0.48,-0.43};     //point1: (lg300, lg3), point2: (lg10000,    lg0.65) 
float           TGS_HydrogenCurve[3] = {2.47,0.61,-0.29};    //point1: (lg300, lg4.1), point2: (lg10000,    lg1.5) 
float           Ro           =  10;                 //Ro is initialized to 10 kilo ohms

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel_mq4 = ADC_CHANNEL_2;
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_2;

static const char *TAG = "example";


void adc_init(void);
void spiffs_init(void);
void pins_init(void);
void ssd_init(void);
void ssd_task(void* p);
uint32_t analogRead(const adc_channel_t);
void escreve_sensor_arquivo(char*);
void print_sensor_arquivo(void);


static void check_efuse(void)
{
#if CONFIG_IDF_TARGET_ESP32
    //Check if TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }
    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
#elif CONFIG_IDF_TARGET_ESP32S2
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("Cannot retrieve eFuse Two Point calibration values. Default calibration values will be used.\n");
    }
#else
#error "This example is configured for ESP32/ESP32S2."
#endif
}


static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}


void app_main(void)
{
    char buffer[512];

    ssd_init();
    pins_init();
    adc_init();
    spiffs_init();

    float Ro_mq4 = GS_Calibration(channel_mq4);

    printf("Ro = %f\r\n", Ro);

    escreve_sensor_arquivo("GS_4(metano),GS_135(tolueno)");

    xTaskCreate(ssd_task, "ssd_task", 2048 * 8, NULL, 5, NULL);

    //Continuously sample ADC1
    while (1) {
        methane = GS_GetGasPercentage(GS_Read(channel_mq4)/Ro_mq4,GAS_METHANE);

        sprintf(buffer, "%f\r\n", methane);
        
        escreve_sensor_arquivo(buffer);

        printf("%s", buffer);

        if(gpio_get_level(GPIO_NUM_14) == pdTRUE)
        {
            print_sensor_arquivo();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void adc_init(void)
{
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();

    //Configure ADC
    if (unit == ADC_UNIT_1) {
        adc1_config_width(width);
        adc1_config_channel_atten(channel_mq4, atten);
    } else {
        adc2_config_channel_atten((adc2_channel_t)channel_mq4, atten);
    }

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

}

uint32_t analogRead(const adc_channel_t channel)
{
    uint32_t adc_reading = 0;
    //Multisampling
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        if (unit == ADC_UNIT_1) {
            adc_reading += adc1_get_raw((adc1_channel_t)channel);
        } else {
            int raw;
            adc2_get_raw((adc2_channel_t)channel, width, &raw);
            adc_reading += raw;
        }
    }
    adc_reading /= NO_OF_SAMPLES;
    //Convert adc_reading to voltage in mV
    return (2 * esp_adc_cal_raw_to_voltage(adc_reading, adc_chars));
}


void spiffs_init(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
}


void escreve_sensor_arquivo(char* buffer)
{
    ESP_LOGI(TAG, "Opening file");
    FILE* f = fopen("/spiffs/hello.txt", "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    fprintf(f, buffer);
    fclose(f);
}

void print_sensor_arquivo(void)
{
    char buffer[512];
    FILE* f = fopen("/spiffs/hello.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }

    ESP_LOGW("PRINT", "IMPRIMINDO ARQUIVO");
    while(fgets(buffer, 512, f))
    {
        printf("%s", buffer);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGW("PRINT", "TERMINOU DE IMPRIMIR ARQUIVO");
}

void pins_init(void)
{
    gpio_config_t io_config = {
        .mode           =   GPIO_MODE_INPUT,
        .pull_up_en     =   GPIO_PULLUP_DISABLE,
        .pull_down_en   =   GPIO_PULLDOWN_ENABLE,
        .intr_type      =   GPIO_INTR_DISABLE,
        .pin_bit_mask   =   GPIO_BIT_MASK,
    };
    gpio_config(&io_config);
}

void ssd_init(void)
{
    i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
#if CONFIG_FLIP
	dev._flip = true;
	ESP_LOGW(tag, "Flip upside down");
#endif
	ESP_LOGI(tag, "Panel is 128x64");
	ssd1306_init(&dev, 128, 64);

    ssd1306_clear_screen(&dev, false);
    ssd1306_contrast(&dev, 0xff);
    ssd1306_display_text_x3(&dev, 0, "Hello", 5, false);
    vTaskDelay(3000 / portTICK_PERIOD_MS);

}

void ssd_task(void* p)
{
    char lineChar[400];

	// Scroll Down
	ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);
	ssd1306_display_text(&dev, 0, "--METHANE PPM--", 15, true);
	//ssd1306_software_scroll(&dev, 1, 7);
	ssd1306_software_scroll(&dev, 1, (dev._pages - 1) );
	for (int line=0;;line++) {
		lineChar[0] = 0x02;
		sprintf(&lineChar[1], "%d: %.6f", line, methane);
		ssd1306_scroll_text(&dev, lineChar, strlen(lineChar), false);
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
}