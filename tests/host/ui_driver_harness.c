#include "app_cfg.h"
#include "ili9341.h"
#include "xpt2046.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
GPIO_TypeDef gpio_a,gpio_b,gpio_c;
static SPI_TypeDef spi;
SPI_HandleTypeDef hspi1={.Instance=&spi};
TIM_HandleTypeDef htim2;
static int dc_level,pressed=1,mode;
static uint8_t command,parameters[4];
static unsigned parameter_count,pixel_count,commands,foreground_pixels;
static uint16_t x0,y0,x1,y1;
static uint16_t pixels[320][240];
void HAL_GPIO_WritePin(GPIO_TypeDef*p,uint16_t pin,GPIO_PinState state){(void)p;if(pin==LCD_DC_PIN)dc_level=state;}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef*p,uint16_t pin){(void)p;(void)pin;return pressed?GPIO_PIN_RESET:GPIO_PIN_SET;}
void HAL_Delay(uint32_t t){(void)t;}
uint32_t HAL_GetTick(void){return 0;}
void app_poll(void){}
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef*h,uint8_t*b,uint16_t n,uint32_t t){
    (void)h;(void)t;
    if(!dc_level){command=*b;commands++;parameter_count=0;pixel_count=0;return HAL_OK;}
    if(command==0x2A || command==0x2B){for(unsigned i=0;i<n;i++)parameters[parameter_count++]=b[i];if(parameter_count==4){uint16_t a=parameters[0]*256+parameters[1],z=parameters[2]*256+parameters[3];if(command==0x2A){x0=a;x1=z;assert(x0<=x1 && x1<240);}else{y0=a;y1=z;assert(y0<=y1 && y1<320);}}}
    else if(command==0x2C){assert(!(n%2));for(unsigned i=0;i<n;i+=2){unsigned x=x0+pixel_count%(x1-x0+1),y=y0+pixel_count/(x1-x0+1);assert(x<=x1 && y<=y1);pixels[y][x]=b[i]*256+b[i+1];if(pixels[y][x]==C_WHITE)foreground_pixels++;pixel_count++;}}
    return HAL_OK;
}
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef*h,uint8_t*t,uint8_t*r,uint16_t n,uint32_t timeout){(void)h;(void)timeout;assert(n==3);unsigned raw=mode?4090:(t[0]==0xD0?2050:2060);r[0]=0;r[1]=raw>>5;r[2]=(raw&31)<<3;return HAL_OK;}
#include "../../Projeto_Modulo_Portatil_StrainGauge/fw_pio/src/ili9341.c"
#include "../../Projeto_Modulo_Portatil_StrainGauge/fw_pio/src/xpt2046.c"
int main(int argc,char **argv){assert(argc==2);
    if(!strcmp(argv[1],"clip")){lcd_rect(239,319,5,5,C_WHITE);assert(pixels[319][239]==C_WHITE);unsigned count=commands;lcd_rect(240,0,3,3,0);lcd_pix(65535,0,0);lcd_rect(0,0,0,1,0);assert(commands==count);lcd_frame(239,319,9,9,0);lcd_text(235,315,"FULL",C_WHITE,C_BLACK,1);}
    else if(!strcmp(argv[1],"font")){lcd_text(0,0,"A",C_WHITE,C_BLACK,1);unsigned expected=0;for(unsigned y=0;y<FONT_H;y++){unsigned row=font6x10['A'-32][y];while(row){expected+=row&1;row>>=1;}}assert(foreground_pixels==expected && expected>8);}
    else if(!strcmp(argv[1],"touch")){touch_calib_set(100,100,100,100);uint16_t x=999,y=999;assert(touch_read(&x,&y));assert(x==119 && y==159);touch_calib_set(3800,3800,300,300);assert(touch_read(&x,&y));assert(x<240 && y<320);mode=1;assert(!touch_read(&x,&y));}
    else assert(!"unknown");puts("PASS");return 0;
}
