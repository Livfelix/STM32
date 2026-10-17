/* sd_diskio.c — diskio FatFs p/ cartão SD em modo SPI (SPI1, CS=PA8)
 * e get_fattime() com data/hora setáveis pela tela. */
#include "app_cfg.h"
#include "ff.h"
#include "diskio.h"
#include <string.h>

extern SPI_HandleTypeDef hspi1;
static volatile DSTATUS stat = STA_NOINIT;
static uint8_t block_address, spi_failed;
#define SD_READY_US 500000u
#define SD_TOKEN_US 200000u
#define SD_INIT_US 1200000u
static bool fast_recording, write_operation, busy_timeout;
static uint32_t max_busy_us, operation_busy_us;
void sd_recording_mode(bool fast){fast_recording=fast;if(fast)max_busy_us=0;}
uint32_t sd_max_busy_ms(void){return (max_busy_us+999u)/1000u;}
void sd_clear_busy_timeout(void){busy_timeout=false;}
bool sd_busy_timed_out(void){return busy_timeout;}

/* data/hora p/ FAT (setada pela tela; default build) */
static int g_yy=2026, g_mo=10, g_dd=13, g_hh=0, g_mi=0, g_ss=0;
void rtc_set(int yy,int mo,int dd,int hh,int mi,int ss){
    g_yy=yy; g_mo=mo; g_dd=dd; g_hh=hh; g_mi=mi; g_ss=ss;
}
DWORD get_fattime(void){
    return ((DWORD)(g_yy-1980)<<25)|((DWORD)g_mo<<21)|((DWORD)g_dd<<16)|
           ((DWORD)g_hh<<11)|((DWORD)g_mi<<5)|((DWORD)g_ss>>1);
}

static void sd_cs(int v){ HAL_GPIO_WritePin(SD_CS_PORT,SD_CS_PIN, v?GPIO_PIN_SET:GPIO_PIN_RESET); }
static uint8_t spi_io(uint8_t b){
    uint8_t r=0xFF;
    if(!spi_failed && HAL_SPI_TransmitReceive(&hspi1,&b,&r,1,20)!=HAL_OK) spi_failed=1;
    return r;
}
static int spi_bytes(const uint8_t *tx,uint8_t *rx,UINT n){
    uint8_t out[32], discard[32];
    while(n && !spi_failed){
        UINT k=n>sizeof out ? sizeof out : n;
        if(tx) memcpy(out,tx,k); else memset(out,0xFF,k);
        if(HAL_SPI_TransmitReceive(&hspi1,out,rx?rx:discard,(uint16_t)k,20)!=HAL_OK) spi_failed=1;
        if(tx) tx+=k;
        if(rx) rx+=k;
        n-=k;
    }
    return !spi_failed;
}
static void sd_rate(uint32_t rate){
    __HAL_SPI_DISABLE(&hspi1);
    MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_BR, rate);
    __HAL_SPI_ENABLE(&hspi1);
}
static uint32_t sd_begin(uint32_t rate){
    uint32_t previous=hspi1.Instance->CR1 & SPI_CR1_BR;
    spi_failed=0;
    write_operation=false;operation_busy_us=0;
    HAL_GPIO_WritePin(LCD_CS_PORT,LCD_CS_PIN,GPIO_PIN_SET);
    HAL_GPIO_WritePin(T_CS_PORT,T_CS_PIN,GPIO_PIN_SET);
    sd_cs(1);
    sd_rate(rate);
    return previous;
}
static int cmd_end(void){ sd_cs(1); spi_io(0xFF); return !spi_failed; }
static DRESULT sd_end(uint32_t previous,DRESULT result){
    if(!cmd_end()) result=RES_ERROR;
    sd_rate(previous);
    return result;
}
static int wait_ready(void){
    uint64_t start=app_time_us();
    uint32_t limit=write_operation && fast_recording?70000u-operation_busy_us:SD_READY_US;
    uint64_t elapsed=0;bool busy_seen=false;int ready=0;
    do {
        uint8_t r=spi_io(0xFF);
        if(r!=0xFF)busy_seen=true;
        elapsed=busy_seen?app_time_us()-start:0;
        if(spi_failed || elapsed>limit)break;
        if(r==0xFF){ready=1;break;}
        if(elapsed>=limit)break;
    } while(1);
    if(write_operation && !ready && !spi_failed)busy_timeout=true;
    if(write_operation){
        operation_busy_us+=(uint32_t)elapsed;
        if(operation_busy_us>max_busy_us)max_busy_us=operation_busy_us;
    }
    return ready;
}
static uint8_t wait_token(void){
    uint64_t start=app_time_us();
    do {
        uint8_t r=spi_io(0xFF);
        if(spi_failed || r!=0xFF) return r;
    } while(app_time_us()-start<SD_TOKEN_US);
    return 0xFF;
}

