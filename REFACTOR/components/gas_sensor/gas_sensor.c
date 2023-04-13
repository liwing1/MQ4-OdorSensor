#include "gas_sensor.h"
#include "math.h"
#include "esp_log.h"

const static char *TAG = "GAS";

static double GS_calculateSlope(double* data_1, double* data_2);
static double GS_calculateB(double* data, double slope);
static double GS_calculateRS(double v_heater, double v_output, double r_load);

static double GS_calculateSlope(double* data_1, double* data_2)
{
    return log10(data_1[_Y]/data_2[_Y])/log10(data_1[_X]/data_2[_X]);
}

static double GS_calculateB(double* data, double slope)
{
    return log10(data[_Y]/pow(data[_X], slope));
}

static double GS_calculateRS(double v_heater, double v_output, double r_load)
{
    return (double)((v_heater/v_output - 1)*r_load);
}

static double GS_calculateR0(double v_heater, double v_output, double r_load, double rsr0_clean)
{
    return GS_calculateRS(v_heater, v_output, r_load)/rsr0_clean;
}

void GS_Init(gas_sensor_t *sensor, double v_calibration)
{
    sensor->curve.slope = GS_calculateSlope(sensor->curve.data_1, sensor->curve.data_2);
    sensor->curve.b = GS_calculateB(sensor->curve.data_1, sensor->curve.slope);
    sensor->r0_calibrated = GS_calculateR0(sensor->v_heater, v_calibration, sensor->r_load, sensor->rsr0_clean);
    ESP_LOGW(TAG, "%s Init\tSLOPE: %lf\tB: %lf\tR0: %lf", 
    sensor->name, sensor->curve.slope, sensor->curve.b, sensor->r0_calibrated);
}

double GS_voltToPPM(gas_sensor_t *sensor, double voltage)
{
    double rs = GS_calculateRS(sensor->v_heater, voltage, sensor->r_load);
    double r0 = sensor->r0_calibrated;
    double rsr0 = rs/r0;
    ESP_LOGW(TAG, "RS: %lf\tR0: %lf\tRSR0:%lf", rs, r0, rsr0);
    return pow(rsr0/pow(10, sensor->curve.b), 1/sensor->curve.slope);
}