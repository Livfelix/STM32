#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "stm32f1xx_hal.h"
#define main firmware_main
#include "../../Projeto_Modulo_Portatil_StrainGauge/fw_pio/src/main.c"
#undef main
#include "../../Projeto_Modulo_Portatil_StrainGauge/fw_pio/src/ads1256.c"

GPIO_TypeDef gpio_a,gpio_b,gpio_c;
TIM_TypeDef tim2,tim3;
static uint64_t now, ready_at;
static uint32_t primask;
static uint8_t trace[1000], regs[16], sample[3];
static uint64_t stamp[1000];
static unsigned n, pos, mode, reg, remaining;
static bool fail_spi, stuck_high, stale_low, corrupt_readback;
static uint32_t sim_cal_us=75000;
static uint64_t rdata_at;
static bool saw_high;
static bool auto_clock=true;
static bool phase_clock;
static uint64_t phase_ns;
static GPIO_PinState cs_state=GPIO_PIN_SET;
static uint32_t gpio_modes[3][16], adc_div;
static bool adc_calibrated;
static int priorities[5],app_drdy_calls;

static void advance(uint32_t us){
    now+=us;
#if !SG6_LEGACY
    uint32_t count=tim3.CNT+us;
    if(count>=65536){ tim3.SR=1; tim3.CNT=count&65535; if(!primask) TIM3_IRQHandler(); }
    else tim3.CNT=count;
#endif
}
uint32_t host_counter(TIM_HandleTypeDef*h){
    if(phase_clock){uint64_t old_us=phase_ns/1000;phase_ns+=10;advance((uint32_t)(phase_ns/1000-old_us));}
    else if(auto_clock)advance(1);
    return h->Instance->CNT;
}
uint32_t __get_PRIMASK(void){return primask;}
void __disable_irq(void){primask=1;}
void __set_PRIMASK(uint32_t v){primask=v;
#if !SG6_LEGACY
    if(!v && tim3.SR)TIM3_IRQHandler();
#endif
}
uint32_t HAL_GetTick(void){advance(1000);return (uint32_t)(now/1000);}
void HAL_Delay(uint32_t ms){for(uint32_t i=0;i<ms;i++) advance(1000);}
void HAL_GPIO_WritePin(GPIO_TypeDef*p,uint16_t pin,GPIO_PinState v){if(p==GPIOB && (pin&ADS_CS_PIN)){cs_state=v;if(v==GPIO_PIN_RESET){pos=0;mode=0;}}}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef*p,uint16_t pin){(void)p;(void)pin;advance(1);bool high=stuck_high||(!stale_low && now<ready_at);if(high)saw_high=true;return high?GPIO_PIN_SET:GPIO_PIN_RESET;}
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef*h,uint8_t*tx,uint8_t*rx,uint16_t len,uint32_t timeout){
    (void)h;(void)timeout;assert(len==1); if(fail_spi)return HAL_ERROR;
    assert(n<sizeof trace);trace[n]=*tx;stamp[n++]=now;*rx=0;advance(8);
    if(pos==0){
        if((*tx&0xF0)==0x50){mode=1;reg=*tx&15;}
        else if((*tx&0xF0)==0x10){mode=2;reg=*tx&15;}
        else if(*tx==0x01){mode=3;remaining=0;rdata_at=now;}
        else if(*tx==0xFC){ready_at=now+300;saw_high=false;pos=(unsigned)-1;}
        else if(*tx==0xF0){ready_at=now+sim_cal_us;saw_high=false;pos=(unsigned)-1;}
        else pos=(unsigned)-1;
    }else if(pos>=2&&mode==1){regs[reg++]=*tx;pos=(unsigned)-1;}
    else if(pos>=2&&mode==2){*rx=regs[reg++]^(corrupt_readback?1:0);pos=(unsigned)-1;}
    else if(pos>=1&&mode==3){*rx=sample[remaining++%3];}
    pos++;return HAL_OK;
}
void HAL_GPIO_Init(GPIO_TypeDef*p,GPIO_InitTypeDef*g){int port=p==GPIOA?0:p==GPIOB?1:2;for(unsigned i=0;i<16;i++)if(g->Pin&(1u<<i))gpio_modes[port][i]=g->Mode;}
HAL_StatusTypeDef HAL_Init(void){return HAL_OK;}
#define MOCK_OK(name,type) HAL_StatusTypeDef name(type*p){(void)p;return HAL_OK;}
MOCK_OK(HAL_RCC_OscConfig,RCC_OscInitTypeDef)
HAL_StatusTypeDef HAL_RCCEx_PeriphCLKConfig(RCC_PeriphCLKInitTypeDef*p){adc_div=p->AdcClockSelection;return HAL_OK;}
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef*p){
#if !SG6_LEGACY
    HAL_SPI_MspInit(p);
#else
    (void)p;
#endif
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef*p){
#if !SG6_LEGACY
    HAL_UART_MspInit(p);
#else
    (void)p;
#endif
    return HAL_OK;
}
MOCK_OK(HAL_TIM_PWM_Init,TIM_HandleTypeDef)
MOCK_OK(HAL_TIM_Base_Init,TIM_HandleTypeDef)
MOCK_OK(HAL_TIM_Base_Start_IT,TIM_HandleTypeDef)
MOCK_OK(HAL_ADC_Init,ADC_HandleTypeDef)
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef*p){(void)p;adc_calibrated=true;return HAL_OK;}
HAL_StatusTypeDef HAL_RCC_ClockConfig(RCC_ClkInitTypeDef*p,uint32_t v){(void)p;(void)v;return HAL_OK;}
HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef*p,TIM_OC_InitTypeDef*q,uint32_t v){(void)p;(void)q;(void)v;return HAL_OK;}
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef*p,uint32_t v){(void)p;(void)v;return HAL_OK;}
HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef*p,ADC_ChannelConfTypeDef*q){(void)p;(void)q;return HAL_OK;}
void HAL_NVIC_SetPriority(int a,int b,int c){(void)c;priorities[a]=b;}
void HAL_NVIC_EnableIRQ(int a){(void)a;}
void app_drdy_irq(void){app_drdy_calls++;}
void HAL_IncTick(void){}
void uart_puts(const char*s){(void)s;}
void app_init(void){}
void app_poll(void){}
void gui_init(void){}
void gui_tick(void){}
void lcd_init(void){}

