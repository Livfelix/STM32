#ifndef ILI9341_H
#define ILI9341_H
#include <stdint.h>
#define LCD_W 240
#define LCD_H 320
/* cores RGB565 */
#define C_BLACK 0x0000
#define C_WHITE 0xFFFF
#define C_RED   0xF800
#define C_GREEN 0x07E0
#define C_BLUE  0x001F
#define C_CYAN  0x07FF
#define C_YELL  0xFFE0
#define C_GRAY  0x8410
#define C_DGRAY 0x4208
#define C_ORAN  0xFD20

void lcd_init(void);
void lcd_fill(uint16_t c);
void lcd_rect(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t c);
void lcd_frame(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t c);
void lcd_setwin(uint16_t x,uint16_t y,uint16_t w,uint16_t h);
void lcd_pix(uint16_t x,uint16_t y,uint16_t c);
void lcd_text(uint16_t x,uint16_t y,const char*s,uint16_t fg,uint16_t bg,uint8_t scale);
void lcd_textf(uint16_t x,uint16_t y,uint16_t fg,uint16_t bg,uint8_t sc,const char*fmt,...);
void lcd_bmp_rgb565(uint16_t x,uint16_t y,uint16_t w,uint16_t h,const uint16_t*d);
void lcd_backlight(uint8_t pct);            /* 0..100 PWM TIM2_CH2 */
void lcd_spi_fast(void);                    /* /8 = 9 MHz p/ tela */
#endif
