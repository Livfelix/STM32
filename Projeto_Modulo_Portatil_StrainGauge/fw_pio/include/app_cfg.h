/* app_cfg.h — pinagem REAL da placa integrada (ver PINOUT_PLACA_INTEGRADA.md) */
#ifndef APP_CFG_H
#define APP_CFG_H
#include "stm32f1xx_hal.h"
#include <stdint.h>

uint64_t app_time_us(void);

/* ---- ADS1256 (SPI2, fixo na placa) ---- */
#define ADS_SPI      hspi2
#define ADS_CS_PORT  GPIOB
#define ADS_CS_PIN   GPIO_PIN_12
#define ADS_DRDY_PORT GPIOB
#define ADS_DRDY_PIN GPIO_PIN_10
#define ADS_RST_PORT GPIOB
#define ADS_RST_PIN  GPIO_PIN_11

/* ---- SPI1 compartilhado: LCD + toque + SD ---- */
#define P_SPI        hspi1
#define LCD_CS_PORT  GPIOA
#define LCD_CS_PIN   GPIO_PIN_4
#define LCD_DC_PORT  GPIOA
#define LCD_DC_PIN   GPIO_PIN_3
#define LCD_RST_PORT GPIOA
#define LCD_RST_PIN  GPIO_PIN_2
#define LCD_BL_PORT  GPIOA
#define LCD_BL_PIN   GPIO_PIN_1      /* TIM2_CH2 PWM */
#define T_CS_PORT    GPIOA
#define T_CS_PIN     GPIO_PIN_0
#define T_IRQ_PORT   GPIOB
#define T_IRQ_PIN    GPIO_PIN_1
#define SD_CS_PORT   GPIOA
#define SD_CS_PIN    GPIO_PIN_8

/* ---- Bateria: PB0 = ADC1_IN8 (divisor 10k/10k) ---- */
#define BAT_PORT     GPIOB
#define BAT_PIN      GPIO_PIN_0

#define NCH          6
#define CH_VEXC      7
#define VREF_ADC     2.5f
#define VMEIO_NOM    1.65f
#define VEXC_DIV     (0.5f/1.01f) /* Buffered 2.2k/2.2k plus onboard 100/10k. */
#endif
