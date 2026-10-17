#include "app_cfg.h"
#include "ads1256.h"

extern SPI_HandleTypeDef hspi2;
static const uint8_t rates[16]={DR_30000,DR_15000,DR_7500,DR_3750,DR_2000,DR_1000,DR_500,DR_100,DR_60,DR_50,DR_30,DR_25,DR_15,DR_10,DR_5,DR_2_5};
/* TI SBAS288K tables13/21, PGA1, CLKIN7.68MHz; timeout = 2x nominal +10ms. */
static const uint32_t settling_us[16]={210,250,310,440,680,1180,2180,10180,16840,20180,33510,40180,66840,100180,200180,400180};
static const uint32_t cal_us[16]={596,696,896,1300,2000,3600,6600,31200,50900,61800,101300,123200,202100,307200,613800,1227200};
static unsigned rate_index=4;
static bool pending;
static uint64_t deadline;

/* One extra counter tick covers the unknown phase of the first snapshot. */
static void delay_us(uint32_t us){uint64_t t=app_time_us();while(app_time_us()-t<=us);}
static void cs(GPIO_PinState v){HAL_GPIO_WritePin(ADS_CS_PORT,ADS_CS_PIN,v);}
static bool xfer(uint8_t tx,uint8_t *rx){return HAL_SPI_TransmitReceive(&hspi2,&tx,rx,1,20)==HAL_OK;}
static bool send(uint8_t tx){uint8_t rx;return xfer(tx,&rx);}
static bool wait_level(GPIO_PinState level,uint32_t timeout){
    uint64_t t=app_time_us();
    while(HAL_GPIO_ReadPin(ADS_DRDY_PORT,ADS_DRDY_PIN)!=level)
        if(app_time_us()-t>=timeout)return false;
    return true;
}
static bool command(uint8_t cmd){cs(GPIO_PIN_RESET);bool ok=send(cmd);cs(GPIO_PIN_SET);delay_us(4);return ok;}
static bool read_reg(uint8_t reg,uint8_t *v){
    cs(GPIO_PIN_RESET);bool ok=send(0x10|reg)&&send(0);
    delay_us(7); /* t6 >=50 CLKIN cycles (6.511us). */
    if(ok)ok=xfer(0,v);
    cs(GPIO_PIN_SET);delay_us(4);return ok;
}
static bool write_reg(uint8_t reg,uint8_t value){
    cs(GPIO_PIN_RESET);bool ok=send(0x50|reg)&&send(0)&&send(value);
    cs(GPIO_PIN_SET);delay_us(4);
    uint8_t actual;
    return ok&&read_reg(reg,&actual)&&((actual&(reg==0?0x0E:0xFF))==value);
}
bool ads_selfcal(void){
    pending=false;
    return command(0xF0)&&wait_level(GPIO_PIN_SET,10000)&&wait_level(GPIO_PIN_RESET,cal_us[rate_index]*2+10000);
}
bool ads_set_drate(uint8_t code){
    pending=false;
    unsigned i;for(i=0;i<16;i++)if(rates[i]==code)break;
    if(i==16 || !write_reg(3,code))return false;
    rate_index=i;return ads_selfcal();
}
bool ads_init(void){
    pending=false;
    HAL_GPIO_WritePin(ADS_RST_PORT,ADS_RST_PIN,GPIO_PIN_RESET);HAL_Delay(2);
    HAL_GPIO_WritePin(ADS_RST_PORT,ADS_RST_PIN,GPIO_PIN_SET);HAL_Delay(10);
    return wait_level(GPIO_PIN_RESET,50000)&&command(0x0F)&&write_reg(0,0x02)&&write_reg(2,0)&&ads_set_drate(DR_2000);
}
bool ads_start_channel(uint8_t channel){
    pending=false;
    if(channel>7)return false;
    /* No readback here: finish MUX/SYNC before the conversion starts. */
    cs(GPIO_PIN_RESET);bool ok=send(0x51)&&send(0)&&send((channel<<4)|8);
    delay_us(4);
    if(ok)ok=send(0xFC);
    delay_us(4); /* t11 SYNC->WAKEUP >=24 CLKIN cycles. */
    if(ok)ok=wait_level(GPIO_PIN_SET,10000)&&send(0x00);
    cs(GPIO_PIN_SET);delay_us(4);
    if(!ok)return false;
    deadline=app_time_us()+settling_us[rate_index]*2+10000;
    pending=true;return true;
}
bool ads_read_ready(int32_t *code){
    if(!code || !pending)return false;
    if(app_time_us()>deadline){pending=false;return false;}
    if(HAL_GPIO_ReadPin(ADS_DRDY_PORT,ADS_DRDY_PIN)==GPIO_PIN_SET)return false;
    pending=false;
    uint8_t b[3];cs(GPIO_PIN_RESET);bool ok=send(0x01);
    delay_us(7);
    for(unsigned i=0;ok&&i<3;i++)ok=xfer(0,&b[i]);
    cs(GPIO_PIN_SET);delay_us(4);
    if(!ok)return false;
    int32_t v=((int32_t)b[0]<<16)|((int32_t)b[1]<<8)|b[2];
    if(v&0x800000)v-=0x1000000;
    *code=v;return true;
}

