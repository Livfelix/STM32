#define main acquisition_main
#include "acquisition_harness.c"
#undef main
#include "ili9341.h"
#include "font.h"
static uint16_t pixels[320][240];
static int trace;
static char seen_shunt[34],seen_range[34],seen_elapsed[34],seen_battery[8];
const uint16_t logo_rgb565[7200]={0};
void lcd_fill(uint16_t c){for(int y=0;y<320;y++)for(int x=0;x<240;x++)pixels[y][x]=c;}
void lcd_rect(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t c){assert(x+w<=240 && y+h<=320);for(unsigned j=y;j<y+h;j++)for(unsigned i=x;i<x+w;i++)pixels[j][i]=c;}
void lcd_frame(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t c){lcd_rect(x,y,w,1,c);lcd_rect(x,y+h-1,w,1,c);lcd_rect(x,y,1,h,c);lcd_rect(x+w-1,y,1,h,c);}
void lcd_pix(uint16_t x,uint16_t y,uint16_t c){assert(x<240 && y<320);pixels[y][x]=c;}
void lcd_text(uint16_t x,uint16_t y,const char*s,uint16_t fg,uint16_t bg,uint8_t scale){assert(scale==1);if(x+strlen(s)*FONT_ADV>240)fprintf(stderr,"TEXT OVERFLOW %u %s\n",x,s);assert(x+strlen(s)*FONT_ADV<=240 && y+FONT_H<=320);if(!strncmp(s,"R SHUNT",7))snprintf(seen_shunt,34,"%s",s);if(strstr(s,"EST "))snprintf(seen_range,34,"%s",s);if(y==234)snprintf(seen_elapsed,34,"%s",s);if(y==4 && s[0]=='~')snprintf(seen_battery,sizeof seen_battery,"%s",s);if(trace)printf("TEXT %u,%u %s\n",x,y,s);for(;*s;s++,x+=FONT_ADV){unsigned ch=(unsigned char)*s;if(ch<32 || ch>127)ch='?';for(unsigned row=0;row<FONT_H;row++)for(unsigned col=0;col<FONT_W;col++)pixels[y+row][x+col]=(font6x10[ch-32][row]&(0x80>>col))?fg:bg;}}
void lcd_bmp_rgb565(uint16_t x,uint16_t y,uint16_t w,uint16_t h,const uint16_t*d){(void)d;lcd_rect(x,y,w,h,0);}
void lcd_backlight(uint8_t c){assert(c<=100);}
int touch_raw(int32_t*x,int32_t*y){(void)x;(void)y;return 0;}
int touch_pressed(void){return 0;}
int touch_read(uint16_t*x,uint16_t*y){(void)x;(void)y;return 0;}
#include "../../Projeto_Modulo_Portatil_StrainGauge/fw_pio/src/gui.c"
static void settle(void){for(int i=0;i<100;i++){now+=1000;gui_tick();}}
static void snapshot(const char *path){FILE *out=fopen(path,"wb");assert(out);fprintf(out,"P6\n240 320\n255\n");for(int y=0;y<320;y++)for(int x=0;x<240;x++){uint16_t c=pixels[y][x];uint8_t rgb[3]={(uint8_t)((c>>11)*255/31),(uint8_t)(((c>>5)&63)*255/63),(uint8_t)((c&31)*255/31)};fwrite(rgb,1,3,out);}fclose(out);}
static void enter_shunt(const char *digits){handle(94);for(int i=0;i<8;i++){int old=i==1?1:0;for(int j=0;j<(digits[i]-'0'-old+10)%10;j++)handle(95);handle(97);}handle(98);}
int main(int argc,char**argv){assert(argc==2);memset(host_flash,255,sizeof host_flash);app_init();drain_uart();
    if(!strcmp(argv[1],"grid")){gui_init();settle();assert(ncur>=4 && cur[0].y==cur[1].y && cur[2].y>cur[0].y);}
    else if(!strcmp(argv[1],"zero")){ch_bias_valid=ch_zero_valid=63;for(int i=0;i<6;i++){ch_bias_v[i]=1.2f;ch_zero_ratio[i]=99;}handle(30);assert(ch_zero_ratio[0]==99);handle(91);assert(ch_zero_ratio[0]==99);handle(30);handle(90);assert(fabsf(ch_zero_ratio[0])<.000001f);}
    else if(!strcmp(argv[1],"selected")){float before[6];memcpy(before,ch_gf,sizeof before);handle(66);handle(62);for(int i=0;i<6;i++)assert(ch_gf[i]==before[i]+(i==1?.01f:0));}
    else if(!strcmp(argv[1],"rates")){handle(74);cfg.ch_mask=1;handle(40);assert(cfg.ch_mask==1);uint8_t seen[256]={0};for(int i=0;i<16;i++){handle(52);seen[cfg.drate_code]=1;}unsigned count=0;for(int i=0;i<256;i++)count+=seen[i];assert(count==16);}
    else if(!strcmp(argv[1],"locks")){PresetCfg before=cfg;float gf=ch_gf[0];uint8_t bright=app_brightness();const int ids[]={40,52,53,54,55,60,61,62,63,65,67,68,69,70,71,72,76,79,81,82,83,84,85,86,87,88,89,92,93,94,95};for(unsigned i=0;i<sizeof ids/sizeof *ids;i++){recording=1;handle(ids[i]);assert(!memcmp(&cfg,&before,sizeof cfg));assert(ch_gf[0]==gf && app_brightness()==bright);}recording=0;}
    else if(!strcmp(argv[1],"render")){trace=1;gui_init();settle();snapshot("tmp/task5-main.ppm");const uint8_t actions[]={74,73,77,78,76};const char *files[]={"tmp/task5-custom.ppm","tmp/task5-calibration.ppm","tmp/task5-log.ppm","tmp/task5-trigger.ppm","tmp/task5-touch.ppm"};for(unsigned i=0;i<5;i++){handle(actions[i]);settle();snapshot(files[i]);}handle(60);settle();snapshot("tmp/task5-confirm.ppm");}
    else if(!strcmp(argv[1],"battery")){
        now=0;batt_poll();batt_mv=4150;gui_init();settle();assert(!strcmp(seen_battery,"~100%"));
        for(unsigned i=0;i<4;i++)assert(pixels[6][206+6*i]==C_GREEN);
        now=500000;draw_main(12);for(unsigned i=0;i<4;i++)assert(pixels[6][206+6*i]==C_GREEN);
        batt_mv=3850;draw_main(12);assert(!strcmp(seen_battery,"~60%"));for(unsigned i=0;i<4;i++)assert(pixels[6][206+6*i]==(i<3?C_GREEN:C_BLUE));
        battery_raw=2110;now=1000000;batt_poll();now=1500000;batt_poll();draw_main(12);assert(strstr(drawn_status[0],"CARREGAR"));assert(pixels[6][206]==C_BLUE);
        now=1999999;draw_main(12);assert(pixels[6][206]==C_BLUE);now=2000000;draw_main(12);assert(pixels[4][204]==C_RED);for(unsigned i=0;i<4;i++)assert(pixels[6][206+6*i]==C_BLUE);
        refresh_ms=1000;last_draw=2000;phase=screen_steps();now=2500000;settle();assert(pixels[6][206]==C_BLUE);refresh_ms=250;
        recording=1;error="SD WRITE / INCOMPLETE";draw_main(13);draw_main(18);assert(strstr(drawn_status[1],"INCOMPLETE") && drawn_recording==1);recording=0;
        error="";invalid_battery=1;batt_poll();draw_main(12);assert(!strcmp(seen_battery,"~--%") && strstr(drawn_status[0],"BAT FALHA"));
        assert(pixels[6][206]==C_BLUE);snapshot("tmp/task1-main-battery-invalid.ppm");
        invalid_battery=0;battery_raw=2110;batt_poll();now=3000000;draw_main(12);snapshot("tmp/task1-main-battery-low.ppm");
    }
    else if(!strcmp(argv[1],"extremes")){cfg.drate_code=DR_30000;cfg.fc_lp=500;assert(app_reapply());ch_ue[0]=-2147483520;ch_ue[1]=2147483520;ch_ue[2]=-12345678;measured_sps=3400;batt_mv=4400;saturation=3;recording=1;error="SD WRITE / INCOMPLETE";gui_init();settle();assert(strstr(drawn_status[1],"INCOMPLETE"));assert(drawn_recording==1);snapshot("tmp/task5-main-rec-errors.ppm");recording=0;}
    else if(!strcmp(argv[1],"fractional")){cfg.drate_code=DR_2_5;cfg.drate_sps=2.5f;cfg.ch_mask=63;cfg.fc_lp=0;assert(app_reapply());measured_sps=.3f;gui_init();settle();assert(strstr(drawn_status[2],"P0.4 M0.3 R0.3 SPS/CH"));snapshot("tmp/task6-main-fractional.ppm");}
    else if(!strcmp(argv[1],"sd_latency")){sd_max_io_us=123456;gui_init();settle();assert(strstr(drawn_status[5],"SDmax123.456ms"));snapshot("tmp/final-fix-main-latency.ppm");}
    else if(!strcmp(argv[1],"elapsed")){error="";strcpy(fname,"LONG1234.SGB");recording=1;rec_ms=125999;gui_init();settle();assert(strstr(drawn_status[1],"02:05") && strstr(drawn_status[1],fname));snapshot("tmp/final-main-elapsed.ppm");error="SD WRITE / INCOMPLETE";phase=13;settle();assert(strstr(drawn_status[1],"INCOMPLETE"));recording=0;}
    else if(!strcmp(argv[1],"measured_shunt")){trace=1;ch_bias_valid=ch_zero_valid=63;ch_bias_v[0]=1.2f;ch_zero_ratio[0]=0;sample_ue=-600;gui_init();handle(73);settle();assert(strstr(seen_shunt,"100000.0"));enter_shunt("03301234");settle();assert(strstr(seen_shunt,"330123.4"));handle(61);settle();snapshot("tmp/final-shunt-confirm.ppm");handle(90);assert(app_shunt_pending(0));float expected;assert(sg_shunt_reference(HALF_BENDING,350,330123.4f,ch_gf[0],&expected));assert(fabsf(shunt_scale-expected/-600)<.00001f);float staged=shunt_scale;handle(94);assert(scr==S_MSG && shunt_scale==staged);sample_ue=0;handle(61);handle(90);assert(!app_shunt_pending(0) && fabsf(ch_scale[0]-staged)<.00001f);handle(94);settle();snapshot("tmp/final-shunt-editor.ppm");recording=1;handle(95);handle(98);recording=0;}
    else if(!strcmp(argv[1],"range")){trace=1;gui_init();handle(73);settle();assert(strstr(seen_range,"INDISP"));ch_bias_valid=ch_zero_valid=63;ch_bias_v[0]=1.2f;ch_zero_ratio[0]=0;vexc_v=5;redraw();settle();assert(strstr(seen_range,"EST -2019..3422 ue"));snapshot("tmp/final-cal-range.ppm");ch_bridge[0]=QUARTER;ch_scale[0]=2;redraw();settle();assert(strstr(seen_range,"EST -8076..13688 ue"));ch_gain[0]=200;redraw();settle();assert(strstr(seen_range,"EST -4038..6844 ue"));vexc_v=NAN;redraw();settle();assert(strstr(seen_range,"INDISP"));}
    else if(!strcmp(argv[1],"shunt_validation")){handle(94);edit_shunt=0;handle(98);assert(shunt_tenth==1000000 && scr==S_MSG);handle(91);edit_shunt=10000000;handle(98);assert(shunt_tenth==1000000 && scr==S_MSG);handle(91);edit_shunt=3300000;recording=1;handle(95);handle(97);handle(98);assert(shunt_tenth==1000000 && edit_shunt==3300000);recording=0;handle(99);assert(shunt_tenth==1000000);ch_bias_valid=ch_zero_valid=63;ch_bias_v[0]=1.2f;sample_ue=-600;assert(app_shunt(0,100000));cal_channel=1;handle(94);assert(scr==S_MSG && shunt_tenth==1000000);}
    else assert(!"unknown case");puts("PASS");return 0;
}





