/* DRDY publishes raw scans; DSP, SD and UART are foreground consumers. */
#include "app_cfg.h"
#include "app.h"
#include "ads1256.h"
#include "sg_record.h"
#include "ff.h"
#include "xpt2046.h"
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
extern ADC_HandleTypeDef hadc1;
extern UART_HandleTypeDef huart1;
PresetCfg cfg;
static PresetCfg applied_cfg;
static bool applied_valid;
static int applied_preset;
static bool metadata_pending;
const PresetCfg *app_applied_config(void){return applied_valid?&applied_cfg:NULL;}
int preset_id,n_act=6;
volatile int recording;
float ch_gf[6],ch_scale[6],ch_gain[6],ch_bias_v[6],ch_zero_ratio[6],ch_vout[6];
BridgeType ch_bridge[6];
uint8_t ch_bias_valid,ch_zero_valid,ch_cal_valid,sat_flags;
int32_t ch_ue[6];
float vexc_v=5;
uint16_t batt_mv;
uint32_t rec_ms;
static const PresetCfg P[4]={
 {"QUASE-ESTATICO",0x3F,1,DR_500,500,5,8,0,0,2,0,.5f},
 {"MEDIO",0x3F,1,DR_7500,7500,50,1,0,0,2,0,.5f},
 {"RAPIDO",0x03,1,DR_15000,15000,200,1,1,500,2,0,.5f},
 {"CUSTOM",0x3F,1,DR_7500,7500,50,1,0,500,2,0,.5f}
};
static const uint8_t rate_codes[16]={DR_30000,DR_15000,DR_7500,DR_3750,DR_2000,DR_1000,DR_500,DR_100,DR_60,DR_50,DR_30,DR_25,DR_15,DR_10,DR_5,DR_2_5};
static const float rate_sps[16]={30000,15000,7500,3750,2000,1000,500,100,60,50,30,25,15,10,5,2.5f};
static const uint32_t settle_us[16]={210,250,310,440,680,1180,2180,10180,16840,20180,33510,40180,66840,100180,200180,400180};
static unsigned rate_id;
static uint8_t act[6];
static uint64_t t0_us,rec_start_us,last_sync_us,last_uart_us,last_metadata_us,last_batt_us;
static const char *error="";
static bool fail(const char *message){error=message;return false;}
const char *app_last_error(void){return error;}
#define TXB 2048
static uint8_t txb[TXB];
static volatile uint16_t txh,txt;
static uint32_t uart_drops,uart_seq,sd_seq,trigger_uart_lost;
static bool trigger_status_pending,sd_latency_status_pending;
static uint32_t sd_max_io_us;
uint32_t app_sd_max_io_us(void){return sd_max_io_us;}
void USART1_IRQHandler(void){
    if(__HAL_UART_GET_FLAG(&huart1,UART_FLAG_TXE)){
        if(txh!=txt){huart1.Instance->DR=txb[txt];txt=(txt+1)&(TXB-1);}
        else __HAL_UART_DISABLE_IT(&huart1,UART_IT_TXE);
    }
}
static bool uart_tx(const uint8_t *data,uint16_t n,bool priority){
    uint16_t head=txh;
    if(n+(priority?0:SG_RECORD_SIZE)>((txt-head-1)&(TXB-1))){uart_drops++;return false;}
    for(uint16_t i=0;i<n;i++)txb[(head+i)&(TXB-1)]=data[i];
    __DMB();txh=(head+n)&(TXB-1);
    __HAL_UART_ENABLE_IT(&huart1,UART_IT_TXE);return true;
}
static bool uart_tx_record(const uint8_t packet[SG_RECORD_SIZE],bool priority){return uart_tx(packet,SG_RECORD_SIZE,priority);}
void uart_puts(const char *s){uart_tx((const uint8_t*)s,(uint16_t)strlen(s),false);}
uint32_t app_uart_dropped(void){return uart_drops;}
uint32_t app_uart_trigger_lost(void){return trigger_uart_lost;}
uint8_t app_take_saturation(void){uint8_t flags=sat_flags;sat_flags=0;return flags;}
#define RAW_CAP 128
typedef struct {uint64_t time;int32_t code[6],excitation;uint8_t rec;} RawScan;
static RawScan raw[RAW_CAP],building;
static volatile uint32_t raw_head,raw_tail;
bool app_ui_pressure(void){return raw_head-raw_tail>RAW_CAP/4;}
static volatile bool acquiring;
static volatile uint8_t acquisition_fault;
static uint8_t rr,round_cnt,monitor;
static int32_t excitation_code;
static volatile uint32_t command_time;
static int adc_ok=1;
static void pause_acquisition(void){HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);acquiring=false;}
static bool start_conversion(uint8_t channel){
    command_time=(uint32_t)app_time_us();
    if(ads_start_channel(channel))return true;
    acquiring=false;acquisition_fault=2;return false;
}
static bool prepare_acquisition(void){
    rr=round_cnt=0;monitor=1;raw_head=raw_tail=0;acquisition_fault=0;
    memset(&building,0,sizeof building);
    __HAL_GPIO_EXTI_CLEAR_IT(ADS_DRDY_PIN);HAL_NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    if(!adc_ok || !start_conversion(CH_VEXC))return fail("ADS FAIL");
    return true;
}
static bool resume_acquisition(void){
    if(!applied_valid)return fail("NO APPLIED CONFIG");
    if(!prepare_acquisition())return false;
    acquiring=true;HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);return true;
}
void app_drdy_irq(void){
    if(!acquiring)return;
    int32_t code;
    if(!ads_read_ready(&code)){acquiring=false;acquisition_fault=2;return;}
    if(monitor){excitation_code=code;monitor=0;building.time=app_time_us();start_conversion(act[0]);return;}
    building.code[act[rr]]=code;
    if(++rr==n_act){
        if(raw_head-raw_tail==RAW_CAP){acquiring=false;acquisition_fault=1;return;}
        building.excitation=excitation_code;building.rec=(uint8_t)recording;
        raw[raw_head&(RAW_CAP-1)]=building;__DMB();raw_head++;
        rr=0;
        if(++round_cnt==8){round_cnt=0;monitor=1;start_conversion(CH_VEXC);return;}
        building.time=app_time_us();
    }
    start_conversion(act[rr]);
}
typedef struct {float x1,x2,y1,y2,b0,a1,a2,sum;uint8_t pos;uint16_t count;} Filt;
static Filt F[6];
static float averages[6][128];
static float measured_sps,dsp_sps;
static uint64_t rate_start;
static uint32_t rate_count;
static bool trigger_above;
float app_sps_per_ch(void){
    if(!applied_valid)return 0;
    /* shortcut: command cost estimated at115us; replace with bench-measured cycling. */
    return 1000000.f/((settle_us[rate_id]+115.f)*(n_act+0.125f));
}
float app_measured_sps(void){return measured_sps;}
float app_effective_cutoff(void){
    if(!applied_valid)return 0;
    return applied_cfg.fc_lp>=dsp_sps*.5f?dsp_sps*.45f:applied_cfg.fc_lp;
}
static void filt_set(Filt *f,float fc,float fs){
    memset(f,0,sizeof *f);
    if(fc<=0 || fs<=0){f->b0=1;return;}
    if(fc>=fs*0.5f)fc=fs*0.45f;
    float k=tanf(3.14159265f*fc/fs),n=1/(1+1.41421356f*k+k*k);
    f->b0=k*k*n;f->a1=2*(k*k-1)*n;f->a2=(1-1.41421356f*k+k*k)*n;
}
static float filt_run(Filt *f,float x){
    if(f->b0==1 && f->a1==0 && f->a2==0)return x;
    float y=f->b0*(x+2*f->x1+f->x2)-f->a1*f->y1-f->a2*f->y2;
    f->x2=f->x1;f->x1=x;f->y2=f->y1;f->y1=y;return y;
}
static float moving_average(int ch,float x){
    Filt *f=&F[ch];
    if(f->count==applied_cfg.avg)f->sum-=averages[ch][f->pos];else f->count++;
    averages[ch][f->pos]=x;f->sum+=x;f->pos=(f->pos+1)%applied_cfg.avg;
    return f->sum/f->count;
}
static void reset_filters(void){
    dsp_sps=app_sps_per_ch();for(int i=0;i<6;i++)filt_set(&F[i],applied_cfg.fc_lp,dsp_sps);
    trigger_above=false;measured_sps=0;rate_start=0;rate_count=0;
}
static bool code_ue(int32_t c,int i,float *ue,float *vout){
    float ratio;*vout=sg_adc_voltage(c)*1.01f;
    if(!(ch_zero_valid&(1<<i)))return false;
    if(!sg_bridge_ratio(c,ch_gain[i],vexc_v,ch_bias_v[i],&ratio,vout)){sat_flags|=1<<i;return false;}
    return sg_strain(ch_bridge[i],ratio-ch_zero_ratio[i],ch_gf[i],ch_scale[i],ue);
}
static FATFS fs;
static FIL fil;
static bool fs_ok,fil_open,media_failed,fmt_bin=true;
static uint8_t bin_batch[16*SG_RECORD_SIZE];
static UINT bin_used;
static uint64_t sd_last_time;
static uint8_t stop_status;
static char prefix[4]="ENS",fname[13],header[1536];
static uint32_t duration_s;
static uint8_t brightness=70;
uint8_t app_brightness(void){return brightness;}
bool app_set_brightness(uint8_t percent){
    if(recording)return fail("CONFIG DURING REC");
    if(percent<20 || percent>100)return fail("BRIGHTNESS INVALID");
    brightness=percent;return true;
}
static size_t header_len;
int sd_ready(void){return fs_ok && !media_failed;}
int log_is_bin(void){return fmt_bin;}
void log_set_bin(int b){if(recording){fail("CONFIG DURING REC");return;}fmt_bin=b!=0;}
const char *app_log_filename(void){return fname;}
const char *app_log_prefix(void){return prefix;}
uint32_t app_log_duration(void){return duration_s;}
bool app_set_log_config(const char *p,uint32_t duration){
    if(recording)return fail("CONFIG DURING REC");
    if(!p || !*p || strlen(p)>3)return fail("PREFIX INVALID");
    for(const char *s=p;*s;s++)if(!(*s>='A'&&*s<='Z') && !(*s>='0'&&*s<='9'))return fail("PREFIX INVALID");
    strcpy(prefix,p);duration_s=duration;return true;
}
static bool make_header(bool binary,unsigned version){
    if(!applied_valid)return fail("NO APPLIED CONFIG");
    int n=snprintf(header,sizeof header,"#SG6%s%u ch=%02X pga=1 drate_x10=%lu spsch_x10=%lu fc_x10=%lu avg=%u vexc_mv=%u bias_valid=%02X zero_valid=%02X cal_valid=%02X time=boot_relative_us scan=sequential uart_max_sps=1000 fat_date=placeholder unit=%s inactive=INT32_MAX encoding=IEEE754_HEX_F32\n",
      binary?"BIN":"CSV",version,applied_cfg.ch_mask,(unsigned long)(applied_cfg.drate_sps*10),(unsigned long)(app_sps_per_ch()*10),
      (unsigned long)(applied_cfg.fc_lp*10),applied_cfg.avg,(unsigned)(vexc_v*1000),ch_bias_valid,ch_zero_valid,ch_cal_valid,binary?"0.1ue":"ue");
    if(n<0 || (size_t)n>=sizeof header)return fail("HEADER SIZE");
    header_len=(size_t)n;
    for(int i=0;i<6;i++){
        float values[5]={ch_gf[i],ch_gain[i],ch_bias_v[i],ch_zero_ratio[i],ch_scale[i]};uint32_t bits[5];memcpy(bits,values,sizeof bits);
        n=snprintf(header+header_len,sizeof header-header_len,"#CH%d type=%s gf_f32=%08lX gain_f32=%08lX bias_v_f32=%08lX zero_ratio_f32=%08lX scale_f32=%08lX\n",i,
          ch_bridge[i]==HALF_BENDING?"HALF_BENDING":"QUARTER",(unsigned long)bits[0],(unsigned long)bits[1],(unsigned long)bits[2],(unsigned long)bits[3],(unsigned long)bits[4]);
        if(n<0 || (size_t)n>=sizeof header-header_len)return fail("HEADER SIZE");
    header_len+=(size_t)n;
    }
    n=snprintf(header+header_len,sizeof header-header_len,"#TRIGGER enabled=%u channel=%u level_x10=%ld pre_ms=%lu post_ms=%lu effective_fc_x10=%lu analog_hz=499 analog_q_x1000=742 alias_protection=unverified\n",applied_cfg.trig_on?1u:0u,applied_cfg.trig_channel,(long)(applied_cfg.trig_lvl_ue*10),(unsigned long)(applied_cfg.trig_pre_s*1000),(unsigned long)(applied_cfg.trig_post_s*1000),(unsigned long)(app_effective_cutoff()*10));
    if(n<0 || (size_t)n>=sizeof header-header_len)return fail("HEADER SIZE");
    header_len+=(size_t)n;
    const char *end=binary?"#ENDHDR\n":"#seq;t_us;ch0_ue;ch1_ue;ch2_ue;ch3_ue;ch4_ue;ch5_ue;vexc_mv;battery_mv;mask;flags\n#ENDHDR\n";
    size_t length=strlen(end);if(header_len+length>=sizeof header)return fail("HEADER SIZE");
    memcpy(header+header_len,end,length+1);header_len+=length;return true;
}
static bool refresh_metadata(void){
    if(!make_header(true,2) || !uart_tx((const uint8_t*)header,(uint16_t)header_len,false))return false;
    metadata_pending=false;last_metadata_us=app_time_us();return true;
}
static void config_committed(void){error="";metadata_pending=true;refresh_metadata();}
static void measure_sd_io(uint64_t start){
    uint64_t elapsed=app_time_us()-start;
    uint32_t us=elapsed>UINT32_MAX?UINT32_MAX:(uint32_t)elapsed;
    if(us>sd_max_io_us){sd_max_io_us=us;sd_latency_status_pending=true;}
}
static FRESULT timed_sync(void){
    uint64_t start=app_time_us();FRESULT result=f_sync(&fil);measure_sd_io(start);return result;
}
static bool checked_write(const void *data,UINT length){
    sd_clear_busy_timeout();
    uint64_t start=app_time_us();UINT written;FRESULT result=f_write(&fil,data,length,&written);measure_sd_io(start);
    if(result==FR_OK && written==length)return true;
    media_failed=true;pause_acquisition();return fail(recording && applied_preset==2 && sd_busy_timed_out()?"SD SLOW / INCOMPLETE":"SD WRITE / INCOMPLETE");
}
static bool log_flush(void){
    if(!bin_used)return true;
    UINT length=bin_used;bin_used=0; /* A failed/short write must never be retried and duplicated. */
    return checked_write(bin_batch,length);
}
static bool log_close(bool clean){
    if(!fil_open)return true;
    bool ok=!media_failed && log_flush();bin_used=0;
    if(ok && clean && fmt_bin){
        uint8_t footer[SG_FOOTER_SIZE];
        sg_encode_footer(footer,sd_seq,sd_seq?sd_seq-1:UINT32_MAX,sd_last_time,stop_status);
        ok=checked_write(footer,sizeof footer);
    }
    sd_clear_busy_timeout();
    if(timed_sync()!=FR_OK){if(!media_failed)fail(recording && applied_preset==2 && sd_busy_timed_out()?"SD SLOW / INCOMPLETE":"SD SYNC / INCOMPLETE");media_failed=true;ok=false;}
    if(f_close(&fil)!=FR_OK){if(!media_failed)fail("SD CLOSE / INCOMPLETE");media_failed=true;ok=false;}
    fil_open=false;return ok;
}
static bool log_open(void){
    if(!fs_ok)return fail("SD UNAVAILABLE");
    media_failed=false;
    FRESULT result=FR_EXIST;
    uint64_t search_start=app_time_us();
    for(uint32_t seq=0;seq<100000;seq++){
        if(app_time_us()-search_start>=2000000)return fail("SD NAME SEARCH TIMEOUT");
        snprintf(fname,sizeof fname,"%s%05lu.%s",prefix,(unsigned long)seq,fmt_bin?"SGB":"CSV");
        result=f_open(&fil,fname,FA_CREATE_NEW|FA_WRITE);if(result!=FR_EXIST)break;
    }
    if(result!=FR_OK)return fail(result==FR_EXIST?"SD NAMES EXHAUSTED":"SD OPEN FAIL");
    fil_open=true;bin_used=0;
    if(!make_header(fmt_bin,fmt_bin?3:2)||!checked_write(header,(UINT)header_len)){log_close(false);return false;}return true;
}
static bool log_write(uint64_t time,const int32_t values[6],uint16_t exc_mv,uint8_t flags){
    if(fmt_bin){sg_encode_record(bin_batch+bin_used,sd_seq,time,values,exc_mv,batt_mv,applied_cfg.ch_mask,flags);bin_used+=SG_RECORD_SIZE;if(bin_used==sizeof bin_batch && !log_flush())return false;sd_last_time=time;}
    else{
        /* newlib-nano does not format %llu; keep the full boot-relative timestamp. */
        char timestamp[21],*digits=timestamp+20;*digits=0;
        do{*--digits=(char)('0'+time%10);time/=10;}while(time);
        char line[256];int n=snprintf(line,sizeof line,"%lu;%s",(unsigned long)sd_seq,digits);
        for(int i=0;i<6;i++){
            int32_t v=values[i];
            if(v==INT32_MAX)n+=snprintf(line+n,sizeof line-(size_t)n,";2147483647");
            else{int64_t magnitude=v<0?-(int64_t)v:v;n+=snprintf(line+n,sizeof line-(size_t)n,";%s%ld.%u",v<0?"-":"",(long)(magnitude/10),(unsigned)(magnitude%10));}
        }
        n+=snprintf(line+n,sizeof line-(size_t)n,";%u;%u;%u;%u\n",exc_mv,batt_mv,applied_cfg.ch_mask,flags);
        if(n<0 || (size_t)n>=sizeof line)return fail("CSV SIZE");
        if(!checked_write(line,(UINT)n))return false;
    }
    sd_seq++;return true;
}
static bool battery_low,battery_warn,warning_low,battery_invalid,battery_valid,stop_requested;
static uint64_t low_since,warn_since,invalid_since;
bool app_battery_warning(void){return battery_warn;}
bool app_battery_valid(void){return battery_valid;}
uint8_t app_battery_percent(void){
    static const uint16_t mv[]={3350,3500,3650,3750,3850,3950,4050,4150};
    static const uint8_t percent[]={0,10,20,40,60,75,90,100};
    if(!battery_valid || batt_mv<=mv[0])return 0;
    for(unsigned i=1;i<sizeof mv/sizeof *mv;i++)if(batt_mv<mv[i])
        return (uint8_t)(percent[i-1]+(batt_mv-mv[i-1])*(percent[i]-percent[i-1])/(mv[i]-mv[i-1]));
    return 100;
}
static void batt_poll(void){
    bool valid=HAL_ADC_Start(&hadc1)==HAL_OK;uint32_t value=0;
    if(valid){
        valid=HAL_ADC_PollForConversion(&hadc1,10)==HAL_OK;if(valid)value=HAL_ADC_GetValue(&hadc1);
        if(HAL_ADC_Stop(&hadc1)!=HAL_OK)valid=false;
    }
    uint64_t time=app_time_us();battery_valid=valid && value<=4095;
    if(!battery_valid){
        batt_mv=0;
        if(!battery_invalid){battery_invalid=true;invalid_since=time;}
        if(recording && time-invalid_since>=500000){stop_requested=true;stop_status=BAT_MONITOR_FAIL;fail("BAT MONITOR FAIL");}
        return;
    }
    battery_invalid=false;batt_mv=(uint16_t)(value*6600/4095);
    if(batt_mv<3500){if(!warning_low){warning_low=true;warn_since=time;}battery_warn=time-warn_since>=500000;}
    else warning_low=battery_warn=false;
    if(batt_mv>=3350){battery_low=false;return;}
    if(!battery_low){battery_low=true;low_since=time;}
    if(recording && time-low_since>=500000){stop_requested=true;stop_status=LOW_BATTERY;fail("BATERIA BAIXA");}
}
static void process_scan(const RawScan *scan){
    int32_t values[6];uint8_t flags=acquisition_fault?SG_FLAG_INCOMPLETE:0;
    float exc=sg_adc_voltage(scan->excitation)/VEXC_DIV;bool excitation_valid=sg_adc_buffer_valid(scan->excitation,exc);
    if(excitation_valid)vexc_v=exc;else flags|=SG_FLAG_INCOMPLETE;
    for(int i=0;i<6;i++){
        values[i]=INT32_MAX;if(!(applied_cfg.ch_mask&(1<<i))){ch_ue[i]=INT32_MAX;continue;}
        float ue,vo=sg_adc_voltage(scan->code[i])*1.01f;bool valid=excitation_valid && code_ue(scan->code[i],i,&ue,&vo);ch_vout[i]=vo;
        if(valid){float result=moving_average(i,filt_run(&F[i],ue))*10;if(isfinite(result) && result<2147483520.f && result>-2147483520.f)values[i]=(int32_t)lroundf(result);}
        if(values[i]==INT32_MAX){flags|=SG_FLAG_INCOMPLETE;filt_set(&F[i],applied_cfg.fc_lp,dsp_sps);}
        if(excitation_valid && (!sg_adc_buffer_valid(scan->code[i],exc) || vo<=0.15f || vo>=3.15f)){flags|=1<<i;sat_flags|=1<<i;}
        ch_ue[i]=values[i];
    }
    uint8_t trigger=applied_cfg.trig_channel;
    if(applied_cfg.trig_on && (applied_cfg.ch_mask&(1<<trigger)) && values[trigger]!=INT32_MAX){bool above=values[trigger]/10.f>=applied_cfg.trig_lvl_ue;if(above && !trigger_above)flags|=SG_FLAG_TRIGGER;trigger_above=above;}else trigger_above=false;
    uint64_t time=scan->time-t0_us;uint16_t exc_mv=excitation_valid?(uint16_t)(vexc_v*1000):0;
    if(scan->rec && fil_open && !media_failed)log_write(time,values,exc_mv,flags);
    uint64_t uart_time=app_time_us();
    bool priority=(flags&SG_FLAG_TRIGGER)!=0;
    if(priority || uart_time-last_uart_us>=1000){
        bool accepted=false;
        if(!metadata_pending || refresh_metadata()){
            uint8_t packet[SG_RECORD_SIZE];sg_encode_record(packet,uart_seq,time,values,exc_mv,batt_mv,applied_cfg.ch_mask,flags);
            accepted=uart_tx_record(packet,priority);
        }
        if(accepted){uart_seq++;last_uart_us=uart_time;}
        else if(priority){trigger_uart_lost++;trigger_status_pending=true;}
    }
    if(!rate_count)rate_start=scan->time;
    rate_count++;
    if(scan->time-rate_start>=500000){
        measured_sps=(rate_count-1)*1000000.f/(float)(scan->time-rate_start);
        if(fabsf(measured_sps-dsp_sps)>dsp_sps*0.1f){dsp_sps=measured_sps;for(int i=0;i<6;i++)filt_set(&F[i],applied_cfg.fc_lp,dsp_sps);}
        rate_start=scan->time;rate_count=1;
    }
}
static void drain_scans(void){while(raw_tail!=raw_head){__DMB();RawScan scan=raw[raw_tail&(RAW_CAP-1)];__DMB();raw_tail++;process_scan(&scan);}}
bool app_rec_start(void){
    if(recording)return fail("ALREADY REC");
    if(!applied_valid)return fail("NO APPLIED CONFIG");
    if(!adc_ok)return fail("ADS FAIL");
    if(applied_preset==2 && !log_is_bin())return fail("BIN REQUIRED / RAPIDO");
    if((ch_bias_valid&applied_cfg.ch_mask)!=applied_cfg.ch_mask || (ch_zero_valid&applied_cfg.ch_mask)!=applied_cfg.ch_mask)return fail("BIAS/ZERO REQUIRED");
    pause_acquisition();error="";sd_latency_status_pending=sd_max_io_us!=0;sd_max_io_us=0;if(!log_open()){resume_acquisition();return false;}
    rec_start_us=last_sync_us=app_time_us();rec_ms=sd_seq=0;stop_requested=battery_low=battery_invalid=false;trigger_above=false;recording=1;
    sd_last_time=0;stop_status=STOP_NORMAL;
    sd_recording_mode(applied_preset==2);
    if(!resume_acquisition()){recording=0;log_close(false);sd_recording_mode(false);return false;}return true;
}
bool app_rec_stop(void){
    if(!recording)return fail("NOT REC");
    if(acquisition_fault && !media_failed)fail(acquisition_fault==1?"QUEUE OVERFLOW / INCOMPLETE":"ADS FAIL / INCOMPLETE");
    pause_acquisition();drain_scans();
    bool ok=!media_failed && !acquisition_fault,closed=log_close(ok);recording=0;stop_requested=false;
    sd_recording_mode(false);
    if(!acquisition_fault && !media_failed)ok=resume_acquisition()&&ok;
    return ok&&closed;
}
void app_rec_toggle(void){if(recording)app_rec_stop();else app_rec_start();}
void app_poll(void){
    uint64_t time=app_time_us();
    /* Signed delta ignores a newer ISR timestamp and is valid across the32bit wrap. */
    if(acquiring && (int32_t)((uint32_t)time-command_time)>(int32_t)(settle_us[rate_id]*2+10000)){pause_acquisition();acquisition_fault=2;}
    if(time-last_batt_us>=50000){last_batt_us=time;batt_poll();}
    if(recording){rec_ms=(uint32_t)((time-rec_start_us)/1000);if(!stop_requested && duration_s && time-rec_start_us>=(uint64_t)duration_s*1000000){stop_requested=true;stop_status=DURATION;}}
    if(acquisition_fault && !media_failed){pause_acquisition();fail(acquisition_fault==1?"QUEUE OVERFLOW / INCOMPLETE":"ADS FAIL / INCOMPLETE");}
    if(recording && (stop_requested || acquisition_fault || media_failed))app_rec_stop();else drain_scans();
    if(recording && media_failed)app_rec_stop();
    if(recording && time-last_sync_us>=1000000){
        bool ok=log_flush();
        if(ok){
            sd_clear_busy_timeout();
            if(timed_sync()!=FR_OK){media_failed=true;ok=fail(applied_preset==2 && sd_busy_timed_out()?"SD SLOW / INCOMPLETE":"SD SYNC / INCOMPLETE");}
        }
        if(!ok)app_rec_stop();
        last_sync_us=time;
    }
    if(metadata_pending || time-last_metadata_us>=2000000)refresh_metadata();
    if(trigger_status_pending || sd_latency_status_pending){
        char status[112];int n=snprintf(status,sizeof status,"#STATUS trigger_uart_lost=%lu sd_max_io_us=%lu\n",(unsigned long)trigger_uart_lost,(unsigned long)sd_max_io_us);
        if(n>0 && (size_t)n<sizeof status && uart_tx((const uint8_t*)status,(uint16_t)n,false))trigger_status_pending=sd_latency_status_pending=false;
    }
}
bool app_apply_preset(int id){
    if(recording)return fail("CONFIG DURING REC");
    if(id<0||id>3)return fail("PRESET INVALID");
    cfg=P[id];preset_id=id;return app_reapply();
}
bool app_reapply(void){
    if(recording)return fail("CONFIG DURING REC");
    PresetCfg next=cfg;
    if(!next.ch_mask || (next.ch_mask&~0x3F) || !next.avg || next.avg>128 || (next.avg&(next.avg-1)) || !isfinite(next.fc_lp) || next.fc_lp<0 || next.fc_lp>500 || !isfinite(next.trig_lvl_ue) || fabsf(next.trig_lvl_ue)>1000000 || next.trig_channel>=6 || next.trig_on>1 || !isfinite(next.trig_pre_s) || next.trig_pre_s<0 || next.trig_pre_s>10 || !isfinite(next.trig_post_s) || next.trig_post_s<.1f || next.trig_post_s>60)return fail("CONFIG INVALID");
    const float cutoffs[]={0,1,5,10,20,50,100,200,500};
    unsigned cut;for(cut=0;cut<9;cut++)if(next.fc_lp==cutoffs[cut])break;
    if(cut==9)return fail("FILTER INVALID");
    unsigned id;for(id=0;id<16;id++)if(rate_codes[id]==next.drate_code)break;if(id==16)return fail("DRATE INVALID");
    pause_acquisition();applied_valid=false;adc_ok=ads_set_drate(next.drate_code);
    if(!adc_ok)return fail("ADS CONFIG FAIL");
    if(!prepare_acquisition())return false;
    next.drate_sps=rate_sps[id];next.pga=1;
    next.nome=P[preset_id>=0 && preset_id<4?preset_id:3].nome;
    applied_cfg=cfg=next;applied_preset=preset_id;rate_id=id;n_act=0;
    for(int i=0;i<6;i++)if(applied_cfg.ch_mask&(1<<i))act[n_act++]=(uint8_t)i;
    applied_valid=true;reset_filters();
    config_committed();acquiring=true;HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);return true;
}
static int shunt_channel=-1;
static float shunt_scale;
static int read_channel(uint8_t channel,int32_t *value){
    if(!ads_start_channel(channel))return 0;
    uint64_t start=app_time_us();
    while(!ads_read_ready(value))if(app_time_us()-start>2500000)return 0;
    return 1;
}
static bool read_excitation(void){
    int32_t code;if(!read_channel(CH_VEXC,&code))return false;float value=sg_adc_voltage(code)/VEXC_DIV;
    if(!sg_adc_buffer_valid(code,value))return false;
    vexc_v=value;return true;
}
static bool measure_ratio(uint8_t channel,float bias,float *mean){
    pause_acquisition();bool ok=read_excitation();float sum=0;
    for(int j=0;ok && j<16;j++){int32_t code;float ratio,vo;ok=read_channel(channel,&code)&&sg_bridge_ratio(code,ch_gain[channel],vexc_v,bias,&ratio,&vo);if(ok){sum+=ratio;HAL_Delay(3);}}
    bool resumed=resume_acquisition();if(!ok || !resumed)return fail("CAL MEASUREMENT FAIL");*mean=sum/16;return true;
}
bool app_measure_bias(uint8_t channel){
    if(recording || channel>=6)return fail("CAL BLOCKED");
    float ratio;if(!measure_ratio(channel,0,&ratio))return false;
    ch_bias_v[channel]=ratio*ch_gain[channel]*vexc_v;ch_bias_valid|=1<<channel;ch_zero_valid&=~(1<<channel);ch_cal_valid&=~(1<<channel);ch_scale[channel]=1;shunt_channel=-1;config_committed();return true;
}
bool app_zero(uint8_t mask){
    if(recording || !mask || (mask&~0x3F) || (ch_bias_valid&mask)!=mask)return fail("ZERO REQUIRES BIAS");
    float zeros[6];memcpy(zeros,ch_zero_ratio,sizeof zeros);for(int i=0;i<6;i++)if((mask&(1<<i)) && !measure_ratio(i,ch_bias_v[i],&zeros[i]))return false;
    memcpy(ch_zero_ratio,zeros,sizeof zeros);ch_zero_valid|=mask;shunt_channel=-1;for(int i=0;i<6;i++)if(mask&(1<<i)){ch_ue[i]=0;filt_set(&F[i],applied_cfg.fc_lp,app_sps_per_ch());}config_committed();return true;
}
bool app_shunt(uint8_t channel,float shunt_ohm){
    if(recording || channel>=6 || !(ch_zero_valid&(1<<channel)))return fail("SHUNT REQUIRES ZERO");
    float reference,ratio,measured;
    if(!sg_shunt_reference(ch_bridge[channel],350,shunt_ohm,ch_gf[channel],&reference) || !measure_ratio(channel,ch_bias_v[channel],&ratio) || !sg_strain(ch_bridge[channel],ratio-ch_zero_ratio[channel],ch_gf[channel],1,&measured) || measured>=-50)return fail("SHUNT MEASUREMENT FAIL");
    float scale=reference/measured;if(!isfinite(scale)||scale<=0)return fail("SHUNT SCALE INVALID");shunt_scale=scale;shunt_channel=channel;return true;
}
bool app_shunt_finish(uint8_t channel){
    if(recording || channel>=6 || shunt_channel!=channel)return fail("SHUNT NOT PENDING");
    float ratio,ue;
    if(!measure_ratio(channel,ch_bias_v[channel],&ratio) || !sg_strain(ch_bridge[channel],ratio-ch_zero_ratio[channel],ch_gf[channel],1,&ue) || fabsf(ue)>50)return fail("SHUNT RETURN ZERO FAIL");
    ch_scale[channel]=shunt_scale;ch_cal_valid|=1<<channel;shunt_channel=-1;config_committed();return true;
}
bool app_shunt_pending(uint8_t channel){return channel<6 && shunt_channel==channel;}
bool app_set_channel_config(uint8_t channel,BridgeType type,float gf,float gain){
    if(recording || channel>=6 || (type!=QUARTER && type!=HALF_BENDING) || !isfinite(gf) || gf<=0 || !isfinite(gain) || gain<2 || gain>1000)return fail("CHANNEL CONFIG INVALID");
    uint8_t bit=1<<channel;if(ch_gain[channel]!=gain){ch_bias_valid&=~bit;ch_zero_valid&=~bit;}if(ch_bridge[channel]!=type)ch_zero_valid&=~bit;
    if(ch_bridge[channel]!=type || ch_gf[channel]!=gf || ch_gain[channel]!=gain){ch_cal_valid&=~bit;ch_scale[channel]=1;shunt_channel=-1;filt_set(&F[channel],applied_cfg.fc_lp,app_sps_per_ch());}
    ch_bridge[channel]=type;ch_gf[channel]=gf;ch_gain[channel]=gain;config_committed();return true;
}
#ifndef APP_CFG_PAGE0
#define APP_CFG_PAGE0 0x0800F800u
#define APP_CFG_PAGE1 0x0800FC00u
#endif
/* Numeric schema only; no pointers. CRC precedes the last programmed commit halfword. */
typedef struct {
    uint32_t magic,generation;
    uint16_t version,bytes;
    uint8_t preset,mask,drate,avg,trigger,trigger_channel,bias_valid,zero_valid,cal_valid,binary,brightness;
    char prefix[4];
    int16_t touch[4];
    uint8_t type[6];
    uint32_t duration;
    float cutoff,trigger_level,trigger_pre,trigger_post,gf[6],gain[6],bias[6],zero[6],scale[6];
    uint16_t crc,commit;
} ConfigImage;
_Static_assert(sizeof(ConfigImage)<=1024 && offsetof(ConfigImage,commit)==offsetof(ConfigImage,crc)+2,"config page schema");
#define CONFIG_MAGIC 0x32434753u
static PresetCfg image_profile(const ConfigImage *p){
    PresetCfg next=P[p->preset<4?p->preset:0];
    next.ch_mask=p->mask;next.drate_code=p->drate;next.avg=p->avg;next.fc_lp=p->cutoff;
    next.trig_on=p->trigger;next.trig_lvl_ue=p->trigger_level;next.trig_channel=p->trigger_channel;next.trig_pre_s=p->trigger_pre;next.trig_post_s=p->trigger_post;return next;
}
static bool image_valid(const ConfigImage *p,bool committed){
    if(p->magic!=CONFIG_MAGIC || p->version!=2 || p->bytes!=sizeof *p || (committed && p->commit!=0xA55A) || p->crc!=sg_crc16((const uint8_t*)p,offsetof(ConfigImage,crc)))return false;
    if(p->preset>3 || !p->mask || (p->mask&~63) || !p->avg || p->avg>128 || (p->avg&(p->avg-1)) || p->trigger>1 || p->trigger_channel>=6 || p->binary>1 || ((p->bias_valid|p->zero_valid|p->cal_valid)&~63) || (p->zero_valid&~p->bias_valid) || (p->cal_valid&~p->zero_valid) || !touch_calib_valid(p->touch))return false;
    if(!isfinite(p->trigger_level) || fabsf(p->trigger_level)>1000000 || !isfinite(p->trigger_pre) || p->trigger_pre<0 || p->trigger_pre>10 || !isfinite(p->trigger_post) || p->trigger_post<.1f || p->trigger_post>60 || p->brightness<20 || p->brightness>100)return false;
    unsigned r;for(r=0;r<16;r++)if(rate_codes[r]==p->drate)break;if(r==16)return false;
    const float cuts[]={0,1,5,10,20,50,100,200,500};for(r=0;r<9;r++)if(p->cutoff==cuts[r])break;if(r==9)return false;
    unsigned length=0;while(length<4 && p->prefix[length]){char c=p->prefix[length++];if(!(c>='A'&&c<='Z') && !(c>='0'&&c<='9'))return false;}if(!length || length==4)return false;
    for(int i=0;i<6;i++)if(p->type[i]>HALF_BENDING || !isfinite(p->gf[i]) || p->gf[i]<=0 || !isfinite(p->gain[i]) || p->gain[i]<2 || p->gain[i]>1000 || !isfinite(p->bias[i]) || !isfinite(p->zero[i]) || !isfinite(p->scale[i]) || p->scale[i]<=0)return false;
    return true;
}
static const ConfigImage *latest_image(void){
    const ConfigImage *a=(const ConfigImage*)APP_CFG_PAGE0,*b=(const ConfigImage*)APP_CFG_PAGE1;
    bool va=image_valid(a,true),vb=image_valid(b,true);
    if(!va)return vb?b:NULL;
    return vb && (int32_t)(b->generation-a->generation)>0?b:a;
}
bool app_save_cfg(void){
    if(recording)return fail("CONFIG DURING REC");
    if(!applied_valid)return fail("NO APPLIED CONFIG");
    ConfigImage next;memset(&next,0,sizeof next);
    const ConfigImage *old=latest_image();
    next.magic=CONFIG_MAGIC;next.generation=old?old->generation+1:0;next.version=2;next.bytes=sizeof next;
    next.preset=(uint8_t)applied_preset;next.mask=applied_cfg.ch_mask;next.drate=applied_cfg.drate_code;next.avg=applied_cfg.avg;
    next.cutoff=applied_cfg.fc_lp;next.trigger=applied_cfg.trig_on;next.trigger_level=applied_cfg.trig_lvl_ue;next.trigger_channel=applied_cfg.trig_channel;
    next.trigger_pre=applied_cfg.trig_pre_s;next.trigger_post=applied_cfg.trig_post_s;next.brightness=brightness;
    next.bias_valid=ch_bias_valid;next.zero_valid=ch_zero_valid;next.cal_valid=ch_cal_valid;next.binary=fmt_bin;next.duration=duration_s;memcpy(next.prefix,prefix,4);touch_calib_get(next.touch);
    for(int i=0;i<6;i++){next.type[i]=(uint8_t)ch_bridge[i];next.gf[i]=ch_gf[i];next.gain[i]=ch_gain[i];next.bias[i]=ch_bias_v[i];next.zero[i]=ch_zero_ratio[i];next.scale[i]=ch_scale[i];}
    next.crc=sg_crc16((const uint8_t*)&next,offsetof(ConfigImage,crc));next.commit=0xFFFF;
    if(!image_valid(&next,false))return fail("CONFIG INVALID");
    uintptr_t address=old==(const ConfigImage*)APP_CFG_PAGE0?APP_CFG_PAGE1:APP_CFG_PAGE0;
    pause_acquisition();bool ok=HAL_FLASH_Unlock()==HAL_OK;uint32_t page_error=0;
    FLASH_EraseInitTypeDef erase={.TypeErase=FLASH_TYPEERASE_PAGES,.PageAddress=address,.NbPages=1};
    if(ok)ok=HAL_FLASHEx_Erase(&erase,&page_error)==HAL_OK;
    const uint16_t *data=(const uint16_t*)&next;
    for(unsigned offset=0;ok && offset<offsetof(ConfigImage,commit);offset+=2)ok=HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,address+offset,data[offset/2])==HAL_OK;
    if(ok)ok=!memcmp((const void*)address,&next,offsetof(ConfigImage,commit)) && image_valid((const ConfigImage*)address,false);
    if(ok)ok=HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,address+offsetof(ConfigImage,commit),0xA55A)==HAL_OK && image_valid((const ConfigImage*)address,true);
    bool locked=HAL_FLASH_Lock()==HAL_OK,resumed=resume_acquisition();
    if(!ok || !locked)return fail("FLASH SAVE FAIL");
    if(!resumed)return false;
    error="";return true;
}
bool app_load_cfg(void){
    if(recording)return fail("CONFIG DURING REC");
    const ConfigImage *p=latest_image();if(!p)return fail("CFG INVALID / CALIBRATE");
    PresetCfg draft=cfg;int previous=preset_id;cfg=image_profile(p);preset_id=p->preset;
    if(!app_reapply()){cfg=draft;preset_id=previous;return false;}
    for(int i=0;i<6;i++){
        if(!app_set_channel_config((uint8_t)i,(BridgeType)p->type[i],p->gf[i],p->gain[i]))return false;
        ch_bias_v[i]=p->bias[i];ch_zero_ratio[i]=p->zero[i];ch_scale[i]=p->scale[i];
    }
    ch_bias_valid=p->bias_valid;ch_zero_valid=p->zero_valid;ch_cal_valid=p->cal_valid;shunt_channel=-1;
    fmt_bin=p->binary;memcpy(prefix,p->prefix,4);duration_s=p->duration;brightness=p->brightness;touch_calib_set(p->touch[0],p->touch[1],p->touch[2],p->touch[3]);
    config_committed();error="";return true;
}
void app_init(void){
    sd_recording_mode(false);bin_used=0;
    pause_acquisition();applied_valid=false;raw_head=raw_tail=0;txh=txt=0;uart_drops=uart_seq=sd_seq=trigger_uart_lost=0;trigger_status_pending=sd_latency_status_pending=false;sd_max_io_us=0;recording=0;fil_open=false;media_failed=false;adc_ok=1;error="";
    ch_bias_valid=ch_zero_valid=ch_cal_valid=sat_flags=0;shunt_channel=-1;last_uart_us=last_metadata_us=last_batt_us=0;battery_low=battery_warn=warning_low=battery_invalid=battery_valid=stop_requested=false;batt_mv=0;
    for(int i=0;i<6;i++){ch_gf[i]=2.08f;ch_scale[i]=1;ch_gain[i]=100;ch_bias_v[i]=ch_zero_ratio[i]=0;ch_bridge[i]=i<3?HALF_BENDING:QUARTER;ch_ue[i]=INT32_MAX;}
    fmt_bin=true;strcpy(prefix,"ENS");duration_s=0;brightness=70;cfg=P[0];preset_id=0;metadata_pending=true;t0_us=0;
    if(!app_load_cfg()){const char *load_error=error;if(app_apply_preset(0))error=load_error;}
    fs_ok=f_mount(&fs,"",1)==FR_OK;
}
