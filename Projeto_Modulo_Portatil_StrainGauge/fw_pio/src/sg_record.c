#include "sg_record.h"
#include <limits.h>
uint16_t sg_crc16(const uint8_t *data,size_t length) {
    uint16_t crc=0xFFFF;
    for(size_t i=0;i<length;i++) {
        crc^=(uint16_t)data[i]<<8;
        for(int bit=0;bit<8;bit++) crc=(crc&0x8000)?(uint16_t)((crc<<1)^0x1021):(uint16_t)(crc<<1);
    }
    return crc;
}
static void le(uint8_t *out,uint64_t value,int bytes) {
    for(int i=0;i<bytes;i++) { out[i]=(uint8_t)value; value>>=8; }
}
void sg_encode_footer(uint8_t out[SG_FOOTER_SIZE],uint32_t count,uint32_t last_seq,uint64_t last_time,uint8_t status) {
    out[0]=0xAA;out[1]=0x57;
    le(out+2,count,4);le(out+6,last_seq,4);le(out+10,last_time,8);
    out[18]=status;le(out+19,sg_crc16(out,19),2);
}
void sg_encode_record(uint8_t out[46],uint32_t seq,uint64_t time_us,const int32_t values[6],uint16_t vexc_mv,uint16_t battery_mv,uint8_t mask,uint8_t flags) {
    out[0]=0xAA; out[1]=0x56;
    le(out+2,seq,4); le(out+6,time_us,8);
    for(int i=0;i<6;i++) le(out+14+i*4,(uint32_t)((mask&(1u<<i))?values[i]:INT32_MAX),4);
    le(out+38,vexc_mv,2); le(out+40,battery_mv,2);
    out[42]=mask; out[43]=flags; le(out+44,sg_crc16(out,44),2);
}
