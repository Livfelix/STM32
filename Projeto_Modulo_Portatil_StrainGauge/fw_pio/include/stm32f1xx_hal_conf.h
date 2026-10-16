/* stm32f1xx_hal_conf.h — habilita só o que o projeto usa */
#ifndef STM32F1xx_HAL_CONF_H
#define STM32F1xx_HAL_CONF_H
#include "stm32f1xx.h"

#define HAL_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_FLASH_EX_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_USART_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_ADC_EX_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED

#define HSE_VALUE    ((uint32_t)8000000)
#define HSE_STARTUP_TIMEOUT 100
#define HSI_VALUE    ((uint32_t)8000000)
#define LSI_VALUE    ((uint32_t)40000)
#define LSE_VALUE    ((uint32_t)32768)
#define LSE_STARTUP_TIMEOUT 5000
#define VDD_VALUE    ((uint32_t)3300)
#define TICK_INT_PRIORITY ((uint32_t)0)
#define USE_RTOS     0
#define PREFETCH_ENABLE 1

#include "stm32f1xx_hal_def.h"
#include "stm32f1xx_hal_rcc.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_flash.h"
#include "stm32f1xx_hal_flash_ex.h"
#include "stm32f1xx_hal_dma.h"
#include "stm32f1xx_hal_cortex.h"
#include "stm32f1xx_hal_pwr.h"
#include "stm32f1xx_hal_spi.h"
#include "stm32f1xx_hal_uart.h"
#include "stm32f1xx_hal_adc.h"
#include "stm32f1xx_hal_tim.h"
#include "stm32f1xx_hal_tim_ex.h"
#endif
