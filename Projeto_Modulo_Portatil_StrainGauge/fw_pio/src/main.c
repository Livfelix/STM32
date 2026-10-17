/* main.c — SG-6CH: inicialização de clocks/periféricos + loop principal */
#include "app_cfg.h"
#include "app.h"
#include "gui.h"
#include "ads1256.h"
#include "ili9341.h"

SPI_HandleTypeDef hspi1, hspi2;
UART_HandleTypeDef huart1;
TIM_HandleTypeDef htim2, htim3;
ADC_HandleTypeDef hadc1;
static volatile uint64_t time_base_us;
static volatile HAL_StatusTypeDef init_fault;

static void require_ok(HAL_StatusTypeDef status){
    if(status!=HAL_OK){ init_fault=status;if(huart1.Instance)uart_puts("#HAL INIT FAIL\r\n");while(1); }
}

uint64_t app_time_us(void){
    /* shortcut: IRQ latency must stay below65.536ms; use a wider timer if longer masking is needed. */
    uint32_t mask=__get_PRIMASK();__disable_irq();
    uint64_t base=time_base_us;
    uint32_t count=__HAL_TIM_GET_COUNTER(&htim3);
    if(__HAL_TIM_GET_FLAG(&htim3,TIM_FLAG_UPDATE)!=RESET){
        base+=65536;count=__HAL_TIM_GET_COUNTER(&htim3);
    }
    __set_PRIMASK(mask);return base+count;
}
void TIM3_IRQHandler(void){
    if(__HAL_TIM_GET_FLAG(&htim3,TIM_FLAG_UPDATE)!=RESET){
        __HAL_TIM_CLEAR_IT(&htim3,TIM_IT_UPDATE);time_base_us+=65536;
    }
}
static void time_init(void){
    __HAL_RCC_TIM3_CLK_ENABLE();htim3.Instance=TIM3;
    htim3.Init.Prescaler=72-1;htim3.Init.CounterMode=TIM_COUNTERMODE_UP;
    htim3.Init.Period=65535;htim3.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
    require_ok(HAL_TIM_Base_Init(&htim3));
    __HAL_TIM_CLEAR_IT(&htim3,TIM_IT_UPDATE);
    HAL_NVIC_SetPriority(TIM3_IRQn,1,0);HAL_NVIC_EnableIRQ(TIM3_IRQn);
    require_ok(HAL_TIM_Base_Start_IT(&htim3));
}

static void clock_init(void){
    RCC_OscInitTypeDef o={0}; RCC_ClkInitTypeDef c={0};
    require_ok(HAL_Init());
    o.OscillatorType=RCC_OSCILLATORTYPE_HSE;
    o.HSEState=RCC_HSE_ON;
    o.HSEPredivValue=RCC_HSE_PREDIV_DIV1;
    o.PLL.PLLState=RCC_PLL_ON;
    o.PLL.PLLSource=RCC_PLLSOURCE_HSE;
    o.PLL.PLLMUL=RCC_PLL_MUL9;                    /* 8 MHz × 9 = 72 MHz */
    require_ok(HAL_RCC_OscConfig(&o));
    c.ClockType=RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource=RCC_SYSCLKSOURCE_PLLCLK;
    c.AHBCLKDivider=RCC_SYSCLK_DIV1;
    c.APB1CLKDivider=RCC_HCLK_DIV2;               /* 36 MHz */
    c.APB2CLKDivider=RCC_HCLK_DIV1;               /* 72 MHz */
    require_ok(HAL_RCC_ClockConfig(&c,FLASH_LATENCY_2));
    RCC_PeriphCLKInitTypeDef p={0};p.PeriphClockSelection=RCC_PERIPHCLK_ADC;
    p.AdcClockSelection=RCC_ADCPCLK2_DIV6; /* 72/6=12MHz <=14MHz. */
    require_ok(HAL_RCCEx_PeriphCLKConfig(&p));
    HAL_NVIC_SetPriority(SysTick_IRQn,0,0);
}

