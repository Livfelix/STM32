#define main acquisition_main
#include "acquisition_harness.c"
#undef main
int main(int argc,char **argv){
    assert(argc==2);memset(host_flash,255,sizeof host_flash);app_init();drain_uart();
    if(!strcmp(argv[1],"gain")){
        assert(!app_set_channel_config(0,HALF_BENDING,2.08f,1));assert(ch_gain[0]==100);
        assert(app_set_channel_config(0,HALF_BENDING,2.08f,2));assert(app_set_channel_config(0,HALF_BENDING,2.08f,1000));
        assert(!app_set_channel_config(0,HALF_BENDING,2.08f,1001));assert(ch_gain[0]==1000);
    }else if(!strcmp(argv[1],"save")){
        cfg.ch_mask=9;cfg.fc_lp=10;cfg.avg=16;cfg.trig_channel=3;cfg.trig_pre_s=1.2f;cfg.trig_post_s=4.3f;assert(app_reapply());assert(app_set_log_config("ABC",123));log_set_bin(0);assert(app_set_brightness(90));touch_calib_set(500,400,3600,3700);
        assert(app_set_channel_config(1,QUARTER,2.10f,200));
        ch_bias_valid=ch_zero_valid=ch_cal_valid=63;for(int i=0;i<6;i++){ch_bias_v[i]=1.2f+i*.01f;ch_zero_ratio[i]=i*.00001f;ch_scale[i]=1+i*.1f;}
        app_save_cfg();assert(host_flash[0]!=65535);assert(app_apply_preset(2));ch_bias_valid=0;
        app_load_cfg();assert(app_applied_config()->ch_mask==9 && app_applied_config()->avg==16 && app_applied_config()->fc_lp==10);
        assert(ch_bias_valid==63 && ch_zero_valid==63 && ch_cal_valid==63);assert(ch_scale[5]==1.5f && ch_bias_v[1]==1.21f);
        assert(!strcmp(app_log_prefix(),"ABC") && app_log_duration()==123 && !log_is_bin());
        assert(ch_bridge[1]==QUARTER && ch_gf[1]==2.10f && ch_gain[1]==200);
        assert(app_applied_config()->trig_channel==3 && app_applied_config()->trig_pre_s==1.2f && app_applied_config()->trig_post_s==4.3f);
        assert(app_brightness()==90 && touch_points[0]==500 && touch_points[3]==3700);
        app_init();assert(ch_zero_ratio[5]==5*.00001f && ch_zero_valid==63 && app_applied_config()->ch_mask==9); /* No boot tare. */
    }else if(!strcmp(argv[1],"cuts")){
        assert(app_save_cfg());uint16_t baseline[1024];memcpy(baseline,host_flash,sizeof baseline);
        assert(app_apply_preset(2));
        for(unsigned cut=0;cut<2+offsetof(ConfigImage,commit)/2;cut++){
            memcpy(host_flash,baseline,sizeof baseline);flash_steps=0;flash_cut=(int)cut;assert(!app_save_cfg());flash_cut=-1;
            assert(app_load_cfg());assert(app_applied_config()->ch_mask==63);assert(app_apply_preset(2));
        }
        memcpy(host_flash,baseline,sizeof baseline);assert(app_save_cfg());assert(app_load_cfg());assert(app_applied_config()->ch_mask==3);
        ConfigImage *bad=(ConfigImage *)(host_flash+512);bad->gain[0]=1;bad->crc=sg_crc16((uint8_t *)bad,offsetof(ConfigImage,crc));assert(app_load_cfg());assert(app_applied_config()->ch_mask==63);
        ((ConfigImage *)host_flash)->crc^=1;assert(!app_load_cfg());assert(app_applied_config()->ch_mask==63);
        memset(host_flash,255,sizeof host_flash);host_flash[0]=0x5653;uint8_t old=ch_zero_valid;assert(!app_load_cfg());assert(ch_zero_valid==old);assert(strstr(app_last_error(),"CALIBRATE"));
    }else if(!strcmp(argv[1],"rejects")){
        assert(app_save_cfg());assert(app_apply_preset(2));cfg.ch_mask=0;cfg.avg=0;assert(!app_reapply());assert(app_save_cfg());assert(app_load_cfg());assert(app_applied_config()->ch_mask==3);
        assert(!app_set_channel_config(0,HALF_BENDING,2.08f,1));
        ch_gain[0]=1;assert(!app_save_cfg());ch_gain[0]=100;
        ch_bias_valid=ch_zero_valid=63;drain_uart();assert(app_rec_start());
        uint16_t before[1024];memcpy(before,host_flash,sizeof before);PresetCfg old=cfg;
        assert(!app_save_cfg() && !app_load_cfg());assert(!memcmp(before,host_flash,sizeof before));assert(!memcmp(&old,&cfg,sizeof old));
    }else if(!strcmp(argv[1],"windows")){
        cfg.ch_mask=9;cfg.fc_lp=0;cfg.avg=1;cfg.trig_on=1;cfg.trig_channel=3;assert(app_reapply());
        ch_bias_valid=ch_zero_valid=63;for(int i=0;i<6;i++)ch_bias_v[i]=1.2f;drain_uart();assert(app_rec_start());scan(300,1000,1);assert(record_at(0)[43]&SG_FLAG_TRIGGER);assert(app_rec_stop());
        cfg.trig_pre_s=10;cfg.trig_post_s=60;assert(app_reapply());cfg.trig_pre_s=0;cfg.trig_post_s=.1f;assert(app_reapply());PresetCfg active=*app_applied_config();
        cfg.trig_pre_s=NAN;assert(!app_reapply());cfg=active;cfg.trig_post_s=0;assert(!app_reapply());cfg=active;cfg.trig_channel=6;assert(!app_reapply());cfg=active;cfg.fc_lp=2;assert(!app_reapply());cfg=active;cfg.avg=3;assert(!app_reapply());assert(!memcmp(&active,app_applied_config(),sizeof active));
    }else if(!strcmp(argv[1],"presets")){
        ch_bias_valid=ch_zero_valid=ch_cal_valid=63;for(int i=0;i<6;i++){ch_bias_v[i]=1.2f;ch_zero_ratio[i]=.001f;ch_scale[i]=1.5f;}
        for(int id=0;id<4;id++){assert(app_apply_preset(id));for(int i=0;i<6;i++){assert(ch_gain[i]==100 && ch_gf[i]==2.08f && ch_zero_ratio[i]==.001f && ch_scale[i]==1.5f && ch_bridge[i]==(i<3?HALF_BENDING:QUARTER));}assert(ch_cal_valid==63);}
        assert(app_set_channel_config(4,HALF_BENDING,2.08f,100));assert(ch_bias_valid==63 && ch_zero_valid==47 && ch_cal_valid==47);
    }else if(!strcmp(argv[1],"triggerheader")){
        cfg.trig_on=0;assert(app_reapply());assert(make_header(true,2));assert(strstr(header,"#TRIGGER enabled=0 "));
        cfg.trig_on=1;assert(make_header(true,2));assert(strstr(header,"#TRIGGER enabled=0 ")); /* Applied only, never draft. */
        assert(app_reapply());assert(make_header(true,2));assert(strstr(header,"#TRIGGER enabled=1 "));
        cfg.trig_on=0;cfg.ch_mask=9;cfg.fc_lp=0;cfg.avg=1;assert(app_reapply());assert(make_header(true,2));
        FILE *out=fopen("tests/fixtures/header_v2.txt","wb");assert(out);assert(fwrite(header,1,header_len,out)==header_len);assert(!fclose(out));
    }else if(!strcmp(argv[1],"metadata")){
        assert(app_apply_preset(1));assert(txh!=txt);drain_uart();
        assert(app_set_channel_config(1,QUARTER,2.1f,100));assert(txh!=txt);
        txt=0;txh=TXB-10;assert(app_apply_preset(2));uint16_t before=txh;
        ch_bias_valid=ch_zero_valid=63;sample_ue=1;now=2000;in_irq=1;app_drdy_irq();app_drdy_irq();app_drdy_irq();in_irq=0;app_poll();assert(txh==before);
    }else if(!strcmp(argv[1],"uartstatus")){
        cfg.ch_mask=9;cfg.fc_lp=0;cfg.avg=1;cfg.trig_on=1;cfg.trig_lvl_ue=500;assert(app_reapply());
        ch_bias_valid=ch_zero_valid=63;for(int i=0;i<6;i++)ch_bias_v[i]=1.2f;assert(app_rec_start());
        txt=0;txh=TXB-SG_RECORD_SIZE;uint16_t before=txh;
        scan(501,1000,1);assert(app_uart_trigger_lost()==1 && trigger_status_pending && txh==before);
        app_poll();assert(trigger_status_pending && app_uart_trigger_lost()==1 && txh==before);
        drain_uart();before=txh;app_poll();
        const char *status="#STATUS trigger_uart_lost=1 sd_max_io_us=0\n";
        assert(!trigger_status_pending && app_uart_trigger_lost()==1 && ((txh-before)&(TXB-1))==strlen(status));
        for(unsigned i=0;i<strlen(status);i++)assert(txb[(before+i)&(TXB-1)]==(uint8_t)status[i]);
        before=txh;app_poll();assert(txh==before);app_init();assert(app_uart_trigger_lost()==0 && !trigger_status_pending);
    }else if(!strcmp(argv[1],"uartmetadatareserve")){
        assert(make_header(true,2));txt=0;txh=(uint16_t)(TXB-header_len-SG_RECORD_SIZE);
        uint16_t before=txh;assert(!refresh_metadata() && txh==before);
        txh--;assert(refresh_metadata());assert(((txt-txh-1)&(TXB-1))==SG_RECORD_SIZE);
        before=txh;uart_puts("x");assert(txh==before);
    }else assert(!"unknown case");return 0;
}