static bool start(uint8_t ch){
#if SG6_LEGACY
    (void)ch;return true;
#else
    return ads_start_channel(ch);
#endif
}
static bool read_value(int32_t*v){
#if SG6_LEGACY
    *v=ads_read_chan(2);return true;
#else
    return ads_read_ready(v);
#endif
}
static int32_t decode_received_24bit(uint8_t a,uint8_t b,uint8_t c){int32_t v=0;sample[0]=a;sample[1]=b;sample[2]=c;assert(start(2));while(now<ready_at)advance(1);assert(read_value(&v));return v;}
int main(int argc,char**argv){
    assert(argc==2);tim3.CNT=0;
#if !SG6_LEGACY
    htim3.Instance=TIM3;
#endif
    if(!strcmp(argv[1],"registers")){
        assert(DR_500==0x92 && DR_1000==0xA1);
        spi_init();assert(hspi2.Init.CLKPhase==SPI_PHASE_2EDGE);assert(hspi2.Init.BaudRatePrescaler==32);
#if !SG6_LEGACY
        clock_init();time_init();gpio_init();uart_init();adc_init();
        assert(priorities[SysTick_IRQn]==0 && priorities[TIM3_IRQn]==1 && priorities[USART1_IRQn]==2 && priorities[EXTI15_10_IRQn]==3);
        assert(gpio_modes[1][10]==GPIO_MODE_IT_FALLING);
        EXTI15_10_IRQHandler();assert(app_drdy_calls==1);
        assert(adc_div==6 && adc_calibrated && gpio_modes[1][0]==GPIO_MODE_ANALOG);
        assert(gpio_modes[0][5]==GPIO_MODE_AF_PP && gpio_modes[0][6]==GPIO_MODE_INPUT && gpio_modes[0][7]==GPIO_MODE_AF_PP);
        assert(gpio_modes[1][13]==GPIO_MODE_AF_PP && gpio_modes[1][14]==GPIO_MODE_INPUT && gpio_modes[1][15]==GPIO_MODE_AF_PP);
        assert(gpio_modes[0][9]==GPIO_MODE_AF_PP && huart1.Init.Mode==UART_MODE_TX);
        sim_cal_us=2000;assert(ads_init());assert((regs[0]&0x0E)==2);assert(regs[2]==0);assert(regs[3]==DR_2000);
        n=0;assert(!ads_set_drate(0x90));assert(n==0);
        corrupt_readback=true;assert(!ads_set_drate(DR_500));corrupt_readback=false;
        fail_spi=true;assert(!ads_start_channel(0));assert(!ads_read_ready(&(int32_t){123}));
#endif
    }else if(!strcmp(argv[1],"mux")){
        int32_t v;assert(start(2));while(now<ready_at)advance(1);assert(read_value(&v));
        assert(n>=9 && trace[0]==0x51 && trace[1]==0 && trace[2]==0x28 && trace[3]==0xFC && trace[4]==0 && trace[5]==1);
        assert(saw_high);assert(stamp[6]-rdata_at>=7);
        assert(stamp[4]-stamp[3]>=12); /*8us SPI command + >=4us t11. */
#if !SG6_LEGACY
        n=0;stale_low=true;assert(!ads_start_channel(2));assert(!ads_read_ready(&v));assert(!ads_start_channel(8));
#endif
    }else if(!strcmp(argv[1],"signed")){
        assert(decode_received_24bit(0xFF,0xFF,0xFF)==-1);
        assert(decode_received_24bit(0x80,0,0)==-8388608);
        int32_t value=123;assert(start(2));stuck_high=true;assert(!read_value(&value));assert(value==123);
#if !SG6_LEGACY
        stuck_high=false;assert(start(2));assert(!ads_read_ready(&value));assert(value==123);
        while(now<ready_at)advance(1);assert(ads_read_ready(&value));assert(!ads_read_ready(&value));assert(!ads_read_ready(NULL));
        assert(start(2));while(now<ready_at)advance(1);value=123;fail_spi=true;assert(!ads_read_ready(&value));assert(value==123 && cs_state==GPIO_PIN_SET);fail_spi=false;
        assert(start(2));advance(20000);value=123;assert(!ads_read_ready(&value));assert(value==123);
#endif
    }else if(!strcmp(argv[1],"cal")){
#if SG6_LEGACY
        ads_set_drate(DR_2_5);uint64_t t=now;ads_selfcal();assert(now-t>=75000);
#else
        assert(ads_set_drate(DR_2_5));assert(now>=75000);
        sim_cal_us=1700000;assert(ads_selfcal());
        stuck_high=true;uint64_t t=now;assert(!ads_selfcal());assert(now-t>=1700000);
#endif
    }else if(!strcmp(argv[1],"phase")){
#if !SG6_LEGACY
        /* First snapshot at960ns: nearly a full timer tick has already elapsed. */
        phase_clock=true;phase_ns=950;
        delay_us(7);assert(phase_ns-950>=6511); /* t6=50/7.68MHz. */
        now=0;tim3.CNT=0;phase_ns=950;
        delay_us(4);assert(phase_ns-950>=3125); /* t11=24/7.68MHz. */
#else
        assert(!"command delay absent");
#endif
    }else if(!strcmp(argv[1],"clock")){
#if SG6_LEGACY
        assert(!"64-bit microsecond clock absent");
#else
        auto_clock=false;tim3.CNT=65534;uint64_t before=app_time_us();primask=1;advance(4);uint64_t pending=app_time_us();assert(pending==before+4 && primask==1);TIM3_IRQHandler();uint64_t after=app_time_us();assert(after==pending);primask=0;
        for(int i=0;i<70000;i++)advance(65536);assert(app_time_us()>UINT32_MAX);
#endif
    }else assert(!"unknown case");
    puts("PASS");return 0;
}