static void gpio_init(void){
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();__HAL_RCC_AFIO_CLK_ENABLE();
    GPIO_InitTypeDef g={0};
    g.Mode=GPIO_MODE_OUTPUT_PP; g.Speed=GPIO_SPEED_FREQ_HIGH; g.Pull=GPIO_NOPULL;
    HAL_GPIO_WritePin(GPIOB,ADS_CS_PIN|ADS_RST_PIN,GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA,LCD_CS_PIN|T_CS_PIN|SD_CS_PIN,GPIO_PIN_SET);
    g.Pin=ADS_CS_PIN|ADS_RST_PIN;                 HAL_GPIO_Init(GPIOB,&g);
    g.Pin=LCD_CS_PIN|LCD_DC_PIN|LCD_RST_PIN|T_CS_PIN|SD_CS_PIN; HAL_GPIO_Init(GPIOA,&g);
    HAL_GPIO_WritePin(ADS_CS_PORT,ADS_CS_PIN,GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_CS_PORT,LCD_CS_PIN,GPIO_PIN_SET);
    HAL_GPIO_WritePin(T_CS_PORT,T_CS_PIN,GPIO_PIN_SET);
    HAL_GPIO_WritePin(SD_CS_PORT,SD_CS_PIN,GPIO_PIN_SET);
    g.Mode=GPIO_MODE_IT_FALLING; g.Pull=GPIO_PULLUP; g.Pin=ADS_DRDY_PIN; HAL_GPIO_Init(GPIOB,&g);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn,3,0);
    g.Mode=GPIO_MODE_INPUT;
    g.Pin=T_IRQ_PIN; HAL_GPIO_Init(GPIOB,&g);
    g.Mode=GPIO_MODE_ANALOG;g.Pull=GPIO_NOPULL;g.Pin=BAT_PIN;HAL_GPIO_Init(BAT_PORT,&g);
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *h){
    GPIO_InitTypeDef g={0};g.Mode=GPIO_MODE_AF_PP;g.Speed=GPIO_SPEED_FREQ_HIGH;g.Pull=GPIO_NOPULL;
    if(h->Instance==SPI1){
        __HAL_RCC_SPI1_CLK_ENABLE();g.Pin=GPIO_PIN_5|GPIO_PIN_7;HAL_GPIO_Init(GPIOA,&g);
        g.Mode=GPIO_MODE_INPUT;g.Pin=GPIO_PIN_6;HAL_GPIO_Init(GPIOA,&g);
    }else if(h->Instance==SPI2){
        __HAL_RCC_SPI2_CLK_ENABLE();g.Pin=GPIO_PIN_13|GPIO_PIN_15;HAL_GPIO_Init(GPIOB,&g);
        g.Mode=GPIO_MODE_INPUT;g.Pin=GPIO_PIN_14;HAL_GPIO_Init(GPIOB,&g);
    }
}
void HAL_UART_MspInit(UART_HandleTypeDef *h){
    if(h->Instance==USART1){
        __HAL_RCC_USART1_CLK_ENABLE();GPIO_InitTypeDef g={0};
        g.Pin=GPIO_PIN_9;g.Mode=GPIO_MODE_AF_PP;g.Speed=GPIO_SPEED_FREQ_HIGH;g.Pull=GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA,&g);
    }
}

static void spi_init(void){
    __HAL_RCC_SPI1_CLK_ENABLE(); __HAL_RCC_SPI2_CLK_ENABLE();
    hspi1.Instance=SPI1;
    hspi1.Init.Mode=SPI_MODE_MASTER;
    hspi1.Init.Direction=SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize=SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity=SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase=SPI_PHASE_1EDGE;
    hspi1.Init.NSS=SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler=SPI_BAUDRATEPRESCALER_64;
    hspi1.Init.FirstBit=SPI_FIRSTBIT_MSB;
    require_ok(HAL_SPI_Init(&hspi1));
    hspi2.Instance=SPI2;
    hspi2.Init=hspi1.Init;
    hspi2.Init.CLKPhase=SPI_PHASE_2EDGE; /* ADS1256 SPI mode1. */
    hspi2.Init.BaudRatePrescaler=SPI_BAUDRATEPRESCALER_32; /* 36/32=1.125MHz. */
    require_ok(HAL_SPI_Init(&hspi2));
}

