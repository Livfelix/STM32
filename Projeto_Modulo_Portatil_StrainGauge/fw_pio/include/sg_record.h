#ifndef SG_RECORD_H
#define SG_RECORD_H
#include <stdint.h>
#include <stddef.h>
#define SG_RECORD_SIZE 46
#define SG_FOOTER_SIZE 21
typedef enum { STOP_NORMAL=0, DURATION=1, LOW_BATTERY=2, BAT_MONITOR_FAIL=3 } SgStopStatus;
#define SG_FLAG_TRIGGER 0x40
#define SG_FLAG_INCOMPLETE 0x80
uint16_t sg_crc16(const uint8_t *data,size_t length);
void sg_encode_footer(uint8_t out[SG_FOOTER_SIZE],uint32_t count,uint32_t last_seq,uint64_t last_time,uint8_t status);
void sg_encode_record(uint8_t out[46],uint32_t seq,uint64_t time_us,const int32_t values[6],uint16_t vexc_mv,uint16_t battery_mv,uint8_t mask,uint8_t flags);
#endif
