#include "sg_measure.h"
#include <math.h>
float sg_adc_voltage(int32_t code) { return code*(5.0f/8388608.0f); }
bool sg_adc_buffer_valid(int32_t code,float avdd) {
    if(!isfinite(avdd) || avdd<4.75f || avdd>5.25f || code<0 || code>=8388607)return false;
    /* ADS1256 BUFEN: raw input <= AVDD-2V; provisional50mV margin needs bench validation. */
    return sg_adc_voltage(code)<=avdd-2.0f-0.05f;
}
bool sg_strain(BridgeType type,float delta_ratio,float gf,float scale,float *ue) {
    if(!ue || (type!=QUARTER && type!=HALF_BENDING) || !isfinite(delta_ratio) ||
       !isfinite(gf) || gf<=0 || !isfinite(scale) || scale<=0) return false;
    float value=(type==QUARTER?4.0f:2.0f)*delta_ratio/gf*1e6f*scale;
    if(!isfinite(value)) return false;
    *ue=value; return true;
}
bool sg_shunt_reference(BridgeType type,float gauge_ohm,float shunt_ohm,float gf,float *ue) {
    if(!isfinite(gauge_ohm) || gauge_ohm<=0 || !isfinite(shunt_ohm) || shunt_ohm<=0) return false;
    /* Rsh/(R+Rsh)-1/2, simplified to avoid cancellation near balance. */
    float delta=-gauge_ohm/(2.0f*(gauge_ohm+2.0f*shunt_ohm));
    return sg_strain(type,delta,gf,1.0f,ue);
}
bool sg_bridge_ratio(int32_t code,float gain,float vexc,float bias,float *ratio,float *vout) {
    if(!ratio || !vout || !isfinite(gain) || gain<=0 || !isfinite(vexc) || vexc<=0 ||
       !isfinite(bias) || !sg_adc_buffer_valid(code,vexc)) return false;
    float v=sg_adc_voltage(code)*1.01f;
    /* shortcut: conservative 0.15..3.15V INA window, verify actual CJMCU rails on bench. */
    if(v<=0.15f || v>=3.15f) return false;
    float r=(v-bias)/gain/vexc;
    if(!isfinite(r)) return false;
    *vout=v; *ratio=r; return true;
}
bool sg_strain_range(BridgeType type,float gain,float vexc,float bias,float zero,float gf,float scale,float *low,float *high) {
    float ratio,vo;
    if(!low || !high || !isfinite(zero) || !sg_bridge_ratio(4194304,gain,vexc,bias,&ratio,&vo))return false;
    /* Search the same contiguous valid ADC window used for live measurements. */
    int32_t a=0,b=4194304;
    while(b-a>1){int32_t m=a+(b-a)/2;if(sg_bridge_ratio(m,gain,vexc,bias,&ratio,&vo))b=m;else a=m;}
    if(!sg_bridge_ratio(b,gain,vexc,bias,&ratio,&vo) || !sg_strain(type,ratio-zero,gf,scale,low))return false;
    a=4194304;b=8388607;
    while(b-a>1){int32_t m=a+(b-a)/2;if(sg_bridge_ratio(m,gain,vexc,bias,&ratio,&vo))a=m;else b=m;}
    return sg_bridge_ratio(a,gain,vexc,bias,&ratio,&vo) && sg_strain(type,ratio-zero,gf,scale,high);
}
