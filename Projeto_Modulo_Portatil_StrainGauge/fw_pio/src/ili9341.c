#include "app_cfg.h"
#include "ili9341.h"
#include "font.h"
#include "app.h"
#include <stdarg.h>
#include <stdio.h>

extern SPI_HandleTypeDef hspi1;
extern TIM_HandleTypeDef htim2;

static void dc(int v){ HAL_GPIO_WritePin(LCD_DC_PORT,LCD_DC_PIN, v?GPIO_PIN_SET:GPIO_PIN_RESET); }
static void cs(int v){ HAL_GPIO_WritePin(LCD_CS_PORT,LCD_CS_PIN, v?GPIO_PIN_SET:GPIO_PIN_RESET); }
static void wr(uint8_t b){ HAL_SPI_Transmit(&hspi1,&b,1,1); }
static void w16(uint16_t v){ uint8_t b[2]={v>>8,v}; HAL_SPI_Transmit(&hspi1,b,2,1); }
static void cmd(uint8_t c){ cs(0); dc(0); wr(c); cs(1); }
static void dat(uint8_t d){ cs(0); dc(1); wr(d); cs(1); }

void lcd_spi_fast(void){
    __HAL_SPI_DISABLE(&hspi1);
    MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_BR, SPI_BAUDRATEPRESCALER_8);
    __HAL_SPI_ENABLE(&hspi1);
}
static void spi_slow(void){
    __HAL_SPI_DISABLE(&hspi1);
    MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_BR, SPI_BAUDRATEPRESCALER_64);
    __HAL_SPI_ENABLE(&hspi1);
}

static int clip(uint16_t x,uint16_t y,uint16_t *w,uint16_t *h){
    if(x>=LCD_W || y>=LCD_H || !*w || !*h)return 0;
    if(*w>LCD_W-x)*w=LCD_W-x;
    if(*h>LCD_H-y)*h=LCD_H-y;
    return 1;
}
void lcd_setwin(uint16_t x,uint16_t y,uint16_t w,uint16_t h){
    if(!clip(x,y,&w,&h))return;
    uint16_t x2=x+w-1, y2=y+h-1;
    cmd(0x2A); dat(x>>8); dat(x); dat(x2>>8); dat(x2);
    cmd(0x2B); dat(y>>8); dat(y); dat(y2>>8); dat(y2);
    cmd(0x2C);
}
void lcd_fill(uint16_t c){
    lcd_rect(0,0,LCD_W,LCD_H,c);
}
void lcd_rect(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t c){
    if(!clip(x,y,&w,&h))return;
    lcd_setwin(x,y,w,h);
    uint8_t buffer[128];for(unsigned i=0;i<sizeof buffer;i+=2){buffer[i]=c>>8;buffer[i+1]=c;}
    for(uint32_t left=(uint32_t)w*h;left;){
        uint16_t count=left>64?64:(uint16_t)left;
        lcd_spi_fast();cs(0);dc(1);HAL_SPI_Transmit(&hspi1,buffer,count*2,2);cs(1);left-=count;
        /* LCD CS must be high before polling because SD shares SPI1. */
        app_poll();
    }
}
void lcd_frame(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t c){
    if(!clip(x,y,&w,&h))return;
    lcd_rect(x,y,w,1,c); lcd_rect(x,y+h-1,w,1,c);
    lcd_rect(x,y,1,h,c); lcd_rect(x+w-1,y,1,h,c);
}
void lcd_pix(uint16_t x,uint16_t y,uint16_t c){if(x>=LCD_W || y>=LCD_H)return; lcd_spi_fast();lcd_setwin(x,y,1,1); cs(0); dc(1); w16(c); cs(1); }

