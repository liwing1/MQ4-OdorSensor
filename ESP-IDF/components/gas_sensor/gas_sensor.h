#ifndef __GAS_SENSOR_H__
#define __GAS_SENSOR_H__


typedef struct{
    uint32_t pin;
    uint32_t rl_value;
    uint32_t heater_voltage;
    double r0_clean_air;
    double r0_calibrated;
    
    struct curve{
        double first_data[2];
        double last_data[2];
        double slope;
    };
} gas_sensor_t;


void GS_Init(gas_sensor_t *gas_sensor);
double GS_getRs(const adc_channel_t);
double GS_getPercentage(double rs_ro_ratio, double *pcurve);

#endif