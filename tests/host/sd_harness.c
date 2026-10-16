#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "stm32f1xx_hal.h"
#include "../../Projeto_Modulo_Portatil_StrainGauge/fw_pio/src/sd_diskio.c"

GPIO_TypeDef gpio_a, gpio_b, gpio_c;
static SPI_TypeDef spi1;
SPI_HandleTypeDef hspi1 = { .Instance = &spi1 };
static uint64_t now;
static unsigned qhead, qtail, cpos, write_phase, remaining, busy, acmd_calls;
static uint8_t queue[1100], command[6], written[1024], csd[16];
static unsigned nwritten, cmd16_calls, commands[64];
static uint32_t last_arg[64], first_arg[64], ocr;
static int selected, v2, hc, bad_r7, fail_spi, permanent_busy, missing_token;
static int reject_token, reject_write, reject_cmd16, reject_ocr, init_stuck, no_response;
static int busy_after_write, fail_after_command, fail_data, fail_release;
static uint64_t busy_until;
static unsigned write_busy_ms, write_busy_second_ms;

uint64_t app_time_us(void) { return now; }
uint32_t HAL_GetTick(void) { now += 1000; return (uint32_t)(now / 1000); }
void __disable_irq(void) { assert(!"SD disabled acquisition interrupts"); }
static void push(uint8_t v) { assert(qtail < sizeof queue); queue[qtail++] = v; }
static void data_block(unsigned n) {
    if (missing_token) return;
    push(0xff); push(reject_token ? 0x0b : 0xfe);
    if (reject_token) return;
    for (unsigned i=0; i<n; ++i) push(n==16 ? csd[i] : (uint8_t)(i ^ 0x5a));
    push(0xff); push(0xff);
}
static void process_command(void) {
    unsigned c=command[0]&63;
    assert((spi1.CR1&~SPI_CR1_BR)==0x401u);
    assert((spi1.CR1&SPI_CR1_BR)==(stat&STA_NOINIT ? SPI_BAUDRATEPRESCALER_256 : SPI_BAUDRATEPRESCALER_8));
    uint32_t arg=((uint32_t)command[1]<<24)|((uint32_t)command[2]<<16)|((uint32_t)command[3]<<8)|command[4];
    last_arg[c]=arg; if(!commands[c])first_arg[c]=arg; commands[c]++;
    if (no_response) return;
    switch(c) {
    case 0: assert(command[5]==0x95);push(1); break;
    case 8:
        assert(arg==0x1aa && command[5]==0x87);
        push(v2 ? 1 : 5);
        if(v2) { push(0); push(0); push(1); push(bad_r7 ? 0xab : 0xaa); }
        break;
    case 55: push(acmd_calls ? 0 : 1); break;
    case 41: assert(arg==(v2 ? 0x40000000u : 0));acmd_calls++; push(init_stuck ? 1 : 0); break;
    case 58: push(reject_ocr ? 4 : 0); for(int i=3;i>=0;--i) push((uint8_t)(ocr>>(8*i))); break;
    case 16: cmd16_calls++; push(reject_cmd16 ? 4 : 0); break;
    case 17: push(0); data_block(512); break;
    case 9: push(0); data_block(16); break;
    case 24: push(0); write_phase=1; break;
    default: push(4); break;
    }
}
void HAL_GPIO_WritePin(GPIO_TypeDef *p,uint16_t pin,GPIO_PinState state) {
    if(p==SD_CS_PORT && pin==SD_CS_PIN) {
        selected=state==GPIO_PIN_RESET;
        if(!selected) { qhead=qtail=cpos=write_phase=0; }
    }
}
static uint8_t exchange(uint8_t tx) {
    now+=100;
    assert(now < 8000000 && "driver exceeded bounded deadline");
    if(!selected) return 0xff;
    if(qhead<qtail) {
        uint8_t r=queue[qhead++];
        if(qhead==qtail) qhead=qtail=0;
        return r;
    }
    if(write_phase==1) { if(tx==0xfe) { write_phase=2;remaining=512; } return 0xff; }
    if(write_phase==2) { assert(nwritten<sizeof written);written[nwritten++]=tx;if(!--remaining){write_phase=3;remaining=2;} return 0xff; }
    if(write_phase==3) { if(!--remaining){unsigned ms=commands[24]>1?write_busy_second_ms:write_busy_ms;write_phase=0;push(reject_write ? 0x0b : 0x05);busy=ms?0:2;busy_until=now+100+ms*1000;permanent_busy=busy_after_write;} return 0xff; }
    if(now<busy_until) return 0;
    if(permanent_busy || busy) { if(busy)busy--; return 0; }
    if(cpos || (tx&0xc0)==0x40) {
        command[cpos++]=tx;
        if(cpos==6){cpos=0;process_command();}
    }
    return 0xff;
}
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *h,uint8_t *tx,uint8_t *rx,uint16_t len,uint32_t timeout) {
    assert(h==&hspi1 && timeout>0);
    if(fail_spi || (fail_data && len>1) || (fail_after_command && commands[17]) || (fail_release && !selected)) { now+=100; return HAL_ERROR; }
    for(unsigned i=0;i<len;i++) rx[i]=exchange(tx[i]);
    return HAL_OK;
}
static void reset_card(int version,int high_capacity) {
    now=0;qhead=qtail=cpos=write_phase=remaining=busy=acmd_calls=nwritten=cmd16_calls=0;
    selected=bad_r7=fail_spi=permanent_busy=missing_token=reject_token=reject_write=reject_cmd16=reject_ocr=init_stuck=no_response=0;
    busy_after_write=fail_after_command=fail_data=fail_release=0;
    busy_until=write_busy_ms=write_busy_second_ms=0;
    memset(commands,0,sizeof commands);memset(last_arg,0,sizeof last_arg);memset(csd,0,sizeof csd);
    v2=version;hc=high_capacity;ocr=hc ? 0xc0ff8000 : 0x80ff8000;
    spi1.CR1=0x401u|SPI_BAUDRATEPRESCALER_32;
    if(hc){csd[0]=0x40;csd[7]=1;csd[8]=0x23;csd[9]=0x45;}
    else {csd[5]=9;csd[6]=0;csd[7]=0xff;csd[8]=0xc0;}
}
static void init_card(int version,int high_capacity) { reset_card(version,high_capacity);assert(disk_initialize(0)==0); }
static uint32_t hash(const uint8_t *p,unsigned n) { uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h; }
static void address_case(int version,int high_capacity) {
    uint8_t b[1024];init_card(version,high_capacity);
    assert(disk_read(0,b,7,2)==RES_OK);assert(first_arg[17]==(hc?7:3584));assert(last_arg[17]==(hc?8:4096));
    for(unsigned i=0;i<sizeof b;i++)assert(b[i]==(uint8_t)((i%512)^0x5a));
    assert(commands[17]==2);
    memset(b,0x33,sizeof b);assert(disk_write(0,b,7,1)==RES_OK);
    assert(last_arg[24]==(hc?7:3584));assert(commands[24]==1);
    assert(cmd16_calls==(hc?0:1));assert(commands[58]==1);
}
int main(int argc,char **argv) {
    assert(argc==2);uint8_t b[512];memset(b,0x69,sizeof b);
    if(!strcmp(argv[1],"sdsc")){address_case(0,0);address_case(1,0);}
    else if(!strcmp(argv[1],"sdhc")) address_case(1,1);
    else if(!strcmp(argv[1],"immutable")) {
        init_card(1,1);uint32_t before=hash(b,512);assert(disk_write(0,b,7,1)==RES_OK);
        assert(hash(b,512)==before && "TX buffer was overwritten");assert(!memcmp(written,b,512));
    } else if(!strcmp(argv[1],"timeouts")) {
        init_card(1,1);reject_token=1;assert(disk_read(0,b,7,1)==RES_ERROR);assert(!selected);
        init_card(1,1);missing_token=1;uint64_t start=now;assert(disk_read(0,b,7,1)==RES_ERROR);assert(now-start<300000);
        init_card(1,1);reject_write=1;assert(disk_write(0,b,7,1)==RES_ERROR);assert(!selected);
        init_card(1,1);busy_after_write=1;start=now;assert(disk_write(0,b,7,1)==RES_ERROR);assert(now-start<700000);assert(!selected);
        init_card(1,1);permanent_busy=1;start=now;assert(disk_ioctl(0,CTRL_SYNC,NULL)==RES_ERROR);assert(now-start<600000);
        init_card(1,1);no_response=1;start=now;assert(disk_read(0,b,7,1)==RES_ERROR);assert(now-start<100000);
        reset_card(1,1);init_stuck=1;assert(disk_initialize(0)&STA_NOINIT);assert(now<2000000);
    } else if(!strcmp(argv[1],"fast_busy")) {
        for(unsigned ms=69;ms<=71;ms++){
            init_card(1,1);sd_recording_mode(true);sd_clear_busy_timeout();write_busy_ms=ms;
            assert(disk_write(0,b,7,1)==(ms<=70?RES_OK:RES_ERROR));
            assert(sd_max_busy_ms()==(ms<=70?ms:70));assert(!selected);
            assert(sd_busy_timed_out()==(ms>70));
        }
        init_card(1,1);sd_recording_mode(true);busy_until=now+70000;
        assert(disk_ioctl(0,CTRL_SYNC,NULL)==RES_OK && sd_max_busy_ms()==70);
        init_card(1,1);sd_recording_mode(true);busy_until=now+71000;
        assert(disk_ioctl(0,CTRL_SYNC,NULL)==RES_ERROR && sd_max_busy_ms()==70);
        sd_recording_mode(false);assert(disk_ioctl(0,CTRL_SYNC,NULL)==RES_OK);
        init_card(1,1);sd_recording_mode(true);busy_until=now+71000;
        assert(disk_read(0,b,7,1)==RES_OK); /* Read commands keep their normal deadline. */
        reset_card(1,1);sd_recording_mode(true);busy_until=now+100000;
        assert(disk_initialize(0)==0); /* Initialization keeps its normal deadline, too. */
        init_card(1,1);sd_recording_mode(false);write_busy_ms=71;
        assert(disk_write(0,b,7,1)==RES_OK && sd_max_busy_ms()==71);
    } else if(!strcmp(argv[1],"busy_cause")) {
        init_card(1,1);sd_recording_mode(true);sd_clear_busy_timeout();write_busy_ms=70;
        assert(disk_write(0,b,7,1)==RES_OK && sd_max_busy_ms()==70 && !sd_busy_timed_out());
        reject_write=1;assert(disk_write(0,b,8,1)==RES_ERROR && !sd_busy_timed_out());
        fail_spi=1;assert(disk_ioctl(0,CTRL_SYNC,NULL)==RES_ERROR && !sd_busy_timed_out());
        init_card(1,1);sd_recording_mode(true);sd_clear_busy_timeout();write_busy_ms=71;
        assert(disk_write(0,b,7,1)==RES_ERROR && sd_busy_timed_out());
        assert(disk_ioctl(0,CTRL_SYNC,NULL)==RES_OK && sd_busy_timed_out()); /* Sticky across one FatFs call. */
        sd_clear_busy_timeout();reject_write=1;
        assert(disk_write(0,b,8,1)==RES_ERROR && !sd_busy_timed_out() && sd_max_busy_ms()==70);
    } else if(!strcmp(argv[1],"cumulative_busy")) {
        uint8_t blocks[1024];memset(blocks,0x69,sizeof blocks);
        init_card(1,1);sd_recording_mode(true);write_busy_ms=write_busy_second_ms=35;
        assert(disk_write(0,blocks,7,2)==RES_OK);
        init_card(1,1);sd_recording_mode(true);write_busy_ms=35;write_busy_second_ms=36;
        assert(disk_write(0,blocks,7,2)==RES_ERROR && sd_max_busy_ms()==70);assert(!selected);
    } else if(!strcmp(argv[1],"ioctl")) {
        struct { DWORD before,value,after; } guard={0x12345678,0xffffffff,0x87654321};
        init_card(1,1);assert(disk_ioctl(0,GET_BLOCK_SIZE,&guard.value)==RES_OK);
        assert(guard.value==1 && "GET_BLOCK_SIZE must write DWORD");assert(guard.before==0x12345678 && guard.after==0x87654321);
        WORD size=0;assert(disk_ioctl(0,GET_SECTOR_SIZE,&size)==RES_OK);assert(size==512);
        LBA_t sectors=0;assert(disk_ioctl(0,GET_SECTOR_COUNT,&sectors)==RES_OK);assert(sectors==76355584);
        init_card(0,0);assert(disk_ioctl(0,GET_SECTOR_COUNT,&sectors)==RES_OK);assert(sectors==4096);
        csd[0]=0x80;assert(disk_ioctl(0,GET_SECTOR_COUNT,&sectors)==RES_ERROR);
        init_card(1,1);memset(csd+7,0xff,3);assert(disk_ioctl(0,GET_SECTOR_COUNT,&sectors)==RES_ERROR);
        init_card(1,1);missing_token=1;assert(disk_ioctl(0,GET_SECTOR_COUNT,&sectors)==RES_ERROR);
    } else if(!strcmp(argv[1],"init")) {
        reset_card(1,1);bad_r7=1;assert(disk_initialize(0)&STA_NOINIT);
        reset_card(1,1);ocr=0x40ff8000;assert(disk_initialize(0)&STA_NOINIT);
        reset_card(1,1);ocr=0xc0000000;assert(disk_initialize(0)&STA_NOINIT);
        reset_card(1,1);reject_ocr=1;assert(disk_initialize(0)&STA_NOINIT);
        reset_card(0,0);reject_cmd16=1;assert(disk_initialize(0)&STA_NOINIT);
    } else if(!strcmp(argv[1],"hal")) {
        init_card(1,1);fail_spi=1;assert(disk_read(0,b,0,1)==RES_ERROR);
        assert(disk_write(0,b,0,1)==RES_ERROR);assert(disk_ioctl(0,CTRL_SYNC,NULL)==RES_ERROR);
        assert(disk_initialize(0)&STA_NOINIT);assert(!selected);
        init_card(1,1);fail_after_command=1;assert(disk_read(0,b,0,1)==RES_ERROR);
        init_card(1,1);fail_data=1;assert(disk_read(0,b,0,1)==RES_ERROR);assert(disk_write(0,b,0,1)==RES_ERROR);
        init_card(1,1);fail_release=1;assert(disk_read(0,b,0,1)==RES_ERROR);assert(!selected);
    } else if(!strcmp(argv[1],"parameters")) {
        reset_card(1,1);assert(disk_read(0,b,0,1)==RES_NOTRDY);assert(disk_ioctl(0,GET_BLOCK_SIZE,b)==RES_NOTRDY);
        init_card(1,1);assert(disk_read(1,b,0,1)==RES_PARERR);assert(disk_write(0,b,0,0)==RES_PARERR);
        assert(disk_read(0,NULL,0,1)==RES_PARERR);assert(disk_ioctl(0,GET_BLOCK_SIZE,NULL)==RES_PARERR);
        assert(disk_ioctl(1,CTRL_SYNC,NULL)==RES_PARERR);assert(disk_status(1)&STA_NOINIT);
        assert(disk_read(0,b,0xffffffffu,2)==RES_PARERR);
        init_card(0,0);assert(disk_write(0,b,8388608u,1)==RES_PARERR);
    } else if(!strcmp(argv[1],"shared")) {
        init_card(1,1);assert(spi1.CR1==(0x401u|SPI_BAUDRATEPRESCALER_32));assert(disk_read(0,b,7,1)==RES_OK);
        assert(!selected && spi1.CR1==(0x401u|SPI_BAUDRATEPRESCALER_32));
        missing_token=1;assert(disk_read(0,b,7,1)==RES_ERROR);assert(!selected && spi1.CR1==(0x401u|SPI_BAUDRATEPRESCALER_32));
    } else assert(!"unknown case");
    puts("OK");return 0;
}
