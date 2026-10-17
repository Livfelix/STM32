#include "sg_measure.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static void close_to(float got, float want, float tolerance) { if(fabsf(got-want)>=tolerance) fprintf(stderr,"got %.9g want %.9g tolerance %.9g\n",got,want,tolerance); assert(fabsf(got-want)<tolerance); }
int main(void) {
    float ue, ratio, vo;
    close_to(sg_adc_voltage(4194304), 2.5f, 0.000001f);
    close_to(sg_adc_voltage(-4194304), -2.5f, 0.000001f);
    assert(sg_strain(HALF_BENDING, 0.00105f, 2.1f, 1, &ue)); close_to(ue,1000,0.01f);
    assert(sg_strain(QUARTER, 0.00105f, 2.1f, 1, &ue)); close_to(ue,2000,0.01f);
    assert(sg_shunt_reference(QUARTER,350,100000,2.1f,&ue)); close_to(ue,-1663.755095f,0.01f);
    assert(sg_shunt_reference(HALF_BENDING,350,100000,2.1f,&ue)); close_to(ue,-831.877548f,0.01f);
    assert(!sg_strain(QUARTER,0.001f,0,1,&ue));
    assert(!sg_strain(QUARTER,0.001f,NAN,1,&ue));
    assert(!sg_strain(QUARTER,NAN,2.1f,1,&ue));
    assert(!sg_strain(QUARTER,0.001f,2.1f,0,&ue));
    assert(!sg_strain((BridgeType)2,0.001f,2.1f,1,&ue));
    assert(!sg_strain(QUARTER,0.001f,2.1f,1,0));
    assert(!sg_shunt_reference(QUARTER,350,0,2.1f,&ue));
    assert(!sg_shunt_reference(QUARTER,-350,100000,2.1f,&ue));
    /* 350+350 vs 1k/1k balances; 350+1k vs 1k/1k cannot be tared into headroom. */
    close_to(350.f/700.f-1000.f/2000.f,0,1e-7f);
    assert(fabsf(350.f/1350.f-0.5f)>0.2f);
    /* Equal axial changes cancel in half bending, but remain in quarter. */
    float rg=350.f*(1+2.1f*0.001f);
    assert(sg_strain(HALF_BENDING,rg/(rg+rg)-0.5f,2.1f,1,&ue)); close_to(ue,0,0.01f);
    assert(sg_strain(QUARTER,rg/(350.f+rg)-0.5f,2.1f,1,&ue)); close_to(ue,998.951f,0.1f);
    /* Output B=1.2V, signal 0.525V; scaling only excitation+signal preserves ratio. */
    assert(sg_bridge_ratio(2865416,100,5,1.2f,&ratio,&vo)); close_to(ratio,0.00105f,1e-8f);
    assert(sg_strain(HALF_BENDING,ratio,2.1f,1,&ue)); close_to(ue,1000,0.01f);
    assert(sg_bridge_ratio(2830533,100,4.8f,1.2f,&ratio,&vo)); close_to(ratio,0.00105f,1e-8f);
    /* Buffer AVDD-2 limit is on raw ADC input, separate from INA output window. */
    assert(!sg_bridge_ratio(5200937,100,5,1.2f,&ratio,&vo)); /* Raw3.10, INA3.131V. */
    assert(sg_bridge_ratio(4932502,100,5,1.2f,&ratio,&vo)); /* Raw2.94V. */
    assert(!sg_bridge_ratio(4966056,100,5,1.2f,&ratio,&vo)); /* Raw2.96V. */
    assert(sg_bridge_ratio(4596957,100,4.8f,1.2f,&ratio,&vo)); /* Raw2.74V. */
    assert(!sg_bridge_ratio(4630512,100,4.8f,1.2f,&ratio,&vo)); /* Raw2.76V. */
    assert(!sg_bridge_ratio(2865416,100,4.74f,1.2f,&ratio,&vo));
    assert(!sg_bridge_ratio(2865416,100,5.26f,1.2f,&ratio,&vo));
    assert(!sg_bridge_ratio(2865464,0,5,1.2f,&ratio,&vo));
    assert(!sg_bridge_ratio(2865464,100,0,1.2f,&ratio,&vo));
    assert(!sg_bridge_ratio(2865464,100,NAN,1.2f,&ratio,&vo));
    assert(!sg_bridge_ratio(8388607,100,5,1.2f,&ratio,&vo));
    float low,high,qlo,qhi;
    assert(sg_strain_range(HALF_BENDING,100,5,1.2f,0,2.08f,1,&low,&high));
    close_to(low,(.15f-1.2f)/500*2/2.08f*1e6f,.01f);
    close_to(high,(2.95f*1.01f-1.2f)/500*2/2.08f*1e6f,.01f);
    assert(sg_strain_range(QUARTER,200,5,1.2f,.0001f,2.08f,2,&qlo,&qhi));
    close_to(qlo,(low-4*.0001f/2.08f*1e6f)*2,.02f);
    close_to(qhi,(high-4*.0001f/2.08f*1e6f)*2,.02f);
    assert(sg_strain_range(HALF_BENDING,100,5.25f,1.2f,0,2.08f,1,&low,&high));
    close_to(high,(3.15f-1.2f)/525*2/2.08f*1e6f,.01f); /* INA upper guard dominates. */
    assert(sg_strain_range(HALF_BENDING,100,4.75f,1.2f,0,2.08f,1,&low,&high));
    close_to(high,(2.7f*1.01f-1.2f)/475*2/2.08f*1e6f,.01f); /* ADC buffer guard dominates. */
    assert(!sg_strain_range(HALF_BENDING,100,NAN,1.2f,0,2.08f,1,&low,&high));
    assert(!sg_strain_range(HALF_BENDING,0,5,1.2f,0,2.08f,1,&low,&high));
    assert(!sg_strain_range(HALF_BENDING,100,5,NAN,0,2.08f,1,&low,&high));
    assert(!sg_strain_range(HALF_BENDING,100,5,1.2f,NAN,2.08f,1,&low,&high));
    assert(!sg_strain_range(HALF_BENDING,100,5,1.2f,0,0,1,&low,&high));
    assert(!sg_strain_range(HALF_BENDING,100,5,1.2f,0,2.08f,-1,&low,&high));
    assert(!sg_strain_range(HALF_BENDING,100,5,1.2f,0,2.08f,1,NULL,&high));
    assert(sg_shunt_reference(QUARTER,350,330000,2.1f,&ue));close_to(ue,-504.782815f,.01f);
    assert(!sg_bridge_ratio(-8388608,100,5,1.2f,&ratio,&vo));
    assert(!sg_bridge_ratio(0,100,5,1.2f,&ratio,&vo));
    assert(!sg_bridge_ratio(2865416,100,5,NAN,&ratio,&vo));
    assert(!sg_bridge_ratio(2865416,100,5,1.2f,0,&vo));
    assert(!sg_bridge_ratio(2865416,100,5,1.2f,&ratio,0));
    /* The prohibited bridge drives G100 output beyond available headroom. */
    assert(!sg_bridge_ratio(8388607,100,5,1.2f,&ratio,&vo));
    return 0;
}

