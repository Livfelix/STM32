#ifndef APP_H
#define APP_H
#include <stdint.h>
#include "sg_measure.h"

typedef struct {
    const char *nome;
    uint8_t  ch_mask;        /* bits 0..5 */
    uint8_t  pga;
    uint8_t  drate_code;
    float    drate_sps;      /* nominal, including 2.5 SPS */
    float    fc_lp;          /* Hz, 0=OFF */
    uint8_t  avg;
    uint8_t  trig_on;
    float    trig_lvl_ue;
    float    trig_post_s;
    uint8_t  trig_channel;  /* CH0..5; extraction on PC, no pretrigger RAM buffer. */
    float    trig_pre_s;    /* CUSTOM0..10s; post0.1..60s; defaults0.5/2s. */
} PresetCfg;

extern PresetCfg cfg;                /* GUI draft; app_reapply commits validated values. */
const PresetCfg *app_applied_config(void); /* Read-only committed values; NULL if hardware apply failed. */
extern int       preset_id;         /* 0..3 */
extern int       n_act;
extern float     ch_gf[6];          /* gauge factor por canal */
extern float     ch_scale[6];       /* fator shunt-cal */
extern BridgeType ch_bridge[6];
extern float ch_gain[6], ch_bias_v[6], ch_zero_ratio[6];
extern uint8_t ch_bias_valid, ch_zero_valid, ch_cal_valid;
extern int32_t   ch_ue[6];          /* 0,1 µε p/ display */
extern float     ch_vout[6];        /* V na saída do INA (p/ display) */
extern uint8_t   sat_flags;
extern float     vexc_v;
extern uint16_t  batt_mv;
extern uint32_t  rec_ms;            /* tempo gravando */
extern volatile int recording;

void app_init(void);
void app_poll(void);                /* foreground DSP, logging, battery and UART */
bool app_apply_preset(int id);
bool app_reapply(void);             /* re-aplica cfg editada no CUSTOM */
void app_drdy_irq(void);
bool app_rec_start(void);
bool app_rec_stop(void);
const char *app_last_error(void);
bool app_set_log_config(const char *prefix, uint32_t duration_s);
const char *app_log_filename(void);
const char *app_log_prefix(void);
uint32_t app_log_duration(void);
float app_measured_sps(void);
uint32_t app_uart_dropped(void);
uint32_t app_uart_trigger_lost(void);
uint8_t app_take_saturation(void);
bool app_battery_warning(void);
uint8_t app_battery_percent(void); /* Approximate voltage curve; 0 if invalid. */
bool app_battery_valid(void);
bool app_measure_bias(uint8_t channel); /* VIN+ tied to VIN- at reference midpoint. */
bool app_zero(uint8_t channel_mask); /* Explicit caller confirmation; gauge sense restored. */
bool app_shunt(uint8_t channel,float shunt_ohm); /* Measures BF350 shunt, stages scale. */
bool app_shunt_finish(uint8_t channel); /* Commit only after physical shunt removal. */
bool app_shunt_pending(uint8_t channel);
bool app_set_channel_config(uint8_t channel,BridgeType type,float gf,float gain);
void app_rec_toggle(void);
bool app_save_cfg(void);
bool app_load_cfg(void);
float app_effective_cutoff(void);
bool app_ui_pressure(void);
uint8_t app_brightness(void);
bool app_set_brightness(uint8_t percent);
void uart_puts(const char*s);
float app_sps_per_ch(void);
int  sd_ready(void);
void sd_recording_mode(bool fast);
uint32_t sd_max_busy_ms(void); /* Largest cumulative write/sync busy time, rounded up. */
uint32_t app_sd_max_io_us(void); /* Per-recording max elapsed foreground f_write/f_sync; saturates at UINT32_MAX. */
void sd_clear_busy_timeout(void); /* Clear before a FatFs operation; latched across its disk calls. */
bool sd_busy_timed_out(void);
int  log_is_bin(void);
void log_set_bin(int b);
void rtc_set(int yy,int mo,int dd,int hh,int mi,int ss);
#endif
