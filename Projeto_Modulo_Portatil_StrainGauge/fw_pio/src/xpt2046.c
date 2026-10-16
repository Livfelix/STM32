#include "app_cfg.h"
#include "xpt2046.h"

extern SPI_HandleTypeDef hspi1;

/* calibração default (ajustar via menu CALIB → Toque): raw nos 4 cantos */
static int16_t cx0=300, cy0=320, cx1=3800, cy1=3800;   /* raw x@0, y@0, x@239, y@319 */

static void t_cs(int v){ HAL_GPIO_WritePin(T_CS_PORT,T_CS_PIN, v?GPIO_PIN_SET:GPIO_PIN_RESET); }
static void spi_touch(void){
    __HAL_SPI_DISABLE(&hspi1);
    MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_BR, SPI_BAUDRATEPRESCALER_64); /* ~1,1 MHz */
    __HAL_SPI_ENABLE(&hspi1);
}
extern void lcd_spi_fast(void);

int touch_pressed(void){
    return HAL_GPIO_ReadPin(T_IRQ_PORT,T_IRQ_PIN)==GPIO_PIN_RESET;
}

static uint16_t raw_axis(uint8_t ctrl){
    uint8_t tx[3]={ctrl,0,0}; uint8_t rx[3]={0,0,0};
    t_cs(0);
    if(HAL_SPI_TransmitReceive(&hspi1,tx,rx,3,2)!=HAL_OK)rx[1]=rx[2]=0;
    t_cs(1);
    return (rx[1]<<5)|(rx[2]>>3);
}

int touch_read(uint16_t *x,uint16_t *y){
    int32_t rx,ry;
    if(!touch_raw(&rx,&ry)) return 0;
    long px=((rx-cx0)*239L)/(cx1-cx0);
    long py=((ry-cy0)*319L)/(cy1-cy0);
    if(px<0)px=0;
    if(px>239)px=239;
    if(py<0)py=0;
    if(py>319)py=319;
    *x=px; *y=py;
    return 1;
}

int touch_raw(int32_t *rx,int32_t *ry){
    if(!touch_pressed()) return 0;
    spi_touch();
    uint32_t sx=0, sy=0; int n=0;
    for(int i=0;i<8;i++){
        uint16_t ax=raw_axis(0xD0), ay=raw_axis(0x90);
        if(ax<100||ax>4000||ay<100||ay>4000) continue;
        sx+=ax; sy+=ay; n++;
    }
    lcd_spi_fast();
    if(n<4) return 0;
    *rx=sx/n; *ry=sy/n;
    return 1;
}

void touch_calib_set(int16_t x0,int16_t y0,int16_t x1,int16_t y1){
    int16_t points[4]={x0,y0,x1,y1};
    if(!touch_calib_valid(points))return;
    cx0=x0; cy0=y0; cx1=x1; cy1=y1;
}
void touch_calib_get(int16_t points[4]){points[0]=cx0;points[1]=cy0;points[2]=cx1;points[3]=cy1;}
int touch_calib_valid(const int16_t p[4]){
    for(int i=0;i<4;i++)if(p[i]<-4096 || p[i]>8192)return 0;
    int dx=p[2]-p[0],dy=p[3]-p[1];
    return (dx>=100 || dx<=-100) && (dy>=100 || dy<=-100);
}
