#ifndef GUI_H
#define GUI_H
#include <stdint.h>
void gui_init(void);       /* splash + tela principal */
void gui_tick(void);       /* toque + redraw periódico (chamar no loop) */
void gui_msg(const char*t1,const char*t2,uint16_t ms);
#endif