void lcd_text(uint16_t x,uint16_t y,const char*s,uint16_t fg,uint16_t bg,uint8_t sc){
    if(!sc) sc=1;
    for(;*s;s++){
        uint8_t ch=(uint8_t)*s; if(ch<32||ch>127) ch='?';
        if(x>=LCD_W || y>=LCD_H)return;
        const uint8_t*g=font6x10[ch-32];
        uint16_t width=FONT_W*sc,height=FONT_H*sc;if(!clip(x,y,&width,&height))return;
        for(uint16_t row=0;row<height;row++){
            lcd_spi_fast();lcd_setwin(x,y+row,width,1);
            uint8_t buffer[128];
            for(unsigned start=0;start<width;){
                unsigned count=width-start>64?64:width-start;
                for(unsigned cx=0;cx<count;cx++){uint16_t c2=(g[row/sc]&(0x80>>((start+cx)/sc)))?fg:bg;buffer[2*cx]=c2>>8;buffer[2*cx+1]=c2;}
                cs(0);dc(1);HAL_SPI_Transmit(&hspi1,buffer,count*2,2);cs(1);start+=count;
            }
            app_poll();
        }
        uint32_t next=(uint32_t)x+FONT_ADV*sc;if(next>=LCD_W)return;x=(uint16_t)next;
    }
}
void lcd_textf(uint16_t x,uint16_t y,uint16_t fg,uint16_t bg,uint8_t sc,const char*fmt,...){
    char b[96]; va_list a; va_start(a,fmt); vsnprintf(b,sizeof b,fmt,a); va_end(a);
    lcd_text(x,y,b,fg,bg,sc);
}
void lcd_bmp_rgb565(uint16_t x,uint16_t y,uint16_t w,uint16_t h,const uint16_t*d){
    uint16_t stride=w;if(!d || !clip(x,y,&w,&h))return;
    for(unsigned row=0;row<h;row++){
        lcd_spi_fast();lcd_setwin(x,y+row,w,1);cs(0);dc(1);
        for(unsigned col=0;col<w;col++)w16(d[row*stride+col]);
        cs(1);app_poll();
    }
}
void lcd_backlight(uint8_t pct){
    if(pct>100)pct=100;
    uint32_t cmp=(uint32_t)pct*200/100;             /* ARR=200 */
    __HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_2,cmp);
}

void lcd_init(void){
    spi_slow();
    HAL_GPIO_WritePin(LCD_RST_PORT,LCD_RST_PIN,GPIO_PIN_RESET); HAL_Delay(10);
    HAL_GPIO_WritePin(LCD_RST_PORT,LCD_RST_PIN,GPIO_PIN_SET);  HAL_Delay(120);
    cmd(0x01); HAL_Delay(5);                 /* soft reset (modulo ja inicia) */
    cmd(0xCF); dat(0);dat(0xC1);dat(0x30);
    cmd(0xED); dat(0x64);dat(0x03);dat(0x12);dat(0x81);
    cmd(0xE8); dat(0x85);dat(0x00);dat(0x78);
    cmd(0xCB); dat(0x39);dat(0x2C);dat(0x00);dat(0x34);dat(0x02);
    cmd(0xF7); dat(0x20);
    cmd(0xEA); dat(0x00);dat(0x00);
    cmd(0xC0); dat(0x23);                    /* PWCTR1 */
    cmd(0xC1); dat(0x10);                    /* PWCTR2 */
    cmd(0xC5); dat(0x3E);dat(0x28);          /* VMCTR1 */
    cmd(0xC7); dat(0x86);                    /* VMCTR2 */
    cmd(0x36); dat(0x48);                    /* MADCTL: portrait, RGB */
    cmd(0x37); dat(0x00);
    cmd(0x3A); dat(0x55);                    /* RGB565 */
    cmd(0xB1); dat(0x00);dat(0x18);
    cmd(0xB6); dat(0x08);dat(0x82);dat(0x27);
    cmd(0xF2); dat(0x00);
    cmd(0x26); dat(0x01);
    cmd(0xE0); for(uint8_t i=0;i<15;i++) dat((uint8_t[]){0x0F,0x31,0x2B,0x0C,0x0E,0x08,0x4E,0xF1,0x37,0x07,0x10,0x03,0x0E,0x09,0x00}[i]);
    cmd(0xE1); for(uint8_t i=0;i<15;i++) dat((uint8_t[]){0x00,0x0E,0x14,0x03,0x11,0x07,0x31,0xCF,0x4E,0x08,0x0E,0x0C,0x31,0x36,0x0F}[i]);
    cmd(0x11); HAL_Delay(120);
    cmd(0x29);
    lcd_spi_fast();
    lcd_backlight(70);
}
