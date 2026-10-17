#include "app_cfg.h"
#include "app.h"
void sd_recording_mode(bool fast){(void)fast;}
uint32_t sd_max_busy_ms(void){return 0;}
void sd_clear_busy_timeout(void){}
bool sd_busy_timed_out(void){return false;}
#include "ads1256.h"
#include "ff.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
void touch_calib_get(int16_t *p){p[0]=300;p[1]=320;p[2]=3800;p[3]=3800;}
int touch_calib_valid(const int16_t *p){return p[0]!=p[2] && p[1]!=p[3];}
ADC_HandleTypeDef hadc1;
static USART_TypeDef uart;
UART_HandleTypeDef huart1={.Instance=&uart};
static int irq_enabled,simulate_ui;
void HAL_NVIC_DisableIRQ(int i){(void)i;irq_enabled=0;}
void HAL_NVIC_EnableIRQ(int i){(void)i;irq_enabled=1;}
void HAL_NVIC_ClearPendingIRQ(int i){(void)i;}
static uint8_t selected;
static float exc=5, signal_ratio=0, baseline_ratio=0.0001f;
static int equal_inputs=0, adc_failure=0, saturated=0;
static float raw_override=-1;
static uint64_t now;
static uint8_t written[46];
static unsigned writes;
static char metadata[1600];
static size_t metadata_len;
uint64_t app_time_us(void){ return now+=100; }
uint32_t HAL_GetTick(void){return (uint32_t)(now/1000);}
void HAL_Delay(uint32_t ms){for(uint32_t i=0;i<ms;i++){now+=1000;if(simulate_ui && irq_enabled)app_drdy_irq();}}
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef*h){(void)h;return HAL_OK;}
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef*h){(void)h;return HAL_OK;}
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef*h,uint32_t ms){(void)h;(void)ms;return HAL_OK;}
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef*h){(void)h;return 2300;}
bool ads_set_drate(uint8_t d){(void)d;return true;}
bool ads_start_channel(uint8_t ch){selected=ch;return !adc_failure;}
bool ads_read_ready(int32_t*c){
    float vadc=selected==7?exc*VEXC_DIV:(saturated?4.f:1.2f+100*exc*(equal_inputs?0:baseline_ratio+signal_ratio))/1.01f;
    if(selected!=7 && raw_override>=0)vadc=raw_override;
    *c=(int32_t)lroundf(vadc*8388608.f/5.f);return true;
}
FRESULT f_mount(FATFS*a,const TCHAR*b,BYTE c){(void)a;(void)b;(void)c;return FR_OK;}
FRESULT f_open(FIL*a,const TCHAR*b,BYTE c){(void)a;(void)b;return c==FA_READ?FR_NO_FILE:FR_OK;}
FRESULT f_close(FIL*a){(void)a;return FR_OK;}
FRESULT f_sync(FIL*a){(void)a;return FR_OK;}
FRESULT f_write(FIL*a,const void*b,UINT n,UINT*w){
    (void)a;*w=n;
    if(n%46==0 && ((const uint8_t*)b)[0]==0xAA){memcpy(written,(const uint8_t*)b+n-46,46);writes+=n/46;}
    else {assert(metadata_len+n<sizeof metadata);memcpy(metadata+metadata_len,b,n);metadata_len+=n;metadata[metadata_len]=0;}
    return FR_OK;
}
#include "../../Projeto_Modulo_Portatil_StrainGauge/fw_pio/src/app.c"
const uint16_t logo_rgb565[120*60]={0};
void lcd_fill(uint16_t c){(void)c;}
void lcd_pix(uint16_t x,uint16_t y,uint16_t c){(void)x;(void)y;(void)c;}
void lcd_rect(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t c){(void)x;(void)y;(void)w;(void)h;(void)c;}
void lcd_frame(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t c){(void)x;(void)y;(void)w;(void)h;(void)c;}
void lcd_text(uint16_t x,uint16_t y,const char*s,uint16_t fg,uint16_t bg,uint8_t z){(void)x;(void)y;(void)s;(void)fg;(void)bg;(void)z;}
void lcd_bmp_rgb565(uint16_t x,uint16_t y,uint16_t w,uint16_t h,const uint16_t*d){(void)x;(void)y;(void)w;(void)h;(void)d;}
void lcd_backlight(uint8_t b){(void)b;}
int touch_raw(int32_t*x,int32_t*y){(void)x;(void)y;return 0;}
int touch_pressed(void){return 0;}
int touch_read(uint16_t*x,uint16_t*y){(void)x;(void)y;return 0;}
void touch_calib_set(int16_t a,int16_t b,int16_t c,int16_t d){(void)a;(void)b;(void)c;(void)d;}
#include "../../Projeto_Modulo_Portatil_StrainGauge/fw_pio/src/gui.c"
static void close_to(float got,float want,float tol){if(fabsf(got-want)>=tol)fprintf(stderr,"got %g want %g\n",got,want);assert(fabsf(got-want)<tol);}
int main(void){
    app_init(); assert(ch_bias_valid==0 && ch_zero_valid==0 && ch_cal_valid==0);
    assert(ch_bridge[0]==HALF_BENDING && ch_bridge[3]==QUARTER && ch_gain[0]==100);
    cfg.ch_mask=1;app_reapply(); assert(!app_zero(cfg.ch_mask));
    equal_inputs=1; assert(app_measure_bias(0));assert(!*app_last_error());close_to(ch_bias_v[0],1.2f,1e-5f);
    equal_inputs=0;assert(app_zero(cfg.ch_mask));close_to(ch_zero_ratio[0],baseline_ratio,1e-8f);
    float vo,ue; signal_ratio=0.00105f;int32_t code;ads_start_channel(0);ads_read_ready(&code);
    assert(code_ue(code,0,&ue,&vo));close_to(ue,1009.6154f,0.03f);
    exc=4.8f;assert(read_excitation());ads_start_channel(0);ads_read_ready(&code);
    assert(code_ue(code,0,&ue,&vo));close_to(ue,1009.6154f,0.03f);
    /* New shunt scale must be independent of the previous factor. */
    signal_ratio=-350.f/(2*(350.f+2*100000.f));ch_scale[0]=3;
    assert(app_shunt(0,100000));assert(!(ch_cal_valid&1));
    assert(!app_shunt_finish(0));close_to(ch_scale[0],3,1e-6f);
    signal_ratio=0;assert(app_shunt_finish(0));close_to(ch_scale[0],1,0.001f);assert(ch_cal_valid&1);
    assert(!app_shunt(0,100000)); /* No shunt: insufficient response. */
    /* Reject buffer-invalid raw volts transactionally, including the rail monitor. */
    float saved_bias=ch_bias_v[0],saved_zero=ch_zero_ratio[0],saved_scale=ch_scale[0];
    uint8_t saved_bvalid=ch_bias_valid,saved_zvalid=ch_zero_valid,saved_cvalid=ch_cal_valid;
    exc=5;raw_override=3.1f;
    assert(!app_measure_bias(0));assert(!app_zero(cfg.ch_mask));assert(!app_shunt(0,100000));
    assert(ch_bias_v[0]==saved_bias && ch_zero_ratio[0]==saved_zero && ch_scale[0]==saved_scale);
    assert(ch_bias_valid==saved_bvalid && ch_zero_valid==saved_zvalid && ch_cal_valid==saved_cvalid);
    raw_override=-1;assert(read_excitation());float saved_exc=vexc_v;
    exc=6.2f;assert(!read_excitation());assert(vexc_v==saved_exc);
    assert(!app_measure_bias(0));assert(!app_zero(cfg.ch_mask));assert(!app_shunt(0,100000));
    assert(ch_bias_v[0]==saved_bias && ch_zero_ratio[0]==saved_zero && ch_scale[0]==saved_scale);
    assert(ch_bias_valid==saved_bvalid && ch_zero_valid==saved_zvalid && ch_cal_valid==saved_cvalid);
    exc=4.751f;assert(read_excitation());exc=5.249f;assert(read_excitation());
    saved_exc=vexc_v;exc=4.749f;assert(!read_excitation());assert(vexc_v==saved_exc);
    exc=5.251f;assert(!read_excitation());assert(vexc_v==saved_exc);exc=5;assert(read_excitation());
    signal_ratio=0.001f;assert(!app_shunt(0,100000));signal_ratio=0; /* Wrong polarity. */
    for(int id=0;id<4;id++){
        app_apply_preset(id);assert(ch_gain[0]==100 && ch_bridge[0]==HALF_BENDING);
        assert(ch_zero_valid&1);assert(ch_cal_valid&1);close_to(ch_zero_ratio[0],baseline_ratio,1e-8f);
    }
    cfg.ch_mask=1;app_reapply();
    /* Failure leaves the last measured fields and calibration intact. */
    saturated=1;assert(!app_measure_bias(0));assert(!app_zero(cfg.ch_mask));assert(!app_shunt(0,100000));
    close_to(ch_bias_v[0],1.2f,1e-5f);assert(ch_cal_valid&1);saturated=0;
    adc_failure=1;assert(!app_measure_bias(0));assert(ch_cal_valid&1);close_to(ch_bias_v[0],1.2f,1e-5f);
    adc_failure=0;app_reapply();
    equal_inputs=1;assert(app_measure_bias(3));equal_inputs=0;
    cfg.ch_mask=0x09;app_reapply();assert(app_zero(cfg.ch_mask));
    cfg.avg=1;cfg.fc_lp=0;cfg.trig_on=1;cfg.trig_lvl_ue=10;app_reapply();
    while(txh!=txt)USART1_IRQHandler();
    signal_ratio=0.00105f;now=0x100000001ULL;app_rec_toggle();
    app_drdy_irq();app_drdy_irq();app_drdy_irq();uint16_t start=txh;app_poll();
    assert(log_flush()); /* Validate the encoded record after committing the partial batch. */
    assert(writes==1);assert(written[0]==0xAA && written[1]==0x56 && written[43]&0x40);
    assert(strstr(metadata,"#SG6BIN3 ch=09"));
    assert(strstr(metadata,"type=HALF_BENDING gf_f32=40051EB8 gain_f32=42C80000"));
    assert(strstr(metadata,"type=QUARTER gf_f32=40051EB8 gain_f32=42C80000"));
    assert(strstr(metadata,"encoding=IEEE754_HEX_F32\n"));
    assert(metadata_len>=8 && !strcmp(metadata+metadata_len-8,"#ENDHDR\n"));
    assert(written[18]==0xFF && written[21]==0x7F);
    assert(written[42]==0x09 && !(written[26]==0xFF && written[29]==0x7F));
    unsigned published=(txh-start)&(TXB-1);
    char status[112];unsigned status_len=(unsigned)snprintf(status,sizeof status,"#STATUS trigger_uart_lost=0 sd_max_io_us=%lu\n",(unsigned long)app_sd_max_io_us());
    assert(published==46+status_len || published==header_len+46+status_len);
    if(published>46+status_len){for(unsigned j=0;j<header_len;j++)assert(txb[(start+j)&(TXB-1)]==(uint8_t)header[j]);}
    start=(txh-status_len-46)&(TXB-1);for(int j=0;j<46;j++)assert(txb[(start+j)&(TXB-1)]==written[j]);
    for(unsigned j=0;j<status_len;j++)assert(txb[(start+46+j)&(TXB-1)]==(uint8_t)status[j]);
    assert(!app_set_channel_config(0,QUARTER,2.1f,100));assert(!app_zero(cfg.ch_mask));
    app_rec_toggle();assert(app_set_channel_config(0,HALF_BENDING,2.1f,100));
    assert(ch_zero_valid&1);assert(!(ch_cal_valid&1));assert(ch_scale[0]==1);
    assert(app_set_channel_config(0,QUARTER,2.1f,100));assert(!(ch_zero_valid&1));assert(ch_bias_valid&1);
    assert(!app_set_channel_config(0,QUARTER,0,100));
    assert(app_set_channel_config(0,QUARTER,2.1f,200));assert(!(ch_bias_valid&1));
    app_apply_preset(2);assert(ch_gain[0]==200 && ch_bridge[0]==QUARTER);
    /* Execute actual GUI callers: cancel by ZERO/GF, restart, retry, then finish. */
    exc=5;signal_ratio=0;app_init();cfg.ch_mask=1;app_reapply();
    equal_inputs=1;assert(app_measure_bias(0));equal_inputs=0;assert(app_zero(cfg.ch_mask));
    const uint8_t cancel_ids[2]={60,62};
    for(int k=0;k<2;k++){
        signal_ratio=-350.f/(2*(350.f+2*100000.f));handle(61);handle(90);assert(shunt_channel==0);
        signal_ratio=0;handle(cancel_ids[k]);if(cancel_ids[k]==60)handle(90);assert(shunt_channel==-1);
        signal_ratio=-350.f/(2*(350.f+2*100000.f));handle(61);handle(90);assert(shunt_channel==0);
        handle(61);handle(90);assert(shunt_channel==0); /* Return-zero failed: retry remains available. */
        signal_ratio=0;handle(61);handle(90);assert(shunt_channel==-1 && (ch_cal_valid&1));
    }
    cfg.ch_mask=1;cfg.fc_lp=0;cfg.avg=1;app_reapply();metadata_len=0;
    assert(app_rec_start());unsigned before=writes;simulate_ui=1;
    gui_msg("REC","acquiring",300);
    for(unsigned i=0;i<300;i++){HAL_Delay(1);app_poll();gui_tick();}simulate_ui=0;
    assert(recording && writes>before+50);assert(app_rec_stop());
    return 0;
}
