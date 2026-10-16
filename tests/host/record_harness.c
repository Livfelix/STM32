#include "sg_record.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
int main(int argc,char **argv) {
    assert(sg_crc16((const uint8_t*)"123456789",9)==0x29B1);
    uint8_t b[46]; int32_t v[6]={-12345,111,222,67890,444,555};
    sg_encode_record(b,1,0x100000001ULL,v,5000,3700,0x09,0x48);
    assert(b[0]==0xAA && b[1]==0x56 && b[2]==1);
    assert(b[6]==1 && b[10]==1);
    assert(b[14]==0xC7 && b[15]==0xCF && b[16]==0xFF && b[17]==0xFF);
    assert(b[18]==0xFF && b[19]==0xFF && b[20]==0xFF && b[21]==0x7F);
    assert(b[26]==0x32 && b[27]==0x09 && b[28]==1 && b[29]==0);
    assert(b[42]==0x09 && b[43]==0x48);
    assert(SG_FOOTER_SIZE==21);
    assert(STOP_NORMAL==0 && DURATION==1 && LOW_BATTERY==2 && BAT_MONITOR_FAIL==3);
    uint8_t footer[SG_FOOTER_SIZE];
    sg_encode_footer(footer,2,1,0x100000001ULL,LOW_BATTERY);
    const uint8_t expected[19]={0xAA,0x57,2,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,2};
    for(unsigned i=0;i<19;i++)assert(footer[i]==expected[i]);
    assert(sg_crc16(footer,19)==(uint16_t)(footer[19]|footer[20]<<8));
    if(argc>2){FILE *f=fopen(argv[2],"wb");assert(f);assert(fwrite(footer,1,21,f)==21);assert(!fclose(f));}
    sg_encode_footer(footer,0,UINT32_MAX,0,STOP_NORMAL);
    assert(footer[2]==0 && footer[6]==0xFF && footer[9]==0xFF && footer[10]==0 && footer[18]==0);
    if(argc>1) { FILE *f=fopen(argv[1],"wb"); assert(f); assert(fwrite(b,1,46,f)==46); assert(!fclose(f)); }
    return 0;
}
