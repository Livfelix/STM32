#ifndef ADS1256_H
#define ADS1256_H
#include <stdint.h>
#include <stdbool.h>
/* DRATE[7:0] reais (SBAS288) */
#define DR_30000 0xF0
#define DR_15000 0xE0
#define DR_7500  0xD0
#define DR_3750  0xC0
#define DR_2000  0xB0
#define DR_1000  0xA1
#define DR_500   0x92
#define DR_100   0x82
#define DR_60    0x72
#define DR_50    0x63
#define DR_30    0x53
#define DR_25    0x43
#define DR_15    0x33
#define DR_10    0x23
#define DR_5     0x13
#define DR_2_5   0x03

bool ads_init(void);
bool ads_set_drate(uint8_t code); /* Validates, reads back and self-calibrates; PGA=1. */
bool ads_selfcal(void);
bool ads_start_channel(uint8_t channel);
bool ads_read_ready(int32_t *code); /* Nonblocking; false never changes *code. */
#endif