static uint8_t cmd(uint8_t c,uint32_t arg,uint8_t crc){
    if(!cmd_end()) return 0xFF;
    sd_cs(0);
    if(!wait_ready()) return 0xFF;
    spi_io(c|0x40);
    spi_io(arg>>24); spi_io(arg>>16); spi_io(arg>>8); spi_io(arg);
    spi_io(crc);
    for(int i=0;i<10 && !spi_failed;i++){ uint8_t r=spi_io(0xFF); if(!(r&0x80)) return r; }
    return 0xFF;
}

DSTATUS disk_status(BYTE p){ return p ? STA_NOINIT : stat; }

DSTATUS disk_initialize(BYTE p){
    if(p) return STA_NOINIT;
    stat=STA_NOINIT;
    block_address=0;
    uint32_t previous=sd_begin(SPI_BAUDRATEPRESCALER_256); /* ~280 kHz init */
    sd_cs(1); for(int i=0;i<20;i++) spi_io(0xFF);      /* >74 clocks */
    if(cmd(0,0,0x95)!=0x01) goto done;
    uint8_t r8=cmd(8,0x1AA,0x87);
    int sdx2=0;
    if(r8==0x01){
        uint8_t r7[4];
        if(!spi_bytes(NULL,r7,4) || r7[0] || r7[1] || r7[2]!=1 || r7[3]!=0xAA) goto done;
        sdx2=1;
    }else if(r8!=0x05) goto done; /* SD v1: idle + illegal CMD8. */
    if(!cmd_end()) goto done;
    uint64_t start=app_time_us();
    for(;;){
        uint8_t r=cmd(55,0,0x65);
        if(r>1) goto done;
        r=cmd(41,sdx2?0x40000000:0,0x77);
        if(!cmd_end()) goto done;
        if(r==0) break;
        if(r!=1 || app_time_us()-start>=SD_INIT_US) goto done;
    }
    if(cmd(58,0,0xFD)!=0) goto done;
    uint8_t ocr[4];
    if(!spi_bytes(NULL,ocr,4) || !(ocr[0]&0x80) || !(ocr[1]&0x30)) goto done;
    block_address=(ocr[0]&0x40)!=0;
    if(block_address && !sdx2) goto done;
    if(!block_address && cmd(16,512,0x15)!=0) goto done;
    stat=0;
done:
    if(sd_end(previous,RES_OK)!=RES_OK) stat=STA_NOINIT;
    return stat;
}

static int valid_range(LBA_t sec,UINT n){
    uint32_t max=block_address ? UINT32_MAX : UINT32_MAX/512;
    return n && sec<=max && n-1<=max-sec;
}
static int read_block(uint8_t *buff,UINT n){
    if(wait_token()!=0xFE || spi_failed || !spi_bytes(NULL,buff,n)) return 0;
    spi_io(0xFF); spi_io(0xFF); /* CRC disabled in the card's default SPI mode. */
    return !spi_failed;
}
DRESULT disk_read(BYTE p,BYTE*buff,LBA_t sec,UINT n){
    if(p || !buff || !n) return RES_PARERR;
    if(stat&STA_NOINIT) return RES_NOTRDY;
    if(!valid_range(sec,n)) return RES_PARERR;
    uint32_t previous=sd_begin(SPI_BAUDRATEPRESCALER_8);
    for(UINT i=0;i<n;i++){
        uint32_t address=block_address ? sec+i : (sec+i)*512;
        if(cmd(17,address,0x01)!=0 || !read_block(buff,512) || !cmd_end()) return sd_end(previous,RES_ERROR);
        buff+=512;
    }
    return sd_end(previous,RES_OK);
}

