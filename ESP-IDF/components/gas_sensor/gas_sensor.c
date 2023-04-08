#include "gas_sensor.h"
#include "math.h"

#define _X 0
#define _Y 1


static gas_sensor_t sensor;


double GS_calculateSlope(void)
{
    double delta_y = log(sensor.curve.last_data[_Y]/sensor.curve.first_data[_Y]);
    double delta_x = log(sensor.curve.last_data[_X]/sensor.curve.first_data[_X]);
    return (delta_y)/(delta_x);
}


double GS_calculteRo(const adc_channel_t channel)
{
    return GS_GetRs(sensor.pin)/RO_CLEAN_AIR_FACTOR;
}


void GS_Init(gas_sensor_t *gas_sensor_properties)
{
    sensor = *gas_sensor_properties;
    sensor.curve.slope = GS_calculateSlope();
    sensor.ro_calibrated = GS_calculteRo();
}


/***************************  GS_GetRs *******************************************
Input:   mq_pin - analog channel
Output:  Rs of the sensor
Remarks: This function use GS_ResistanceCalculation to caculate the sensor resistenc (Rs).
         The Rs changes as the sensor is in the different consentration of the target
         gas. The sample times and the time interval between samples could be configured
         by changing the definition of the macros.
**********************************************************************************/ 
double GS_getRs(double output_voltage)
{
    return ((sensor.rl_value*((sensor.heater_voltage/output_voltage) - 1)));
}


/***************************  GS_GetPercentage ********************************
Input:   rs_ro_ratio - Rs divided by Ro
         pcurve      - pointer to the curve of the target gas
Output:  ppm of the target gas
Remarks: By using the slope and a point of the line. The x(logarithmic value of ppm) 
         of the line could be derived if y(rs_ro_ratio) is provided. As it is a 
         logarithmic coordinate, power of 10 is used to convert the result to non-logarithmic 
         value.
**********************************************************************************/ 
double GS_getPercentage(double voltage)
{
    double rs_ro_ratio = GS_GetRs(channel_mq4)/Ro_mq4;

    return (pow(10,( ((log(rs_ro_ratio)-pcurve[1])/pcurve[2]) + pcurve[0])));
}
