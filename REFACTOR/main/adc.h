#ifndef __ADC_H__
#define __ADC_H__

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define ADC1_TGS_CHANNEL          ADC_CHANNEL_4
#define ADC1_MQ4_CHANNEL          ADC_CHANNEL_5

extern adc_oneshot_unit_handle_t adc1_handle;
extern adc_cali_handle_t adc1_cali_handle;

void adc_init(void);

#endif