DRESULT disk_write(BYTE p,const BYTE*buff,LBA_t sec,UINT n){
    if(p || !buff || !n) return RES_PARERR;
    if(stat&STA_NOINIT) return RES_NOTRDY;
    if(!valid_range(sec,n)) return RES_PARERR;
    uint32_t previous=sd_begin(SPI_BAUDRATEPRESCALER_8);
    write_operation=true;
    for(UINT i=0;i<n;i++){
        uint32_t address=block_address ? sec+i : (sec+i)*512;
        if(cmd(24,address,0x01)!=0) return sd_end(previous,RES_ERROR);
        spi_io(0xFF); spi_io(0xFE);
        if(!spi_bytes(buff,NULL,512)) return sd_end(previous,RES_ERROR);
        spi_io(0xFF); spi_io(0xFF);
        if((wait_token()&0x1F)!=0x05 || spi_failed || !wait_ready() || !cmd_end()) return sd_end(previous,RES_ERROR);
        buff+=512;
    }
    return sd_end(previous,RES_OK);
}

DRESULT disk_ioctl(BYTE p,BYTE ctl,void*buf){
    if(p || (ctl!=CTRL_SYNC && !buf)) return RES_PARERR;
    if(stat&STA_NOINIT) return RES_NOTRDY;
    if(ctl==GET_SECTOR_SIZE){ *(WORD*)buf=512; return RES_OK; }
    /* shortcut: conservative erase unit, use SD status if format alignment needs tuning. */
    if(ctl==GET_BLOCK_SIZE){ *(DWORD*)buf=1; return RES_OK; }
    if(ctl!=CTRL_SYNC && ctl!=GET_SECTOR_COUNT) return RES_PARERR;
    uint32_t previous=sd_begin(SPI_BAUDRATEPRESCALER_8);
    switch(ctl){
    case CTRL_SYNC:
        write_operation=true;
        sd_cs(0);
        return sd_end(previous,wait_ready()?RES_OK:RES_ERROR);
    case GET_SECTOR_COUNT: {                              /* via CSD (CMD9) */
        uint8_t csd[16];
        if(cmd(9,0,0x01)!=0 || !read_block(csd,16)) return sd_end(previous,RES_ERROR);
        uint64_t n;
        if((csd[0]>>6)==1 && block_address){             /* SDHC/SDXC */
            uint32_t c=((uint32_t)csd[7]<<16)|((uint32_t)csd[8]<<8)|csd[9];
            c&=0x3FFFFF;
            n=((uint64_t)c+1)*1024;
        }else if((csd[0]>>6)==0 && !block_address){       /* SDSC */
            uint32_t read_len=csd[5]&15;
            if(read_len<9 || read_len>11) return sd_end(previous,RES_ERROR);
            uint32_t c=((uint32_t)(csd[6]&3)<<10)|((uint32_t)csd[7]<<2)|((csd[8]>>6)&3);
            uint32_t c_mult=1<<((((csd[9]&3)<<1)|(csd[10]>>7))+2);
            n=((uint64_t)c+1)*c_mult*(1u<<read_len)/512;
        }else return sd_end(previous,RES_ERROR);
        if(!n || n>UINT32_MAX) return sd_end(previous,RES_ERROR);
        DRESULT result=sd_end(previous,RES_OK);
        if(result==RES_OK) *(LBA_t*)buf=(LBA_t)n;
        return result; }
    default: return sd_end(previous,RES_PARERR);
    }
}
