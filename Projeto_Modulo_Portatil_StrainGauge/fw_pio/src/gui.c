/* Each tick draws one bounded element; LCD yields with CS released to app_poll. */
#include "app_cfg.h"
#include "gui.h"
#include "ili9341.h"
#include "xpt2046.h"
#include "app.h"
#include "ads1256.h"
#include "font.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>

enum { S_MAIN,S_CUSTOM,S_CAL,S_MENU,S_CALT,S_LOG,S_TRIGGER,S_CONFIRM,S_MSG,S_SHUNT };
static int scr=S_MAIN,return_scr=S_MAIN,phase=-40;
static uint8_t cal_channel,pending,pending_channel;
static uint8_t zero_mask,cal_step;
static uint32_t last_draw,last_touch,message_until;
static uint16_t refresh_ms=250;
static char message[2][34],edit_prefix[4];
static uint8_t prefix_pos;
static uint32_t edit_duration;
static uint32_t shunt_tenth=1000000,edit_shunt;
static uint8_t shunt_pos;
static const uint32_t shunt_digits[]={10000000,1000000,100000,10000,1000,100,10,1};
static int32_t cal_x,cal_y,cal_count,first_x,first_y;
static int8_t graph[96];
static unsigned graph_pos;
static int32_t drawn_values[6],drawn_graph=INT32_MIN;
static uint8_t drawn_mask,drawn_sat,drawn_types[6],saturation;
static char drawn_status[6][34];
static int drawn_recording=-1,drawn_battery=-1;
typedef struct {uint16_t x,y,w,h;uint8_t id;} Btn;
static Btn cur[24];static unsigned ncur;
static const uint8_t drcode[]={DR_30000,DR_15000,DR_7500,DR_3750,DR_2000,DR_1000,DR_500,DR_100,DR_60,DR_50,DR_30,DR_25,DR_15,DR_10,DR_5,DR_2_5};
static const float drates[]={30000,15000,7500,3750,2000,1000,500,100,60,50,30,25,15,10,5,2.5f};
static const float fcs[]={0,1,5,10,20,50,100,200,500};
static void btn_add(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint8_t id){
    for(unsigned i=0;i<ncur;i++)if(cur[i].id==id)return;
    if(ncur<24)cur[ncur++]=(Btn){x,y,w,h,id};
}
static void button(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint8_t id,const char *label,uint16_t bg){
    lcd_rect(x,y,w,h,bg);lcd_frame(x,y,w,h,C_GRAY);
    size_t width=strlen(label)*FONT_ADV;
    lcd_text(x+(w>width?(w-width)/2:2),y+(h-FONT_H)/2,label,C_WHITE,bg,1);btn_add(x,y,w,h,id);
}
static void row(unsigned index,uint8_t id,const char *label){button(4,28+index*25,232,23,id,label,C_DGRAY);}
static void line(uint16_t y,const char *text,uint16_t color){lcd_text(4,y,text,color,C_BLACK,1);}
static void title(const char *text){lcd_rect(0,0,240,18,C_BLUE);lcd_text(4,4,text,C_WHITE,C_BLUE,1);}
static void redraw(void){
    phase=-40;ncur=0;drawn_mask=drawn_sat=255;drawn_recording=drawn_battery=-1;drawn_graph=INT32_MIN;
    memset(drawn_status,0,sizeof drawn_status);for(int i=0;i<6;i++){drawn_values[i]=INT32_MIN;drawn_types[i]=255;}
}
void gui_msg(const char *a,const char *b,uint16_t ms){
    if(scr!=S_MSG)return_scr=scr;
    snprintf(message[0],sizeof message[0],"%.33s",a);snprintf(message[1],sizeof message[1],"%.33s",b);
    message_until=HAL_GetTick()+ms;scr=S_MSG;redraw();
}
static void result(const char *label,bool ok){gui_msg(label,ok?"CONCLUIDO":app_last_error(),1200);}
static void status_line(unsigned i,uint16_t y,const char *text,uint16_t color){
    if(strcmp(drawn_status[i],text)){lcd_rect(0,y,240,12,C_BLACK);line(y,text,color);snprintf(drawn_status[i],34,"%.33s",text);}
}
static void draw_main(int step){
    const PresetCfg *a=app_applied_config();char b[48];
    if(step==0){title("SG6 - SEIS CANAIS");return;}
    if(step>=1 && step<=4){const char *names[]={"QUASE-ESTATICO","MEDIO","RAPIDO","CUSTOM"};unsigned i=step-1;button(i%2?122:4,20+(i/2)*24,114,22,10+i,names[i],a && !strcmp(a->nome,names[i])?C_GREEN:C_DGRAY);return;}
    if(step>=5 && step<=10){unsigned i=step-5;uint8_t mask=a?a->ch_mask:0;uint8_t type=(uint8_t)ch_bridge[i];
        if(drawn_values[i]==ch_ue[i] && drawn_types[i]==type && (drawn_mask&(1<<i))==(mask&(1<<i)) && (drawn_sat&(1<<i))==(saturation&(1<<i)))return;
        uint16_t y=72+i*18;lcd_rect(2,y,236,16,C_BLACK);
        if(ch_ue[i]==INT32_MAX)snprintf(b,sizeof b,"CH%u %s SEM ZERO%s",i,type==HALF_BENDING?"H":"Q",saturation&(1<<i)?" SAT":"");
        else{int64_t v=ch_ue[i],mag=v<0?-v:v;snprintf(b,sizeof b,"CH%u %s %s%ld.%u ue%s",i,type==HALF_BENDING?"H":"Q",v<0?"-":"+",(long)(mag/10),(unsigned)(mag%10),saturation&(1<<i)?" SAT":"");}
        line(y+2,b,mask&(1<<i)?C_GREEN:C_GRAY);btn_add(2,y,236,16,100+i);drawn_values[i]=ch_ue[i];drawn_types[i]=type;
        drawn_mask=(drawn_mask&~(1<<i))|(mask&(1<<i));drawn_sat=(drawn_sat&~(1<<i))|(saturation&(1<<i));return;
    }
    if(step==11){
        if(drawn_graph==ch_ue[cal_channel])return;
        drawn_graph=ch_ue[cal_channel];int32_t value=drawn_graph==INT32_MAX?0:drawn_graph/833;if(value>12)value=12;if(value< -12)value=-12;graph[graph_pos++%96]=(int8_t)value;
        lcd_rect(2,182,236,38,C_BLACK);snprintf(b,sizeof b,"GRAF%u +/-1000ue H=flex Q=local",cal_channel);line(182,b,C_CYAN);lcd_rect(4,206,230,1,C_DGRAY);
        for(unsigned i=0;i<96;i++)lcd_pix(6+i*2,206-graph[(graph_pos+i)%96],C_YELL);
        btn_add(2,182,236,38,96);return;
    }
    if(step==12){
        bool valid=app_battery_valid(),low=valid && app_battery_warning(),visible=!low || (HAL_GetTick()/500)%2==0;
        uint8_t percent=app_battery_percent();int state=percent | (valid<<8) | (low<<9) | (visible<<10);
        if(drawn_battery!=state){
            drawn_battery=state;lcd_rect(160,0,80,18,C_BLUE);
            if(visible){
                uint16_t color=!valid?C_GRAY:low?C_RED:C_GREEN;
                if(valid)snprintf(b,sizeof b,"~%u%%",percent);else snprintf(b,sizeof b,"~--%%");
                lcd_text(162,4,b,color,C_BLUE,1);lcd_frame(204,4,26,10,color);lcd_rect(230,7,2,4,color);
                for(unsigned i=0;!low && i<(unsigned)(percent+24)/25;i++)lcd_rect(206+i*6,6,4,6,color);
            }
        }
        unsigned exc=(unsigned)(vexc_v*100);
        if(valid)snprintf(b,sizeof b,"EXC%u.%02u BAT%u.%02u SD:%s%s",exc/100,exc%100,batt_mv/1000,(batt_mv/10)%100,sd_ready()?"OK":"FALHA",low?" CARREGAR":"");
        else snprintf(b,sizeof b,"EXC%u.%02u BAT FALHA SD:%s",exc/100,exc%100,sd_ready()?"OK":"FALHA");
        status_line(0,222,b,low || !valid?C_RED:C_YELL);return;
    }
    if(step==13){const char *e=app_last_error();if(*e)snprintf(b,sizeof b,"%.33s",e);else if(recording)snprintf(b,sizeof b,"%.12s %02lu:%02lu",app_log_filename(),(unsigned long)(rec_ms/60000),(unsigned long)(rec_ms/1000%60));else snprintf(b,sizeof b,"SD OCIOSO / FAT DATA PLACEHOLDER");status_line(1,234,b,*e?C_RED:C_WHITE);return;}
    if(step==14){unsigned p=(unsigned)(app_sps_per_ch()*10),m=(unsigned)(app_measured_sps()*10);snprintf(b,sizeof b,"P%u.%u M%u.%u R%u.%u SPS/CH",p/10,p%10,m/10,m%10,m/10,m%10);status_line(2,246,b,C_WHITE);return;}
    if(step==15){unsigned dr=a?(unsigned)(a->drate_sps*10):0,req=a?(unsigned)(a->fc_lp*10):0,eff=(unsigned)(app_effective_cutoff()*10);snprintf(b,sizeof b,"DR%u.%u FC%u.%u EF%u.%u Hz",dr/10,dr%10,req/10,req%10,eff/10,eff%10);status_line(3,258,b,C_CYAN);return;}
    if(step==16){status_line(4,270,"Analog499Hz: alias nao garantido",C_ORAN);return;}
    if(step==17){uint32_t us=app_sd_max_io_us();snprintf(b,sizeof b,"Media%u SDmax%lu.%03lums",a?a->avg:0,(unsigned long)(us/1000),(unsigned long)(us%1000));status_line(5,282,b,C_YELL);return;}
    if(step==18){if(drawn_recording==recording)return;drawn_recording=recording;button(4,300,70,18,30,"ZERO",C_DGRAY);button(80,300,70,18,31,recording?"PARAR":"GRAVAR",recording?C_RED:C_DGRAY);button(156,300,80,18,32,"MENU",C_DGRAY);}
}
static void draw_custom(int step){char b[34];
    switch(step){
    case 0:title("CUSTOM - RASCUNHO / PGA1 FIXO");break;
    case 1:for(unsigned i=0;i<6;i++){snprintf(b,sizeof b,"CH%u",i);button(4+i*39,28,37,23,40+i,b,cfg.ch_mask&(1<<i)?C_GREEN:C_DGRAY);}break;
    case 2:snprintf(b,sizeof b,"DRATE %u.%u SPS",(unsigned)(cfg.drate_sps*10)/10,(unsigned)(cfg.drate_sps*10)%10);row(1,52,b);break;
    case 3:snprintf(b,sizeof b,"FILTRO SOLICITADO %u Hz",(unsigned)cfg.fc_lp);row(2,53,b);break;
    case 4:snprintf(b,sizeof b,"MEDIA MOVEL %u (SEM DECIMAR)",cfg.avg);row(3,54,b);break;
    case 5:snprintf(b,sizeof b,"TRIGGER %s / JANELA",cfg.trig_on?"ON":"OFF");row(4,78,b);break;
    case 6:row(5,57,"CANAIS / CALIBRACAO INDIVIDUAL");break;
    case 7:line(190,"Analog499Hz: alias nao garantido",C_ORAN);break;
    case 8:line(204,"Controlar banda; validar na placa",C_ORAN);break;
    case 9:line(218,"Media atenua e introduz atraso",C_YELL);break;
    case 10:snprintf(b,sizeof b,"ATUAL FC EFETIVA %u.%u Hz",(unsigned)(app_effective_cutoff()*10)/10,(unsigned)(app_effective_cutoff()*10)%10);line(232,b,C_CYAN);break;
    case 11:button(4,292,112,24,56,"APLICAR CUSTOM",C_GREEN);button(124,292,112,24,58,"VOLTAR",C_DGRAY);break;
    }
}
static void draw_cal(int step){char b[34];
    switch(step){
    case 0:title("CALIBRACAO / CANAL SELECIONADO");break;
    case 1:snprintf(b,sizeof b,"CANAL CH%u (TOQUE PARA TROCAR)",cal_channel);row(0,66,b);break;
    case 2:snprintf(b,sizeof b,"TIPO FISICO: %s",ch_bridge[cal_channel]==HALF_BENDING?"MEIA FLEXAO":"QUARTO LOCAL");row(1,67,b);break;
    case 3:snprintf(b,sizeof b,"GF %u.%02u",(unsigned)(ch_gf[cal_channel]*100)/100,(unsigned)(ch_gf[cal_channel]*100)%100);line(82,b,C_CYAN);button(4,98,112,22,79,"GF -0.01",C_DGRAY);button(124,98,112,22,62,"GF +0.01",C_DGRAY);break;
    case 4:snprintf(b,sizeof b,"GANHO FISICO G%u (2..1000)",(unsigned)ch_gain[cal_channel]);line(124,b,C_YELL);button(4,140,112,22,69,"GANHO -",C_DGRAY);button(124,140,112,22,68,"GANHO +",C_DGRAY);break;
    case 5:snprintf(b,sizeof b,"R SHUNT %lu.%u ohm",(unsigned long)(shunt_tenth/10),(unsigned)(shunt_tenth%10));button(4,166,232,22,94,b,C_DGRAY);break;
    case 6:button(4,190,112,22,65,"1 BIAS",C_DGRAY);button(124,190,112,22,60,"2 ZERO",C_DGRAY);break;
    case 7:button(4,216,232,23,61,app_shunt_pending(cal_channel)?"3 REMOVER / RETORNO ZERO":"3 APLICAR SHUNT MANUAL",C_DGRAY);break;
    case 8:snprintf(b,sizeof b,"BIAS:%s ZERO:%s SHUNT:%s",ch_bias_valid&(1<<cal_channel)?"OK":"--",ch_zero_valid&(1<<cal_channel)?"OK":"--",ch_cal_valid&(1<<cal_channel)?"OK":"--");line(244,b,C_CYAN);break;
    case 9:{float low,high;uint8_t bit=1<<cal_channel;
        if(!(ch_bias_valid&ch_zero_valid&bit) || !sg_strain_range(ch_bridge[cal_channel],ch_gain[cal_channel],vexc_v,ch_bias_v[cal_channel],ch_zero_ratio[cal_channel],ch_gf[cal_channel],ch_scale[cal_channel],&low,&high))snprintf(b,sizeof b,"EST INDISP: BIAS/ZERO/EXC");
        else if(fabsf(low)>999999 || fabsf(high)>999999)snprintf(b,sizeof b,"EST >999999 ue: REVER GF/GANHO");
        else snprintf(b,sizeof b,"EST %ld..%ld ue",(long)ceilf(low),(long)floorf(high));
        line(256,b,C_CYAN);line(268,"ESTIMATIVA: BANCADA PENDENTE",C_YELL);break;}
    case 10:line(280,"PONTE COMPLETA INDISPONIVEL",C_ORAN);break;
    case 11:button(4,292,112,24,63,"SALVAR FLASH",C_GREEN);button(124,292,112,24,64,"VOLTAR",C_DGRAY);break;
    }
}
static void draw_menu(int step){
    const char *labels[]={log_is_bin()?"FORMATO BIN":"FORMATO CSV","CANAIS / CALIBRACAO","CUSTOM","PREFIXO / DURACAO","TRIGGER / JANELA","TOQUE: CALIBRAR","SALVAR NA FLASH"};
    const uint8_t ids[]={70,73,74,77,78,76,63};
    if(step==0)title("MENU - CONFIGURACAO");
    else if(step<=7)row(step-1,ids[step-1],labels[step-1]);
    else if(step==8){button(4,238,112,24,72,"BRILHO -",C_DGRAY);button(124,238,112,24,71,"BRILHO +",C_DGRAY);}
    else if(step==9)line(270,"FAT DATA PLACEHOLDER; SEM RTC",C_ORAN);
    else if(step==10)button(4,292,232,24,75,"VOLTAR",C_DGRAY);
}
static void draw_log(int step){char b[34];
    switch(step){case 0:title("ARQUIVO / DURACAO - RASCUNHO");break;
    case 1:snprintf(b,sizeof b,"PREFIXO %s / POSICAO %u",edit_prefix,prefix_pos+1);row(0,82,b);break;
    case 2:row(1,81,"ALTERAR LETRA A..Z / 0..9");break;
    case 3:snprintf(b,sizeof b,"DURACAO %lu s (0=MANUAL)",(unsigned long)edit_duration);line(100,b,C_CYAN);break;
    case 4:row(4,83,"DURACAO +60s / MAX86400");break;
    case 5:row(5,84,"DURACAO -60s");break;
    case 6:line(204,"Nome exclusivo; nao sobrescreve",C_YELL);break;
    case 7:button(4,292,112,24,80,"APLICAR",C_GREEN);button(124,292,112,24,75,"CANCELAR",C_DGRAY);break;}
}
static void draw_trigger(int step){char b[34];
    switch(step){case 0:title("TRIGGER - RASCUNHO CUSTOM");break;
    case 1:snprintf(b,sizeof b,"TRIGGER %s / BORDA SUBIDA",cfg.trig_on?"ON":"OFF");row(0,55,b);break;
    case 2:snprintf(b,sizeof b,"FONTE CH%u",cfg.trig_channel);row(1,85,b);break;
    case 3:snprintf(b,sizeof b,"NIVEL%ld ue / LIM +/-1e6",(long)cfg.trig_lvl_ue);line(84,b,C_CYAN);break;
    case 4:button(4,104,112,24,86,"NIVEL -100",C_DGRAY);button(124,104,112,24,87,"NIVEL +100",C_DGRAY);break;
    case 5:snprintf(b,sizeof b,"PRE %u.%u s / 0..10",(unsigned)(cfg.trig_pre_s*10)/10,(unsigned)(cfg.trig_pre_s*10)%10);line(138,b,C_CYAN);button(4,154,112,24,88,"PRE -0.1",C_DGRAY);button(124,154,112,24,89,"PRE +0.1",C_DGRAY);break;
    case 6:snprintf(b,sizeof b,"POS %u.%u s / 0.1..60",(unsigned)(cfg.trig_post_s*10)/10,(unsigned)(cfg.trig_post_s*10)%10);line(188,b,C_CYAN);button(4,204,112,24,92,"POS -0.1",C_DGRAY);button(124,204,112,24,93,"POS +0.1",C_DGRAY);break;
    case 7:line(242,"Janela no PC; gravacao continua",C_YELL);break;
    case 8:button(4,292,112,24,56,"APLICAR CUSTOM",C_GREEN);button(124,292,112,24,75,"CANCELAR",C_DGRAY);break;}
}
static void draw_confirm(int step){
    char b[34];
    if(step==0)title("CONFIRMAR ACAO FISICA");
    else if(step==1){if(pending==65)line(36,"Desconecte sense do gauge VIN+.",C_YELL);else if(pending==67)line(36,"Altere a ponte fisica do canal.",C_YELL);else if(pending==61){if(app_shunt_pending(pending_channel))snprintf(b,sizeof b,"REMOVA shunt; restaure sensor.");else snprintf(b,sizeof b,"Aplique %lu.%u ohm no gauge.",(unsigned long)(shunt_tenth/10),(unsigned)(shunt_tenth%10));line(36,b,C_YELL);}else line(36,"Sensor restaurado e SEM CARGA.",C_YELL);}
    else if(step==2){line(60,pending==65?"Una VIN+ a VIN- no ponto medio.":"Confira ligacoes antes de medir.",C_YELL);}
    else if(step==3){line(84,pending==65?"Apos BIAS: restaure, ZERO, SHUNT.":"Tara nao sera refeita em presets.",C_CYAN);}
    else if(step==4){button(4,130,112,28,90,"CONFIRMAR",C_GREEN);button(124,130,112,28,91,"CANCELAR",C_DGRAY);}
}
static void draw_shunt(int step){char b[34];
    switch(step){case 0:title("SHUNT MEDIDO - RASCUNHO");break;
    case 1:snprintf(b,sizeof b,"R SHUNT %07lu.%u ohm",(unsigned long)(edit_shunt/10),(unsigned)(edit_shunt%10));line(42,b,C_CYAN);snprintf(b,sizeof b,"DIGITO %u / 8 (ULTIMO=0.1 ohm)",shunt_pos+1);line(64,b,C_YELL);break;
    case 2:row(3,95,"DIGITO +1 (9 VOLTA A 0)");break;
    case 3:row(4,97,"PROXIMO DIGITO");break;
    case 4:line(176,"VALIDO 1.0..999999.9 ohm",C_YELL);line(194,"Reinicia em100000.0 apos boot",C_YELL);break;
    case 5:button(4,292,112,24,98,"APLICAR",C_GREEN);button(124,292,112,24,99,"CANCELAR",C_DGRAY);break;}
}
static int screen_steps(void){const uint8_t counts[]={19,12,12,11,3,8,9,5,4,6};return counts[scr];}
static void draw_step(void){
    if(phase<0){lcd_rect(0,(phase+40)*8,240,8,C_BLACK);phase++;return;}
    if(scr==S_MAIN)draw_main(phase);
    else if(scr==S_CUSTOM)draw_custom(phase);
    else if(scr==S_CAL)draw_cal(phase);
    else if(scr==S_MENU)draw_menu(phase);
    else if(scr==S_LOG)draw_log(phase);
    else if(scr==S_TRIGGER)draw_trigger(phase);
    else if(scr==S_CONFIRM)draw_confirm(phase);
    else if(scr==S_SHUNT)draw_shunt(phase);
    else if(scr==S_MSG){if(phase==0)title("MENSAGEM");else if(phase==1)line(100,message[0],C_WHITE);else if(phase==2)line(124,message[1],C_YELL);else button(4,292,232,24,91,"VOLTAR",C_DGRAY);}
    else if(scr==S_CALT){if(phase==0)title("TOQUE - DOIS PONTOS");else if(phase==1)line(100,"SEGURE A CRUZ; DEPOIS SOLTE",C_WHITE);else{unsigned x=cal_step?210:30,y=cal_step?280:40;lcd_rect(x-10,y,21,2,C_YELL);lcd_rect(x,y-10,2,21,C_YELL);}}
    phase++;
}
static void request(uint8_t id){pending=id;pending_channel=cal_channel;return_scr=scr;const PresetCfg *a=app_applied_config();zero_mask=id==30?(a?a->ch_mask:0):(1<<cal_channel);scr=S_CONFIRM;redraw();}
static bool navigation(uint8_t id){return id==31 || id==32 || id==57 || id==58 || id==64 || id==66 || id==73 || id==74 || id==75 || id==77 || id==78 || id==91 || id==96 || (id>=100 && id<=105);}
static bool shunt_staged(void){for(uint8_t ch=0;ch<6;ch++)if(app_shunt_pending(ch))return true;return false;}
static void handle(uint8_t id){
    if(recording && !navigation(id)){gui_msg("CONFIG BLOQUEADA","PARE REC PRIMEIRO",900);return;}
    if(id>=10 && id<14){bool ok=app_apply_preset(id-10);scr=S_MAIN;result("PRESET",ok);return;}
    if(id>=100 && id<=105){cal_channel=id-100;memset(graph,0,sizeof graph);drawn_graph=INT32_MIN;phase=11;return;}
    if(id>=40 && id<=45){uint8_t mask=cfg.ch_mask^(1<<(id-40));if(!mask){gui_msg("MASCARA VAZIA","MANTENHA UM CANAL",900);return;}cfg.ch_mask=mask;redraw();return;}
    switch(id){
    case 30:case 60:case 61:case 65:case 67:request(id);return;
    case 31:result("REC",recording?app_rec_stop():app_rec_start());return;
    case 32:scr=S_MENU;break;
    case 52:{unsigned i;for(i=0;i<16;i++)if(drcode[i]==cfg.drate_code)break;i=(i+1)%16;cfg.drate_code=drcode[i];cfg.drate_sps=drates[i];break;}
    case 53:{unsigned i;for(i=0;i<9;i++)if(fcs[i]==cfg.fc_lp)break;cfg.fc_lp=fcs[(i+1)%9];break;}
    case 54:cfg.avg=cfg.avg>=128?1:cfg.avg*2;break;
    case 55:cfg.trig_on=!cfg.trig_on;break;
    case 56:preset_id=3;{bool ok=app_reapply();result("CUSTOM",ok);return;}
    case 57:case 73:scr=S_CAL;break;
    case 58:case 64:case 75:scr=S_MAIN;break;
    case 62:case 79:result("GF",app_set_channel_config(cal_channel,ch_bridge[cal_channel],ch_gf[cal_channel]+(id==62?.01f:-.01f),ch_gain[cal_channel]));return;
    case 63:result("FLASH",app_save_cfg());return;
    case 66:cal_channel=(cal_channel+1)%6;break;
    case 68:case 69:{float gain=ch_gain[cal_channel]+(id==68?10:-10);if(gain<2)gain=2;if(gain>1000)gain=1000;result("AJUSTE FISICO + RECALIBRAR",app_set_channel_config(cal_channel,ch_bridge[cal_channel],ch_gf[cal_channel],gain));return;}
    case 70:log_set_bin(!log_is_bin());break;
    case 71:case 72:{int value=app_brightness()+(id==71?10:-10);if(value>=20 && value<=100 && app_set_brightness(value))lcd_backlight(value);break;}
    case 74:if(!recording && app_applied_config())cfg=*app_applied_config();scr=S_CUSTOM;break;
    case 76:cal_step=0;cal_x=cal_y=cal_count=0;scr=S_CALT;break;
    case 77:snprintf(edit_prefix,sizeof edit_prefix,"%s",app_log_prefix());edit_duration=app_log_duration();prefix_pos=0;scr=S_LOG;break;
    case 78:scr=S_TRIGGER;break;
    case 80:result("ARQUIVO",app_set_log_config(edit_prefix,edit_duration));return;
    case 81:{const char *chars="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";const char *p=strchr(chars,edit_prefix[prefix_pos]);edit_prefix[prefix_pos]=chars[p?(p-chars+1)%36:0];edit_prefix[3]=0;break;}
    case 82:prefix_pos=(prefix_pos+1)%3;break;
    case 83:if(edit_duration<=86340)edit_duration+=60;break;
    case 84:edit_duration=edit_duration>=60?edit_duration-60:0;break;
    case 85:cfg.trig_channel=(cfg.trig_channel+1)%6;break;
    case 86:case 87:cfg.trig_lvl_ue+=id==87?100:-100;if(cfg.trig_lvl_ue>1000000)cfg.trig_lvl_ue=1000000;if(cfg.trig_lvl_ue< -1000000)cfg.trig_lvl_ue=-1000000;break;
    case 88:case 89:cfg.trig_pre_s=roundf(cfg.trig_pre_s*10+(id==89?1:-1))/10;if(cfg.trig_pre_s<0)cfg.trig_pre_s=0;if(cfg.trig_pre_s>10)cfg.trig_pre_s=10;break;
    case 92:case 93:cfg.trig_post_s=roundf(cfg.trig_post_s*10+(id==93?1:-1))/10;if(cfg.trig_post_s<.1f)cfg.trig_post_s=.1f;if(cfg.trig_post_s>60)cfg.trig_post_s=60;break;
    case 94:case 95:case 97:case 98:
        if(shunt_staged()){gui_msg("SHUNT EM ANDAMENTO","REMOVA E CONFIRME RETORNO",900);return;}
        if(id==94){edit_shunt=shunt_tenth;shunt_pos=0;scr=S_SHUNT;}
        else if(id==95){uint32_t unit=shunt_digits[shunt_pos],digit=edit_shunt/unit%10;edit_shunt=edit_shunt-digit*unit+(digit+1)%10*unit;}
        else if(id==97)shunt_pos=(shunt_pos+1)%8;
        else{if(edit_shunt<10 || edit_shunt>9999999){gui_msg("SHUNT INVALIDO","USE 1.0..999999.9 ohm",900);return;}shunt_tenth=edit_shunt;scr=S_CAL;}
        break;
    case 99:scr=S_CAL;break;
    case 90:{bool ok=false;uint8_t ch=pending_channel;
        if(pending==30 || pending==60)ok=app_zero(zero_mask);
        else if(pending==65)ok=app_measure_bias(ch);
        else if(pending==67)ok=app_set_channel_config(ch,ch_bridge[ch]==HALF_BENDING?QUARTER:HALF_BENDING,ch_gf[ch],ch_gain[ch]);
        else if(pending==61)ok=app_shunt_pending(ch)?app_shunt_finish(ch):app_shunt(ch,shunt_tenth/10.0f);
        scr=return_scr;result("CALIBRACAO",ok);pending=0;return;}
    case 91:scr=return_scr;pending=0;break;
    case 96:cal_channel=(cal_channel+1)%6;memset(graph,0,sizeof graph);drawn_graph=INT32_MIN;phase=11;return;
    default:return;
    }redraw();
}
void gui_init(void){lcd_backlight(app_brightness());scr=S_MAIN;redraw();}
void gui_tick(void){
    uint32_t t=HAL_GetTick();uint64_t start=app_time_us();
    if(scr==S_MSG && (int32_t)(t-message_until)>=0){scr=return_scr;redraw();}
    if(scr==S_CALT && phase>=screen_steps()){
        if(recording){scr=S_MENU;gui_msg("TOQUE BLOQUEADO","PARE REC",900);return;}
        int32_t x,y;if(touch_raw(&x,&y)){if(cal_count<32){cal_x+=x;cal_y+=y;cal_count++;}}
        else if(!touch_pressed() && cal_count){
            if(cal_count>=7){x=cal_x/cal_count;y=cal_y/cal_count;
                if(!cal_step){first_x=x;first_y=y;cal_step=1;redraw();}
                else{int32_t dx=x-first_x,dy=y-first_y;int16_t p[4]={(int16_t)(first_x-30*dx/180),(int16_t)(first_y-40*dy/240),(int16_t)(first_x+209*dx/180),(int16_t)(first_y+279*dy/240)};
                    scr=S_MENU;if(touch_calib_valid(p)){touch_calib_set(p[0],p[1],p[2],p[3]);gui_msg("TOQUE CALIBRADO","SALVE FLASH NO MENU",1200);}else gui_msg("TOQUE INVALIDO","REPITA OS DOIS PONTOS",1200);
                }
            }cal_x=cal_y=cal_count=0;
        }return;
    }
    if(t-last_touch>=180 && phase>=0){uint16_t x,y;if(touch_read(&x,&y)){last_touch=t;for(unsigned i=0;i<ncur;i++)if(x>=cur[i].x && x<cur[i].x+cur[i].w && y>=cur[i].y && y<cur[i].y+cur[i].h){handle(cur[i].id);break;}}}
    unsigned refresh=app_battery_warning() && refresh_ms>500?500:refresh_ms;
    if(scr==S_MAIN && phase>=screen_steps() && t-last_draw>=refresh){last_draw=t;saturation|=app_take_saturation();phase=5;if(app_ui_pressure())refresh_ms=1000;}
    if(phase<screen_steps() && !app_ui_pressure())draw_step();
    if(app_time_us()-start>=20000 || app_ui_pressure())refresh_ms=1000;
}




