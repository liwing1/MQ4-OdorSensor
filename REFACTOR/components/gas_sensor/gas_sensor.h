#ifndef __GAS_SENSOR_H__
#define __GAS_SENSOR_H__

#include <stdint.h>

#define _X 0
#define _Y 1

typedef struct{
    uint32_t pin;
    uint32_t r_load;
    uint32_t v_heater;
    double rsr0_clean;
    double r0_calibrated;
    
    struct CURVE{
        double data_1[2];
        double data_2[2];
        double slope;
    } curve;
} gas_sensor_t;


void GS_Init(gas_sensor_t *gas_sensor, double v_calibration);
double GS_voltToPPM(gas_sensor_t *sensor, double voltage);

#endif