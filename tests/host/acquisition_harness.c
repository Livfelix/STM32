#include "app_cfg.h"
#include "app.h"
#include "ads1256.h"
#include "ff.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
static int nano_snprintf(char *out,size_t size,const char *format,...){
    /* Target newlib-nano has no %ll; host libc must not hide that limit. */
    assert(!strstr(format,"%ll"));
    va_list args;va_start(args,format);int n=vsnprintf(out,size,format,args);va_end(args);return n;
}
static int16_t touch_points[4]={300,320,3800,3800};
void touch_calib_get(int16_t *p){memcpy(p,touch_points,sizeof touch_points);}
int touch_calib_valid(const int16_t *p){return p[0]!=p[2] && p[1]!=p[3];}
void touch_calib_set(int16_t a,int16_t b,int16_t c,int16_t d){touch_points[0]=a;touch_points[1]=b;touch_points[2]=c;touch_points[3]=d;}
ADC_HandleTypeDef hadc1;
static USART_TypeDef uart;
UART_HandleTypeDef huart1={.Instance=&uart};
static uint64_t now;
static uint8_t selected;
static int in_irq, irq_enabled, ads_failure, invalid_battery, short_write, sync_error, close_error, write_error,start_failure;
static unsigned reads[8], records, syncs, closes, opens, occupied;
static int open_error;
static uint32_t open_delay_us, write_delay_us, sync_delay_us;
static uint32_t battery_raw=2300;
static uint8_t data[6000][46];
static char metadata[2200], csv_row[256], name[16];
static size_t metadata_len;
static bool sd_fast;
static uint32_t max_busy;
static bool busy_timeout, write_timeout, sync_timeout;
static unsigned write_busy_ms;
void sd_clear_busy_timeout(void){busy_timeout=false;}
bool sd_busy_timed_out(void){return busy_timeout;}
static unsigned binary_writes, last_binary_size;
static uint8_t footer_data[21];
static unsigned footers, footer_records, footer_syncs, footer_closes;
static bool footer_error;
void sd_recording_mode(bool fast){sd_fast=fast;if(fast)max_busy=0;}
uint32_t sd_max_busy_ms(void){return max_busy;}
static float sample_ue;
uint64_t app_time_us(void){return now;}
uint32_t HAL_GetTick(void){return (uint32_t)(now/1000);}
void HAL_Delay(uint32_t ms){now+=ms*1000;}
void HAL_NVIC_DisableIRQ(int i){(void)i;irq_enabled=0;}
void HAL_NVIC_EnableIRQ(int i){(void)i;irq_enabled=1;}
void HAL_NVIC_ClearPendingIRQ(int i){(void)i;}
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef*h){(void)h;assert(!in_irq);return invalid_battery==1?HAL_ERROR:HAL_OK;}
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef*h){(void)h;return invalid_battery==3?HAL_ERROR:HAL_OK;}
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef*h,uint32_t t){(void)h;assert(t<=10);return invalid_battery==2?HAL_ERROR:HAL_OK;}
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef*h){(void)h;return battery_raw;}
bool ads_set_drate(uint8_t d){(void)d;assert(!in_irq && !irq_enabled);return !ads_failure;}
bool ads_start_channel(uint8_t c){assert(c!=6);assert(in_irq || !irq_enabled);selected=c;return !ads_failure && !start_failure;}
bool ads_read_ready(int32_t*c){
    if(ads_failure)return false;
    reads[selected]++;
    float voltage=selected==7?5*VEXC_DIV:(1.2f+sample_ue*2.08f*100*5/2000000.f)/1.01f;
    *c=(int32_t)lroundf(voltage*8388608.f/5);return true;
}
FRESULT f_mount(FATFS*a,const TCHAR*b,BYTE c){(void)a;(void)b;(void)c;assert(!in_irq);return FR_OK;}
FRESULT f_open(FIL*a,const TCHAR*b,BYTE c){(void)a;assert(!in_irq);assert(c==(FA_WRITE|FA_CREATE_NEW));strcpy(name,b);opens++;now+=open_delay_us;if(open_error)return FR_DENIED;if(opens<=occupied)return FR_EXIST;metadata_len=0;return FR_OK;}
FRESULT f_sync(FIL*a){(void)a;assert(!in_irq);syncs++;now+=sync_delay_us;if(sync_timeout)busy_timeout=true;return sync_error?FR_DISK_ERR:FR_OK;}
FRESULT f_close(FIL*a){(void)a;assert(!in_irq);closes++;return close_error?FR_DISK_ERR:FR_OK;}
FRESULT f_write(FIL*a,const void*b,UINT n,UINT*w){
    (void)a;assert(!in_irq);now+=write_delay_us;*w=short_write?n-1:n;
    if(write_timeout)busy_timeout=true;
    if(sd_fast && write_busy_ms>max_busy)max_busy=write_busy_ms;
    if(n==21 && ((const uint8_t*)b)[0]==0xAA){memcpy(footer_data,b,n);footers++;footer_records=records;footer_syncs=syncs;footer_closes=closes;if(footer_error)return FR_DISK_ERR;}
    else if(n%46==0 && ((const uint8_t*)b)[0]==0xAA){binary_writes++;last_binary_size=n;for(unsigned i=0;i<n/46;i++){assert(records<6000);memcpy(data[records++],(const uint8_t*)b+i*46,46);}}
    else if(((const char*)b)[0]=='#'){assert(metadata_len+n<sizeof metadata);memcpy(metadata+metadata_len,b,n);metadata_len+=n;metadata[metadata_len]=0;}
    else{assert(n<sizeof csv_row);memcpy(csv_row,b,n);csv_row[n]=0;}
    return write_error?FR_DISK_ERR:FR_OK;
}
#define snprintf nano_snprintf
#include "../../Projeto_Modulo_Portatil_StrainGauge/fw_pio/src/app.c"
#undef snprintf
static unsigned accepted_records(void){return records+bin_used/SG_RECORD_SIZE;}
static const uint8_t *record_at(unsigned index){
    assert(index<accepted_records());
    return index<records?data[index]:bin_batch+(index-records)*SG_RECORD_SIZE;
}
static uint32_t u32(const uint8_t*p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t u64(const uint8_t*p){return u32(p)|((uint64_t)u32(p+4)<<32);}
static void drain_uart(void){while(txh!=txt)USART1_IRQHandler();}
static void setup(void){
    app_init();cfg.ch_mask=0x09;cfg.fc_lp=0;cfg.avg=1;assert(app_reapply());
    ch_bias_valid=ch_zero_valid=0x3F;for(int i=0;i<6;i++){ch_bias_v[i]=1.2f;ch_zero_ratio[i]=0;}
    drain_uart();assert(app_rec_start());drain_uart();
}
static void scan(float ue,uint64_t t,int poll){
    sample_ue=ue;now=t;
    /* Monitor conversions occur between scans; drive actual ISR to completed gauges. */
    unsigned channel=act[n_act-1],before=reads[channel];while(reads[channel]==before){assert(irq_enabled);in_irq=1;app_drdy_irq();in_irq=0;}
    if(poll)app_poll();
}
static void trigger_setup(void){
    assert(app_rec_stop());cfg.trig_on=1;cfg.trig_lvl_ue=500;assert(app_reapply());
    assert(app_rec_start());drain_uart();
}
static void uart_record(unsigned index,uint8_t packet[SG_RECORD_SIZE]){
    for(unsigned i=0;i<SG_RECORD_SIZE;i++)packet[i]=txb[(txt+index*SG_RECORD_SIZE+i)&(TXB-1)];
    assert(packet[0]==0xAA && packet[1]==0x56);
    assert(sg_crc16(packet,44)==(uint16_t)(packet[44]|packet[45]<<8));
}
int main(int argc,char**argv){
    assert(argc==2);setup();
    if(!strcmp(argv[1],"footer")){
        assert(strstr(metadata,"#SG6BIN3 "));
        for(unsigned i=0;i<19;i++)scan(1,1000+i*1000,i<16);
        assert(app_rec_stop());
        assert(footers==1 && footer_records==19 && footer_syncs+1==syncs && footer_closes+1==closes);
        assert(footer_data[0]==0xAA && footer_data[1]==0x57 && u32(footer_data+2)==19 && u32(footer_data+6)==18);
        assert(u64(footer_data+10)==u64(data[18]+6) && footer_data[18]==0);
        assert(sg_crc16(footer_data,19)==(uint16_t)(footer_data[19]|footer_data[20]<<8));
        assert(app_rec_start());assert(app_rec_stop());
        assert(footers==2 && u32(footer_data+2)==0 && u32(footer_data+6)==UINT32_MAX && u64(footer_data+10)==0);
    }else if(!strcmp(argv[1],"footer_failures")){
        for(unsigned i=0;i<128;i++)scan(1,i*1000,0);
        scan(1,128000,0);app_poll();assert(!recording && footers==0);
        assert(app_rec_start());short_write=1;scan(1,now+1000,0);assert(!app_rec_stop());assert(footers==0);short_write=0;
        assert(app_rec_start());scan(1,now+1000,0);footer_error=true;assert(!app_rec_stop());
        assert(footers==1 && strstr(app_last_error(),"SD WRITE / INCOMPLETE"));
        app_poll();assert(footers==1);footer_error=false;
        assert(app_rec_start());sync_error=1;scan(1,now+1000000,1);assert(!recording && footers==1);sync_error=0;
        start_failure=1;assert(!app_rec_start());assert(footers==1);
    }else if(!strcmp(argv[1],"pipeline")){
        assert(app_rec_stop());cfg.avg=8;assert(app_reapply());metadata_len=0;assert(app_rec_start());
        for(int i=0;i<16;i++){scan(i*100.f,(i+1)*1000,1);assert(accepted_records()==(unsigned)i+1);assert(!(record_at(i)[43]&0x3F));assert((int32_t)u32(record_at(i)+18)==INT32_MAX);assert(record_at(i)[42]==9);float want=(i<8?i/2.f:i-3.5f)*100;assert(fabsf((int32_t)u32(record_at(i)+14)/10.f-want)<0.3f);}
        assert(app_rec_stop());cfg.avg=1;cfg.trig_on=1;cfg.trig_lvl_ue=500;assert(app_reapply());metadata_len=0;assert(app_rec_start());
        const float v[6]={-1,501,700,700,499,501};unsigned base=accepted_records();
        for(int i=0;i<6;i++){scan(v[i],100000+i*1000,1);assert(!!(record_at(base+i)[43]&SG_FLAG_TRIGGER)==(i==1||i==5));}
        assert(reads[7]>=2 && reads[6]==0);
    }else if(!strcmp(argv[1],"overflow")){
        for(int i=0;i<128;i++)scan(1,i*1000,0);
        scan(1,128000,0);assert(!acquiring);app_poll();assert(!recording && records==128);assert(strstr(app_last_error(),"OVERFLOW"));assert(record_at(127)[43]&SG_FLAG_INCOMPLETE);assert(syncs==1&&closes==1);
    }else if(!strcmp(argv[1],"rate")){
        unsigned sent=0;uint64_t previous=0;
        for(int i=0;i<1200;i++){uint64_t t=(i+1)*1000000ULL/1200;uint16_t h=txh;scan(i%100,t,1);assert(u32(record_at(i)+2)==(uint32_t)i);if(txh!=h){uint16_t start=(txh-46)&(TXB-1);uint8_t packet[46];for(int j=0;j<46;j++)packet[j]=txb[(start+j)&(TXB-1)];if(packet[0]==0xAA){assert(u32(packet+2)==sent++);assert(u64(packet+6)==u64(record_at(i)+6));assert(!memcmp(packet+14,record_at(i)+14,30));assert(sg_crc16(packet,44)==(uint16_t)(packet[44]|packet[45]<<8));assert(u64(packet+6)>=previous+1000 || sent==1);previous=u64(packet+6);}}drain_uart();}
        assert(records==1200 && sent<=1000 && sent>400);assert(app_measured_sps()>1100);
        txt=0;txh=TXB-10;uint16_t before=txh;assert(!uart_tx_record(record_at(0),false));assert(txh==before && app_uart_dropped()>0);
        uint32_t rejected_seq=uart_seq;scan(1,now+2000,1);assert(txh==before && uart_seq==rejected_seq);
        drain_uart();before=txh;scan(1,now+2000,1);if(txh==before)scan(1,now+2000,1);assert(accepted_records()>=1202);uint16_t start=(txh-46)&(TXB-1);uint8_t packet[46];for(int j=0;j<46;j++)packet[j]=txb[(start+j)&(TXB-1)];assert(u32(packet+2)==rejected_seq);
        drain_uart();now+=2000000;scan(1,now,1);assert(recording && txh!=txt);assert(txb[(txh-header_len)&(TXB-1)]=='#');
    }else if(!strcmp(argv[1],"trigger_gate")){
        trigger_setup();scan(0,1000,1);uint16_t before=txh;scan(501,1500,1);
        assert(((txh-before)&(TXB-1))==SG_RECORD_SIZE);
        scan(700,1700,1);scan(0,2000,1);scan(501,2100,1);
        assert(((txh-txt)&(TXB-1))==3*SG_RECORD_SIZE);
        for(unsigned i=0;i<3;i++){uint8_t packet[SG_RECORD_SIZE];uart_record(i,packet);assert(u32(packet+2)==i);assert(u64(packet+6)==(i<2?1000:2000));assert(!!(packet[43]&SG_FLAG_TRIGGER)==(i>0));}
    }else if(!strcmp(argv[1],"trigger_reserve")){
        trigger_setup();
        for(unsigned i=0;i<43;i++)scan(0,(i+1)*1000,1);
        uint16_t before=txh;assert(((txt-txh-1)&(TXB-1))>=SG_RECORD_SIZE && ((txt-txh-1)&(TXB-1))<2*SG_RECORD_SIZE);
        scan(0,44000,1);assert(txh==before && uart_seq==43);
        scan(501,44500,1);assert(((txh-before)&(TXB-1))==SG_RECORD_SIZE && uart_seq==44);
        uint8_t packet[SG_RECORD_SIZE];uart_record(43,packet);assert(u32(packet+2)==43 && (packet[43]&SG_FLAG_TRIGGER));
    }else if(!strcmp(argv[1],"trigger_full")){
        trigger_setup();txt=0;txh=TXB-SG_RECORD_SIZE;uint16_t before=txh;
        scan(501,1000,1);assert(txh==before && uart_seq==0 && app_uart_trigger_lost()==1);
        scan(0,1200,1);scan(501,1300,1);assert(txh==before && uart_seq==0 && app_uart_trigger_lost()==2);
        drain_uart();scan(0,1400,1);scan(501,1500,1);assert(uart_seq==2 && app_uart_trigger_lost()==2);
    }else if(!strcmp(argv[1],"trigger_exact_reserve")){
        uint8_t packet[SG_RECORD_SIZE]={0};
        txt=0;txh=TXB-2*SG_RECORD_SIZE;uint16_t before=txh;
        assert(!uart_tx_record(packet,false) && txh==before);
        txh--;assert(uart_tx_record(packet,false));assert(((txt-txh-1)&(TXB-1))==SG_RECORD_SIZE);
        assert(uart_tx_record(packet,true) && txh==TXB-1);assert(!uart_tx_record(packet,true));
        txt=SG_RECORD_SIZE;assert(uart_tx_record(packet,true) && txh==SG_RECORD_SIZE-1);
    }else if(!strcmp(argv[1],"burst")){
        for(int i=0;i<100;i++)scan(1,(i+1)*1000,0);
        uint16_t before=txh;app_poll();assert(accepted_records()==100);
        unsigned length=(txh-before)&(TXB-1),offset=0,packets=0;
        /* Commit metadata is not a visual frame; still accept at most one46byte record. */
        while(offset<length){
            if(txb[(before+offset)&(TXB-1)]=='#'){
                assert(length-offset>=header_len);for(unsigned j=0;j<header_len;j++)assert(txb[(before+offset+j)&(TXB-1)]==(uint8_t)header[j]);offset+=(unsigned)header_len;
            }else{assert(length-offset>=46);uint8_t packet[46];for(unsigned j=0;j<46;j++)packet[j]=txb[(before+offset+j)&(TXB-1)];assert(packet[0]==0xAA && packet[1]==0x56 && sg_crc16(packet,44)==(uint16_t)(packet[44]|packet[45]<<8));offset+=46;packets++;}
        }assert(packets<=1);
    }else if(!strcmp(argv[1],"config")){
        PresetCfg old=cfg;assert(!app_apply_preset(1));assert(!app_reapply());assert(!app_zero(9));assert(!memcmp(&cfg,&old,sizeof cfg));
        assert(app_rec_stop());assert(!app_zero(0));assert(!app_zero(0x40));assert(!app_set_log_config("long",0));assert(app_set_log_config("AB",2));
        assert(app_apply_preset(1));assert(cfg.drate_sps==7500&&cfg.fc_lp==50&&cfg.avg==1);assert(ch_bias_valid==0x3F);
        cfg.ch_mask=9;cfg.avg=1;cfg.fc_lp=0;assert(app_reapply());metadata_len=0;assert(app_rec_start());ch_scale[0]=1e30f;scan(100,1000,1);assert((int32_t)u32(record_at(0)+14)==INT32_MAX && record_at(0)[43]&SG_FLAG_INCOMPLETE);
    }else if(!strcmp(argv[1],"draftavg") || !strcmp(argv[1],"rejectavg")){
        assert(app_rec_stop());cfg.avg=8;assert(app_reapply());
        for(int i=0;i<8;i++)scan(i*100.f,(i+1)*1000,1);
        assert(fabsf(ch_ue[0]/10.f-350)<0.3f);
        cfg.avg=!strcmp(argv[1],"draftavg")?1:0;
        if(!cfg.avg)assert(!app_reapply());
        const PresetCfg *active=app_applied_config();assert(active && active!=&cfg && active->avg==8);
        scan(800,9000,1);assert(fabsf(ch_ue[0]/10.f-450)<0.3f);
        assert(make_header(true,2));assert(strstr(header,"avg=8 "));
        char draft_name[]="DRAFT";cfg.nome=draft_name;cfg.avg=1;scan(900,10000,0);assert(app_reapply());
        assert(app_applied_config()->nome!=draft_name && app_applied_config()->avg==1);
        scan(1000,11000,1);assert(fabsf(ch_ue[0]/10.f-1000)<0.3f);
    }else if(!strcmp(argv[1],"rejectmask")){
        assert(app_rec_stop());cfg.ch_mask=0;assert(!app_reapply());
        assert(cfg.ch_mask==0 && app_applied_config()->ch_mask==9);
        metadata_len=0;assert(app_rec_start());assert(strstr(metadata,"#SG6BIN3 ch=09 "));
        scan(100,1000,1);assert(record_at(0)[42]==9 && n_act==2 && reads[0] && reads[3]);
        assert((int32_t)u32(record_at(0)+18)==INT32_MAX && (int32_t)u32(record_at(0)+26)!=INT32_MAX);
    }else if(!strcmp(argv[1],"applyfailure")){
        assert(app_rec_stop());cfg.avg=8;start_failure=1;assert(!app_reapply());start_failure=0;
        assert(!app_applied_config());
        assert(!app_rec_start() && !recording);
        assert(app_reapply());metadata_len=0;assert(app_rec_start());assert(strstr(metadata,"avg=8 "));
        scan(100,1000,1);assert(fabsf(ch_ue[0]/10.f-100)<0.3f);
    }else if(!strcmp(argv[1],"filter")){
        assert(app_rec_stop());cfg.fc_lp=5;cfg.avg=8;assert(app_reapply());
        float fs=app_sps_per_ch(),k=tanf(3.14159265f*5/fs);
        assert(fabsf(F[0].b0-k*k/(1+1.41421356f*k+k*k))<1e-6f);
        F[0].x1=5;F[0].count=8;cfg.avg=16;assert(app_reapply());
        assert(F[0].x1==0 && F[0].count==0 && app_sps_per_ch()==fs);
        cfg.drate_code=DR_2_5;assert(app_reapply());assert(cfg.drate_sps==2.5f);
        cfg.drate_code=DR_500;cfg.fc_lp=0;cfg.avg=1;assert(app_reapply());metadata_len=0;assert(app_rec_start());
        scan(4000,1000,1);assert((record_at(0)[43]&9)==9);scan(0,2000,1);assert(!(record_at(1)[43]&0x3F));assert(app_take_saturation()==9);assert(app_take_saturation()==0);
    }else if(!strcmp(argv[1],"watchdog")){
        command_time=1000;now=999;app_poll();assert(acquiring && recording);
        for(int i=0;i<10;i++)scan(1,2000+i*1000,0);
        now=50000;app_poll();assert(!recording && records==10 && strstr(app_last_error(),"ADS FAIL"));assert(record_at(9)[43]&SG_FLAG_INCOMPLETE);
        metadata_len=0;now=UINT32_MAX-100ULL;assert(app_rec_start());now+=1100;app_poll();assert(acquiring && recording);
        metadata_len=0;scan(1,now+1000,0);ads_failure=1;in_irq=1;app_drdy_irq();in_irq=0;ads_failure=0;app_poll();assert(!recording && records==11);
    }else if(!strcmp(argv[1],"open")){
        assert(app_rec_stop());metadata_len=0;occupied=opens+2;assert(app_rec_start());assert(!strcmp(name,"ENS00002.SGB"));assert(app_rec_stop());
        open_error=1;assert(!app_rec_start()&&!recording);assert(strstr(app_last_error(),"OPEN"));open_error=0;occupied=1000000;assert(!app_rec_start()&&!recording);assert(strstr(app_last_error(),"NAMES"));occupied=0;short_write=1;assert(!app_rec_start()&&!recording);assert(strstr(app_last_error(),"WRITE"));
        short_write=0;occupied=1000000;open_delay_us=10000;assert(!app_rec_start()&&!recording);assert(strstr(app_last_error(),"NAME SEARCH TIMEOUT"));
    }else if(!strcmp(argv[1],"errors")){
        short_write=1;for(unsigned i=0;i<16;i++)scan(1,(i+1)*1000,1);assert(!recording);assert(strstr(app_last_error(),"WRITE"));short_write=0;metadata_len=0;assert(app_rec_start());sync_error=1;scan(1,1100000,1);assert(!recording && strstr(app_last_error(),"SYNC"));sync_error=0;metadata_len=0;assert(app_rec_start());close_error=1;assert(!app_rec_stop());assert(strstr(app_last_error(),"CLOSE"));
        close_error=0;metadata_len=0;assert(app_rec_start());write_error=1;for(unsigned i=0;i<16;i++)scan(1,1101000+i*1000,1);assert(!recording && strstr(app_last_error(),"WRITE"));
    }else if(!strcmp(argv[1],"stops")){
        assert(app_rec_stop());assert(app_set_log_config("ENS",2));metadata_len=0;now=0;assert(app_rec_start());scan(1,1999000,1);assert(recording);scan(1,2000000,1);assert(!recording&&closes==2&&syncs>=2);app_poll();assert(closes==2);
        assert(footer_data[18]==1);
        assert(app_set_log_config("ENS",0));metadata_len=0;assert(app_rec_start());battery_raw=2072;now=3000000;batt_poll();now+=499000;batt_poll();assert(recording);now+=1000;batt_poll();scan(1,now,1);assert(!recording);
        assert(footer_data[18]==2);
        assert(!strcmp(app_last_error(),"BATERIA BAIXA"));
        metadata_len=0;assert(app_rec_start());now+=1000000;batt_poll();now+=400000;invalid_battery=1;batt_poll();invalid_battery=0;now+=200000;batt_poll();scan(1,now,1);assert(!recording && !strcmp(app_last_error(),"BATERIA BAIXA"));
    }else if(!strcmp(argv[1],"battery_warning")){
        battery_raw=2110;now=0;batt_poll();assert(!app_battery_warning());now=499999;batt_poll();assert(!app_battery_warning());now=500000;batt_poll();assert(app_battery_warning() && recording);
        battery_raw=2172;now++;batt_poll();assert(!app_battery_warning());
        battery_raw=2110;now++;batt_poll();now+=400000;batt_poll();battery_raw=2172;now++;batt_poll();battery_raw=2110;now++;batt_poll();now+=100000;batt_poll();assert(!app_battery_warning());
    }else if(!strcmp(argv[1],"battery_percent")){
        assert(!app_battery_valid() && app_battery_percent()==0);batt_poll();assert(app_battery_valid());
        const uint16_t mv[]={0,3349,3350,3400,3425,3500,3575,3650,3700,3750,3800,3850,3900,3950,4000,4050,4100,4150,4400,65535};
        const uint8_t percent[]={0,0,0,3,5,10,15,20,30,40,50,60,67,75,82,90,95,100,100,100};
        for(unsigned i=0;i<sizeof mv/sizeof *mv;i++){batt_mv=mv[i];assert(app_battery_percent()==percent[i]);}
        invalid_battery=1;batt_poll();assert(!app_battery_valid() && batt_mv==0 && app_battery_percent()==0);invalid_battery=0;batt_poll();assert(app_battery_valid());
    }else if(!strcmp(argv[1],"battery_invalid")){
        for(int failure=1;failure<=4;failure++){
            if(!recording){metadata_len=0;assert(app_rec_start());}
            invalid_battery=failure==4?0:failure;battery_raw=failure==4?4096:2300;
            now+=1000000;batt_poll();assert(recording && batt_mv==0);now+=499999;batt_poll();assert(recording && !stop_requested);now++;batt_poll();scan(1,now,1);assert(!recording && !strcmp(app_last_error(),"BAT MONITOR FAIL"));
            assert(footer_data[18]==3);
            invalid_battery=0;battery_raw=2300;batt_poll();
        }
    }else if(!strcmp(argv[1],"battery_timers")){
        battery_raw=2072;now=0;batt_poll();now=400000;batt_poll();invalid_battery=1;now=500000;batt_poll();assert(!stop_requested);now=999999;batt_poll();assert(!stop_requested);now=1000000;batt_poll();scan(1,now,1);assert(!recording && !strcmp(app_last_error(),"BAT MONITOR FAIL"));
        metadata_len=0;assert(app_rec_start());invalid_battery=1;now+=1000000;batt_poll();now+=400000;batt_poll();invalid_battery=0;battery_raw=2300;now++;batt_poll();invalid_battery=1;now++;batt_poll();now+=100000;batt_poll();assert(!stop_requested);
    }else if(!strcmp(argv[1],"fastcsv")){
        assert(app_rec_stop());assert(app_apply_preset(2));log_set_bin(0);unsigned before=opens;
        assert(!app_rec_start() && !recording && opens==before);assert(strstr(app_last_error(),"BIN REQUIRED"));
        cfg.nome="draft";preset_id=0;assert(!app_rec_start()); /* Applied preset controls admission. */
    }else if(!strcmp(argv[1],"batch")){
        for(unsigned i=0;i<15;i++)scan(1,(i+1)*1000,1);
        assert(records==0 && binary_writes==0);scan(1,16000,1);
        assert(records==16 && binary_writes==1 && last_binary_size==16*46);
        for(unsigned i=16;i<21;i++)scan(1,(i+1)*1000,1);
        assert(records==16);assert(app_rec_stop());assert(records==21 && binary_writes==2 && last_binary_size==5*46);
        for(unsigned i=0;i<records;i++)assert(u32(data[i]+2)==i && sg_crc16(data[i],44)==(uint16_t)(data[i][44]|data[i][45]<<8));
        metadata_len=0;assert(app_rec_start());scan(1,30000,1);assert(app_rec_stop());assert(records==22 && u32(data[21]+2)==0);
    }else if(!strcmp(argv[1],"batch_sync")){
        for(unsigned i=0;i<5;i++)scan(1,996000+i*1000,1);
        assert(records==5 && binary_writes==1 && last_binary_size==5*46 && syncs==1);
        for(unsigned i=0;i<3;i++)scan(1,1001000+i*1000,0);
        assert(app_rec_stop());assert(records==8 && binary_writes==2 && last_binary_size==3*46);
        for(unsigned i=0;i<records;i++)assert(u32(data[i]+2)==i);
    }else if(!strcmp(argv[1],"batch_error")){
        for(unsigned i=0;i<5;i++)scan(1,(i+1)*1000,1);
        ads_failure=1;in_irq=1;app_drdy_irq();in_irq=0;ads_failure=0;app_poll();
        assert(!recording && records==5 && binary_writes==1 && bin_used==0 && strstr(app_last_error(),"ADS FAIL"));
        metadata_len=0;assert(app_rec_start());unsigned before=binary_writes;short_write=1;
        for(unsigned i=0;i<16;i++)scan(1,20000+i*1000,1);
        assert(!recording && binary_writes==before+1 && bin_used==0 && strstr(app_last_error(),"WRITE"));
        app_poll();assert(binary_writes==before+1); /* A partial batch is never retried during close. */
    }else if(!strcmp(argv[1],"slow_sync")){
        assert(app_rec_stop());assert(app_apply_preset(2));assert(app_rec_start() && sd_fast);
        sync_error=close_error=1;sync_timeout=true;max_busy=70;scan(1,1000000,1);
        assert(!recording && !sd_fast && !strcmp(app_last_error(),"SD SLOW / INCOMPLETE"));
    }else if(!strcmp(argv[1],"slow")){
        assert(app_rec_stop());assert(app_apply_preset(2));assert(app_rec_start() && sd_fast);
        write_error=1;write_timeout=true;max_busy=0;
        for(unsigned i=0;i<16;i++)scan(1,(i+1)*1000,1);
        assert(!recording && !sd_fast && !strcmp(app_last_error(),"SD SLOW / INCOMPLETE"));
    }else if(!strcmp(argv[1],"latency")){
        assert(app_rec_stop());assert(app_apply_preset(2));write_delay_us=12345;assert(app_rec_start() && sd_fast);
        assert(app_sd_max_io_us()==12345 && sd_latency_status_pending);drain_uart();
        metadata_pending=false;last_metadata_us=now;uint16_t before=txh;app_poll();char status[112];
        snprintf(status,sizeof status,"#STATUS trigger_uart_lost=0 sd_max_io_us=12345\n");
        assert(((txh-before)&(TXB-1))==strlen(status));
        for(unsigned i=0;i<strlen(status);i++)assert(txb[(before+i)&(TXB-1)]==(uint8_t)status[i]);
        assert(!sd_latency_status_pending);
        write_delay_us=98765;assert(log_flush());
        uint8_t bytes[SG_RECORD_SIZE]={0};assert(checked_write(bytes,sizeof bytes));
        assert(app_sd_max_io_us()==98765 && sd_latency_status_pending);
        /* A complete line needs its own bytes plus the untouched trigger reserve. */
        snprintf(status,sizeof status,"#STATUS trigger_uart_lost=0 sd_max_io_us=98765\n");
        command_time=(uint32_t)now;txt=100;txh=(txt-1-strlen(status)-SG_RECORD_SIZE+1)&(TXB-1);before=txh;
        app_poll();assert(txh==before && sd_latency_status_pending);
        txt=(txt+1)&(TXB-1);app_poll();assert(!sd_latency_status_pending);
        assert(((txt-txh-1)&(TXB-1))==SG_RECORD_SIZE);
        for(unsigned i=0;i<strlen(status);i++)assert(txb[(before+i)&(TXB-1)]==(uint8_t)status[i]);
        drain_uart();sync_delay_us=110001;assert(app_rec_stop());assert(app_sd_max_io_us()==110001);
        write_delay_us=sync_delay_us=0;assert(app_rec_start());assert(app_sd_max_io_us()==0 && sd_latency_status_pending);
        write_delay_us=71000;assert(checked_write(bytes,sizeof bytes));
        write_delay_us=0;write_error=1;assert(!checked_write(bytes,sizeof bytes));
        assert(app_sd_max_io_us()==71000 && !strcmp(app_last_error(),"SD WRITE / INCOMPLETE"));
        app_init();assert(app_sd_max_io_us()==0 && !sd_latency_status_pending);
    }else if(!strcmp(argv[1],"busy_cause")){
        assert(app_rec_stop());assert(app_apply_preset(2));
        for(unsigned failure=0;failure<4;failure++){
            metadata_len=0;assert(app_rec_start());write_busy_ms=70;uint64_t start=now;
            for(unsigned i=0;i<16;i++)scan(1,start+(i+1)*1000,1);
            assert(recording && max_busy==70 && !busy_timeout);write_busy_ms=0;
            if(failure==3){sync_error=1;assert(!app_rec_stop());}
            else if(failure==2){sync_error=1;scan(1,start+1000000,1);}
            else{
                write_error=failure==0;short_write=failure==1;
                for(unsigned i=0;i<16;i++)scan(1,start+17000+i*1000,1);
            }
            assert(!recording && !strcmp(app_last_error(),failure>=2?"SD SYNC / INCOMPLETE":"SD WRITE / INCOMPLETE"));
            write_error=short_write=sync_error=0;
        }
    }else if(!strcmp(argv[1],"csv")){
        assert(app_rec_stop());log_set_bin(0);metadata_len=0;assert(app_rec_start());assert(strstr(metadata,"#SG6CSV2"));assert(strstr(metadata,"gf_f32="));assert(strstr(metadata,"#ENDHDR\n"));assert(strstr(metadata,"ch0_ue"));scan(-1.2f,1234567,1);assert(strstr(csv_row,";-1.2;"));assert(strstr(csv_row,";2147483647;"));
    }else if(!strcmp(argv[1],"csv64")){
        assert(app_rec_stop());log_set_bin(0);assert(app_rec_start());batt_mv=3700;
        const int32_t values[6]={-12,INT32_MAX,0,INT32_MAX,INT32_MAX,INT32_MAX};
        const uint64_t times[3]={0,4294967297ULL,UINT64_MAX};
        const char *expected[3]={
            "0;0;-1.2;2147483647;0.0;2147483647;2147483647;2147483647;5000;3700;9;64\n",
            "0;4294967297;-1.2;2147483647;0.0;2147483647;2147483647;2147483647;5000;3700;9;64\n",
            "0;18446744073709551615;-1.2;2147483647;0.0;2147483647;2147483647;2147483647;5000;3700;9;64\n"};
        for(unsigned i=0;i<3;i++){sd_seq=0;assert(log_write(times[i],values,5000,64));assert(!strcmp(csv_row,expected[i]));}
    }else if(!strcmp(argv[1],"budget")){
        assert(sizeof(RawScan)==40 && sizeof raw==5120);
        printf("RawScan=%zu raw=%zu averages=%zu F=%zu UART=%zu header=%zu header_len=%zu batch=%zu\n",sizeof(RawScan),sizeof raw,sizeof averages,sizeof F,sizeof txb,sizeof header,header_len,sizeof bin_batch);
        puts(header);
    }else assert(!"unknown test");
    puts("PASS");return 0;
}
