#ifndef XPT2046_H
#define XPT2046_H
#include <stdint.h>
/* retorna 1 se toque valido; *x,*y em pixels 0..239 / 0..319 */
int  touch_read(uint16_t *x, uint16_t *y);
int  touch_pressed(void);
int  touch_raw(int32_t *rx, int32_t *ry);   /* média bruta p/ calibração */
void touch_calib_set(int16_t x0,int16_t y0,int16_t x1,int16_t y1);
void touch_calib_get(int16_t points[4]);
int touch_calib_valid(const int16_t points[4]);
#endif
