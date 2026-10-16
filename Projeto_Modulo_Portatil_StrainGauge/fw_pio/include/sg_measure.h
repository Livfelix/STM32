#ifndef SG_MEASURE_H
#define SG_MEASURE_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { QUARTER, HALF_BENDING } BridgeType;
float sg_adc_voltage(int32_t code);
bool sg_adc_buffer_valid(int32_t code,float avdd);
bool sg_strain(BridgeType type, float delta_ratio, float gf, float scale, float *ue);
bool sg_shunt_reference(BridgeType type, float gauge_ohm, float shunt_ohm, float gf, float *ue);
/* Bias is measured with equal INA inputs, before the onboard 100/10k attenuation. */
bool sg_bridge_ratio(int32_t code, float gain, float vexc, float bias, float *ratio, float *vout);
/* Provisional interval at the first/last ADC codes accepted by the live voltage guards. */
bool sg_strain_range(BridgeType type,float gain,float vexc,float bias,float zero,float gf,float scale,float *low,float *high);
#endif
