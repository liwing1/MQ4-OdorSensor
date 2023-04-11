#ifndef __GAS_SENSOR_H__
#define __GAS_SENSOR_H__

#include <stdint.h>

typedef struct{
    uint32_t pin;
    uint32_t rl_value;
    uint32_t heater_voltage;
    double r0_clean_air;
    double r0_calibrated;
    
    struct CURVE{
        double first_data[2];
        double last_data[2];
        double slope;
    } curve;
} gas_sensor_t;


void GS_Init(gas_sensor_t *gas_sensor);
double GS_getRs(double output_voltage);
double GS_getPercentage(double voltage);

#endif