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

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling
#define GPIO_BIT_MASK  ((1ULL<<GPIO_NUM_14))

#define         MQ_PIN                       (12)    //define which analog input channel you are going to use
#define         RL_VALUE                     (5)     //define the load resistance on the board, in kilo ohms
#define         RO_CLEAN_AIR_FACTOR          (9.83)  //RO_CLEAR_AIR_FACTOR=(Sensor resistance in clean air)/RO,
                                                     //which is derived from the chart in datasheet
 
/**********************Software Related Macros***********************************/
#define         CALIBARAION_SAMPLE_TIMES     (50)    //define how many samples you are going to take in the calibration phase
#define         CALIBRATION_SAMPLE_INTERVAL  (500)   //define the time interal(in milisecond) between each samples in the
                                                     //cablibration phase
#define         READ_SAMPLE_INTERVAL         (50)    //define how many samples you are going to take in normal operation
#define         READ_SAMPLE_TIMES            (5)     //define the time interal(in milisecond) between each samples in 

const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
 
/*********************Application Related Macros*********************************/
#define         GAS_LPG                      (0)
#define         GAS_CO                       (1)
#define         GAS_SMOKE                    (2)
#define         GAS_ALCOHOL                  (3)
#define         GAS_METHANE                  (4)
#define         GAS_TOLUENE                  (5)

 
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
float           Ro           =  10;                 //Ro is initialized to 10 kilo ohms

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel_mq4 = ADC_CHANNEL_5;     //GPIO13 if ADC1, GPIO12 if ADC2
static const adc_channel_t channel_mq135 = ADC_CHANNEL_7;     //GPIO35 if ADC1, GPIO27 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_2;

static const char *TAG = "example";


void adc_init(void);
void spiffs_init(void);
void pins_init(void);
uint32_t analogRead(const adc_channel_t);
float MQCalibration(const adc_channel_t channel);
float MQResistanceCalculation(uint32_t adc);
float MQRead(const adc_channel_t);
double MQGetGasPercentage(float rs_ro_ratio, int gas_id);
double MQGetPercentage(float rs_ro_ratio, float *pcurve);
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
    double toluene, methane;
    char buffer[512];

    pins_init();
    adc_init();
    spiffs_init();

    float Ro_mq4 = MQCalibration(channel_mq4);
    float Ro_mq135 = MQCalibration(channel_mq135);

    printf("Ro = %f\r\n", Ro);

    escreve_sensor_arquivo("MQ4(metano),MQ135(tolueno)");

    //Continuously sample ADC1
    while (1) {
        methane = MQGetGasPercentage(MQRead(channel_mq4)/Ro_mq4,GAS_METHANE);
        toluene = MQGetGasPercentage(MQRead(channel_mq135)/Ro_mq135,GAS_TOLUENE);

        sprintf(buffer, "%f,%f\r\n", methane, toluene);
        
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
        adc1_config_channel_atten(channel_mq135, atten);
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


/**************** MQResistanceCalculation **************************************
Input:   raw_adc - raw value read from adc, which represents the voltage
Output:  the calculated sensor resistance
Remarks: The sensor and the load resistor forms a voltage divider. Given the voltage
         across the load resistor and its resistance, the resistance of the sensor
         could be derived.
**********************************************************************************/ 
float MQResistanceCalculation(uint32_t adc)
{
  return ( ((float)RL_VALUE*((5000/adc) - 1)) );
}


/*************************** MQCalibration **************************************
Input:   mq_pin - analog channel
Output:  Ro of the sensor
Remarks: This function assumes that the sensor is in clean air. It use  
         MQResistanceCalculation to calculates the sensor resistance in clean air 
         and then divides it with RO_CLEAN_AIR_FACTOR. RO_CLEAN_AIR_FACTOR is about 
         10, which differs slightly between different sensors.
**********************************************************************************/ 
float MQCalibration(const adc_channel_t channel)
{
  int i;
  float val=0;
 
  for (i=0;i<CALIBARAION_SAMPLE_TIMES;i++) {            //take multiple samples
    val += MQResistanceCalculation(analogRead(channel));
    vTaskDelay(CALIBRATION_SAMPLE_INTERVAL/portTICK_RATE_MS);
  }
  val = val/CALIBARAION_SAMPLE_TIMES;                   //calculate the average value
 
  val = val/RO_CLEAN_AIR_FACTOR;                        //divided by RO_CLEAN_AIR_FACTOR yields the Ro 
                                                        //according to the chart in the datasheet 
 
  return val; 
}


/***************************  MQRead *******************************************
Input:   mq_pin - analog channel
Output:  Rs of the sensor
Remarks: This function use MQResistanceCalculation to caculate the sensor resistenc (Rs).
         The Rs changes as the sensor is in the different consentration of the target
         gas. The sample times and the time interval between samples could be configured
         by changing the definition of the macros.
**********************************************************************************/ 
float MQRead(const adc_channel_t channel)
{
  int i;
  float rs=0;
 
  for (i=0;i<READ_SAMPLE_TIMES;i++) {
    rs += MQResistanceCalculation(analogRead(channel));
    vTaskDelay(READ_SAMPLE_INTERVAL/portTICK_PERIOD_MS);
  }
 
  rs = rs/READ_SAMPLE_TIMES;
 
  return rs;  
}
 

/***************************  MQGetGasPercentage ********************************
Input:   rs_ro_ratio - Rs divided by Ro
         gas_id      - target gas type
Output:  ppm of the target gas
Remarks: This function passes different curves to the MQGetPercentage function which 
         calculates the ppm (parts per million) of the target gas.
**********************************************************************************/ 
double MQGetGasPercentage(float rs_ro_ratio, int gas_id)
{
  if ( gas_id == GAS_LPG ) {
     return MQGetPercentage(rs_ro_ratio,LPGCurve);
  } else if ( gas_id == GAS_CO ) {
     return MQGetPercentage(rs_ro_ratio,COCurve);
  } else if ( gas_id == GAS_SMOKE ) {
     return MQGetPercentage(rs_ro_ratio,SmokeCurve);
  }else if( gas_id == GAS_ALCOHOL ) {
     return MQGetPercentage(rs_ro_ratio,AlcoholCurve);
  }else if( gas_id == GAS_METHANE ) {
     return MQGetPercentage(rs_ro_ratio,MethaneCurve);
  }else if( gas_id == GAS_TOLUENE ) {
     return MQGetPercentage(rs_ro_ratio,TolueneCurve);
  }
  return 0;
}
 

/***************************  MQGetPercentage ********************************
Input:   rs_ro_ratio - Rs divided by Ro
         pcurve      - pointer to the curve of the target gas
Output:  ppm of the target gas
Remarks: By using the slope and a point of the line. The x(logarithmic value of ppm) 
         of the line could be derived if y(rs_ro_ratio) is provided. As it is a 
         logarithmic coordinate, power of 10 is used to convert the result to non-logarithmic 
         value.
**********************************************************************************/ 
double MQGetPercentage(float rs_ro_ratio, float *pcurve)
{
  return (pow(10,( ((log(rs_ro_ratio)-pcurve[1])/pcurve[2]) + pcurve[0])));
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