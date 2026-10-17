#ifndef HOST_HAL_H
#define HOST_HAL_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef enum { HAL_OK, HAL_ERROR } HAL_StatusTypeDef;
typedef struct {uint32_t TypeErase;uintptr_t PageAddress;uint32_t NbPages;} FLASH_EraseInitTypeDef;
#define FLASH_TYPEERASE_PAGES 1
#define FLASH_TYPEPROGRAM_HALFWORD 1
static uint16_t host_flash[1024];
static int flash_cut=-1,flash_steps,flash_corrupt;
#define APP_CFG_PAGE0 ((uintptr_t)host_flash)
#define APP_CFG_PAGE1 ((uintptr_t)(host_flash+512))
static inline HAL_StatusTypeDef HAL_FLASH_Unlock(void){return HAL_OK;}
static inline HAL_StatusTypeDef HAL_FLASH_Lock(void){return HAL_OK;}
static inline HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *e,uint32_t *err){
    (void)err;if(flash_cut>=0 && flash_steps++==flash_cut)return HAL_ERROR;
    memset((void *)(uintptr_t)e->PageAddress,255,1024);return HAL_OK;
}
/* Host addresses are64bit; production PageAddress is32bit. */
#undef APP_CFG_PAGE0
#undef APP_CFG_PAGE1
#define APP_CFG_PAGE0 ((uintptr_t)host_flash)
#define APP_CFG_PAGE1 ((uintptr_t)(host_flash+512))
static inline HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type,uintptr_t address,uint64_t data){
    (void)type;if(flash_cut>=0 && flash_steps++==flash_cut)return HAL_ERROR;
    *(uint16_t *)address&=(uint16_t)data;if(flash_corrupt)*(uint16_t *)address=0;return HAL_OK;
}
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef struct { int unused; } GPIO_TypeDef;
extern GPIO_TypeDef gpio_a, gpio_b, gpio_c;
#define GPIOA (&gpio_a)
#define GPIOB (&gpio_b)
#define GPIOC (&gpio_c)
#define GPIO_PIN_0 1u
#define GPIO_PIN_1 2u
#define GPIO_PIN_2 4u
#define GPIO_PIN_3 8u
#define GPIO_PIN_4 16u
#define GPIO_PIN_5 32u
#define GPIO_PIN_6 64u
#define GPIO_PIN_7 128u
#define GPIO_PIN_8 256u
#define GPIO_PIN_9 512u
#define GPIO_PIN_10 1024u
#define GPIO_PIN_11 2048u
#define GPIO_PIN_12 4096u
#define GPIO_PIN_13 8192u
#define GPIO_PIN_14 16384u
#define GPIO_PIN_15 32768u
typedef struct { uint32_t Mode,Direction,DataSize,CLKPolarity,CLKPhase,NSS,BaudRatePrescaler,FirstBit; } SPI_InitTypeDef;
typedef struct { uint32_t CR1; } SPI_TypeDef;
typedef struct { SPI_TypeDef *Instance; SPI_InitTypeDef Init; } SPI_HandleTypeDef;
#define SPI_CR1_BR 0x38u
#define __HAL_SPI_DISABLE(h) ((void)(h))
#define __HAL_SPI_ENABLE(h) ((void)(h))
#define MODIFY_REG(r, mask, value) ((r) = ((r) & ~(mask)) | (value))
typedef struct { uint32_t BaudRate,WordLength,StopBits,Parity,Mode,HwFlowCtl,OverSampling; } UART_InitTypeDef;
typedef struct { uint32_t DR; } USART_TypeDef;
typedef struct { USART_TypeDef *Instance; UART_InitTypeDef Init; } UART_HandleTypeDef;
#define UART_FLAG_TXE 1
#define UART_IT_TXE 1
#define __HAL_UART_GET_FLAG(h,f) 1
#define __HAL_UART_DISABLE_IT(h,f) ((void)0)
#define __HAL_UART_ENABLE_IT(h,f) ((void)0)
typedef struct { uint32_t CNT,SR; } TIM_TypeDef;
extern TIM_TypeDef tim2, tim3;
typedef struct { uint32_t Prescaler,CounterMode,Period,ClockDivision; } TIM_InitTypeDef;
typedef struct { TIM_TypeDef *Instance; TIM_InitTypeDef Init; } TIM_HandleTypeDef;
typedef struct { uint32_t ScanConvMode,ContinuousConvMode,DiscontinuousConvMode,ExternalTrigConv,DataAlign,NbrOfConversion; } ADC_InitTypeDef;
typedef struct { void *Instance; ADC_InitTypeDef Init; } ADC_HandleTypeDef;
typedef struct { uint32_t Pin,Mode,Speed,Pull; } GPIO_InitTypeDef;
typedef struct { uint32_t PLLState,PLLSource,PLLMUL; } PLL_InitTypeDef;
typedef struct { uint32_t OscillatorType,HSEState,HSEPredivValue; PLL_InitTypeDef PLL; } RCC_OscInitTypeDef;
typedef struct { uint32_t ClockType,SYSCLKSource,AHBCLKDivider,APB1CLKDivider,APB2CLKDivider; } RCC_ClkInitTypeDef;
typedef struct { uint32_t PeriphClockSelection,AdcClockSelection; } RCC_PeriphCLKInitTypeDef;
typedef struct { uint32_t OCMode,Pulse,OCPolarity; } TIM_OC_InitTypeDef;
typedef struct { uint32_t Channel,Rank,SamplingTime; } ADC_ChannelConfTypeDef;
#define SPI1 ((void*)1)
#define SPI2 ((void*)2)
#define USART1 ((void*)3)
#define ADC1 ((void*)4)
#define TIM2 (&tim2)
#define TIM3 (&tim3)
#define RESET 0
#define DISABLE 0
#define RCC_OSCILLATORTYPE_HSE 1
#define RCC_HSE_ON 1
#define RCC_HSE_PREDIV_DIV1 1
#define RCC_PLL_ON 1
#define RCC_PLLSOURCE_HSE 1
#define RCC_PLL_MUL9 9
#define RCC_CLOCKTYPE_HCLK 1
#define RCC_CLOCKTYPE_PCLK1 2
#define RCC_CLOCKTYPE_PCLK2 4
#define RCC_CLOCKTYPE_SYSCLK 8
#define RCC_SYSCLKSOURCE_PLLCLK 1
#define RCC_SYSCLK_DIV1 1
#define RCC_HCLK_DIV2 2
#define RCC_HCLK_DIV1 1
#define FLASH_LATENCY_2 2
#define RCC_PERIPHCLK_ADC 1
#define RCC_ADCPCLK2_DIV6 6
#define GPIO_MODE_OUTPUT_PP 1
#define GPIO_MODE_INPUT 2
#define GPIO_MODE_AF_PP 3
#define GPIO_MODE_ANALOG 4
#define GPIO_SPEED_FREQ_HIGH 1
#define GPIO_NOPULL 0
#define GPIO_PULLUP 1
#define SPI_MODE_MASTER 1
#define SPI_DIRECTION_2LINES 1
#define SPI_DATASIZE_8BIT 8
#define SPI_POLARITY_LOW 0
#define SPI_PHASE_1EDGE 0
#define SPI_PHASE_2EDGE 1
#define SPI_NSS_SOFT 1
#define SPI_BAUDRATEPRESCALER_64 0x28u
#define SPI_BAUDRATEPRESCALER_32 0x20u
#define SPI_BAUDRATEPRESCALER_8 0x10u
#define SPI_BAUDRATEPRESCALER_256 56
#define SPI_FIRSTBIT_MSB 1
#define UART_WORDLENGTH_8B 8
#define UART_STOPBITS_1 1
#define UART_PARITY_NONE 0
#define UART_MODE_TX_RX 3
#define UART_MODE_TX 1
#define UART_HWCONTROL_NONE 0
#define UART_OVERSAMPLING_16 16
#define USART1_IRQn 1
#define TIM3_IRQn 2
#define EXTI15_10_IRQn 3
#define SysTick_IRQn 4
#define GPIO_MODE_IT_FALLING 5
#define __HAL_GPIO_EXTI_CLEAR_IT(p) ((void)(p))
#define __HAL_GPIO_EXTI_GET_IT(p) 1
#define __DMB() ((void)0)
#define TIM_COUNTERMODE_UP 0
#define TIM_CLOCKDIVISION_DIV1 1
#define TIM_OCMODE_PWM1 1
#define TIM_OCPOLARITY_HIGH 1
#define TIM_CHANNEL_2 2
#define __HAL_TIM_SET_COMPARE(h,c,v) ((void)(h),(void)(c),(void)(v))
#define TIM_FLAG_UPDATE 1
#define TIM_IT_UPDATE 1
#define ADC_SCAN_DISABLE 0
#define ADC_SOFTWARE_START 1
#define ADC_DATAALIGN_RIGHT 1
#define ADC_CHANNEL_8 8
#define ADC_REGULAR_RANK_1 1
#define ADC_SAMPLETIME_28CYCLES_5 1
uint32_t host_counter(TIM_HandleTypeDef*);
#define __HAL_TIM_GET_COUNTER(h) host_counter(h)
#define __HAL_TIM_GET_FLAG(h,f) ((h)->Instance->SR & (f))
#define __HAL_TIM_CLEAR_IT(h,f) ((h)->Instance->SR &= ~(f))
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOC_CLK_ENABLE() ((void)0)
#define __HAL_RCC_AFIO_CLK_ENABLE() ((void)0)
#define __HAL_RCC_SPI1_CLK_ENABLE() ((void)0)
#define __HAL_RCC_SPI2_CLK_ENABLE() ((void)0)
#define __HAL_RCC_USART1_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM2_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM3_CLK_ENABLE() ((void)0)
#define __HAL_RCC_ADC1_CLK_ENABLE() ((void)0)
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef*,uint8_t*,uint8_t*,uint16_t,uint32_t);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef*,uint8_t*,uint16_t,uint32_t);
void HAL_GPIO_WritePin(GPIO_TypeDef*,uint16_t,GPIO_PinState);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef*,uint16_t);
void HAL_Delay(uint32_t);
uint32_t HAL_GetTick(void);
void HAL_GPIO_Init(GPIO_TypeDef*,GPIO_InitTypeDef*);
HAL_StatusTypeDef HAL_Init(void);
HAL_StatusTypeDef HAL_RCC_OscConfig(RCC_OscInitTypeDef*);
HAL_StatusTypeDef HAL_RCC_ClockConfig(RCC_ClkInitTypeDef*,uint32_t);
HAL_StatusTypeDef HAL_RCCEx_PeriphCLKConfig(RCC_PeriphCLKInitTypeDef*);
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef*);
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef*);
void HAL_NVIC_SetPriority(int,int,int);
void HAL_NVIC_EnableIRQ(int);
void HAL_NVIC_DisableIRQ(int);
void HAL_NVIC_ClearPendingIRQ(int);
HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef*);
HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef*,TIM_OC_InitTypeDef*,uint32_t);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef*,uint32_t);
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef*);
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef*);
HAL_StatusTypeDef HAL_ADC_Init(ADC_HandleTypeDef*);
HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef*,ADC_ChannelConfTypeDef*);
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef*);
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef*);
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef*);
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef*,uint32_t);
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef*);
void HAL_IncTick(void);
#endif