static void uart_init(void){
    __HAL_RCC_USART1_CLK_ENABLE();
    huart1.Instance=USART1;
    huart1.Init.BaudRate=921600;
    huart1.Init.WordLength=UART_WORDLENGTH_8B;
    huart1.Init.StopBits=UART_STOPBITS_1;
    huart1.Init.Parity=UART_PARITY_NONE;
    huart1.Init.Mode=UART_MODE_TX;
    huart1.Init.HwFlowCtl=UART_HWCONTROL_NONE;
    huart1.Init.OverSampling=UART_OVERSAMPLING_16;
    require_ok(HAL_UART_Init(&huart1));
    HAL_NVIC_SetPriority(USART1_IRQn,2,0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static void pwm_bl_init(void){                    /* PA1 = TIM2_CH2, 1 kHz */
    __HAL_RCC_TIM2_CLK_ENABLE();
    GPIO_InitTypeDef g={0};
    g.Pin=LCD_BL_PIN; g.Mode=GPIO_MODE_AF_PP; g.Speed=GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LCD_BL_PORT,&g);
    htim2.Instance=TIM2;
    htim2.Init.Prescaler=72-1;
    htim2.Init.CounterMode=TIM_COUNTERMODE_UP;
    htim2.Init.Period=200-1;
    htim2.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
    require_ok(HAL_TIM_PWM_Init(&htim2));
    TIM_OC_InitTypeDef oc={0};
    oc.OCMode=TIM_OCMODE_PWM1;
    oc.Pulse=140;
    oc.OCPolarity=TIM_OCPOLARITY_HIGH;
    require_ok(HAL_TIM_PWM_ConfigChannel(&htim2,&oc,TIM_CHANNEL_2));
    require_ok(HAL_TIM_PWM_Start(&htim2,TIM_CHANNEL_2));
}

static void adc_init(void){                       /* PB0 = ADC1_IN8 (bateria) */
    __HAL_RCC_ADC1_CLK_ENABLE();
    hadc1.Instance=ADC1;
    hadc1.Init.ScanConvMode=ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode=DISABLE;
    hadc1.Init.DiscontinuousConvMode=DISABLE;
    hadc1.Init.ExternalTrigConv=ADC_SOFTWARE_START;
    hadc1.Init.DataAlign=ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion=1;
    require_ok(HAL_ADC_Init(&hadc1));
    ADC_ChannelConfTypeDef c={0};
    c.Channel=ADC_CHANNEL_8;
    c.Rank=ADC_REGULAR_RANK_1;
    c.SamplingTime=ADC_SAMPLETIME_28CYCLES_5;
    require_ok(HAL_ADC_ConfigChannel(&hadc1,&c));
    require_ok(HAL_ADCEx_Calibration_Start(&hadc1));
}

int main(void){
    clock_init();
    time_init();
    gpio_init();
    spi_init();
    uart_init();
    pwm_bl_init();
    adc_init();
    uart_puts("\r\n#SG6 BOOT\r\n");
    lcd_init();
    if(!ads_init()){uart_puts("#ADS INIT FAIL\r\n");while(1);}
    app_init();
    gui_init();
    for(;;){
        app_poll();
        gui_tick();
    }
}

void SysTick_Handler(void){ HAL_IncTick(); }
void EXTI15_10_IRQHandler(void){
    if(__HAL_GPIO_EXTI_GET_IT(ADS_DRDY_PIN)){
        __HAL_GPIO_EXTI_CLEAR_IT(ADS_DRDY_PIN);app_drdy_irq();
    }
}
