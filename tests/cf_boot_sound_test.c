/* SPDX-License-Identifier: GPL-3.0-only
 * Actual boot DSP and full physical/USB/MIDI path, with no device flashing. */
#include <stdio.h>
#include "usb_mock.h"
#define FELUCCA_UAC 1
#define FELUCCA_CDC 1
#include "../tools/emu/emu_fw.c"
static void ua_service_paced(uint32_t t0,int isr){(void)t0;(void)isr;ua_service();}
static unsigned checks,failures;
#define CHECK(x) do{checks++;if(!(x)){failures++;if(failures<25)printf("FAIL %d: %s\n",__LINE__,#x);}}while(0)
static void direct(int32_t *stereo,unsigned frames,unsigned chunk)
{
    memset(&cfb,0,sizeof cfb);cf_boot_go=1;
    for(unsigned at=0;at<frames;){unsigned n=frames-at;if(n>chunk)n=chunk;
        int32_t l[CTL]={0},r[CTL]={0};cf_boot_mix(l,r,n);
        for(unsigned i=0;i<n;i++){stereo[(at+i)*2]=l[i];stereo[(at+i)*2+1]=r[i];}at+=n;
    }
}
static void state_and_dsp(void)
{
    unsigned frames=CF_BOOT_SAMPLES+FS/5u,writes=emu_flash_writes,events=cf_evq_w,midi=mo_w;
    unsigned patterns=cfs_fnv(&sq_st,sizeof sq_st),sounds=cfs_fnv(cf_snd,sizeof cf_snd);
    int32_t *a=calloc(frames*2,sizeof *a),*b=calloc(frames*2,sizeof *b);CHECK(a && b);if(!a||!b)return;
    direct(a,frames,CTL);CHECK(!cfb.on);direct(b,frames,17);CHECK(!memcmp(a,b,frames*2*sizeof *a));
    unsigned peak=0,side=0;
    for(unsigned i=0;i<frames*2;i++){unsigned v=a[i]<0?-a[i]:a[i];if(v>peak)peak=v;CHECK(v<26000);if(i>=CF_BOOT_SAMPLES*2)CHECK(!v);}
    for(unsigned i=0;i<frames;i++)side|=a[i*2]!=a[i*2+1];CHECK(peak>2000 && side);
    CHECK(a[0]==0 && a[1]==0);CHECK(abs(a[(CF_BOOT_SAMPLES-1)*2])<3 && abs(a[(CF_BOOT_SAMPLES-1)*2+1])<3);
    /* Inactive state adds nothing to an existing mix, including negative samples. */
    int32_t l[CTL],r[CTL];for(unsigned i=0;i<CTL;i++){l[i]=17;r[i]=-23;}cf_boot_mix(l,r,CTL);
    for(unsigned i=0;i<CTL;i++)CHECK(l[i]==17 && r[i]==-23);
    cf_boot_pending=1;cf_boot_go=1;cf_boot_note();CHECK(!cf_boot_pending);cf_boot_mix(l,r,CTL);CHECK(!cf_boot_go && !cfb.on);
    cf_boot_go=1;CHECK(cf_boot_yield() && !cf_boot_go && !cfb.on);
    /* An active signature fades within 256 samples for either priority source. */
    for(unsigned overload=0;overload<2;overload++) {
        direct(a,FS/10,CTL);CHECK(cfb.on);if(overload)CHECK(cf_boot_yield());else cf_boot_note();
        unsigned elapsed=0;int gain=cfb.gain;
        while(cfb.on && elapsed<257){l[0]=r[0]=0;cf_boot_mix(l,r,1);CHECK(cfb.gain<=gain);gain=cfb.gain;elapsed++;}
        CHECK(!cfb.on && elapsed<=256);
    }
    CHECK(writes==emu_flash_writes && events==cf_evq_w && midi==mo_w);
    CHECK(patterns==cfs_fnv(&sq_st,sizeof sq_st) && sounds==cfs_fnv(cf_snd,sizeof cf_snd));
    free(a);free(b);
}
static void controls(void)
{
    int32_t l[CTL]={0},r[CTL]={0};cu.scr=SCR_GLO;cu.page[SCR_GLO]=0;
    unsigned writes=emu_flash_writes;cr_safe=0;ui_knob_turn(2,1);CHECK(cs.boot_sound && cf_boot_go==1);
    cf_boot_mix(l,r,CTL);CHECK(cfb.on);ui_knob_turn(2,-1);CHECK(!cs.boot_sound && cf_boot_go==2);
    for(unsigned i=0;i<8;i++)cf_boot_mix(l,r,CTL);CHECK(!cfb.on && !cf_boot_go);
    ui_knob_turn(2,1);ui_knob_turn(2,-1);cf_boot_mix(l,r,CTL);CHECK(!cfb.on && !cf_boot_go);
    CHECK(writes==emu_flash_writes);
    CHECK(cf_settings_write(0)==1);cf_boot_go=0;cf_splash();cf_ui_init();CHECK(!cs.boot_sound && !cf_boot_go && !cf_boot_pending);
    /* Saved Motion Off still allows sound. Its preference is independent. */
    cs.boot_sound=1;CHECK(cf_settings_write(0)==1);cf_motion=0;cpb_dirty=1;cpb_flush();
    cf_boot_go=0;cf_splash();cf_ui_init();CHECK(!cf_motion && !cf_intro_on && cf_boot_pending && !cf_boot_go);
    fm1_ms+=200;cf_ui_frame();CHECK(cf_boot_go==1 && !cf_boot_pending);
    cf_boot_go=0;cr_safe=1;cf_splash();cf_ui_init();CHECK(cf_boot_go!=1);
    cu.scr=SCR_GLO;cu.page[SCR_GLO]=0;ui_knob_turn(2,1);CHECK(cf_boot_go!=1);cr_safe=0;cf_boot_go=0;
}
static void physical(unsigned rate,const char *path,int pending,int key,unsigned master)
{
    FILE *f=fopen(path,"wb");CHECK(f);if(!f)return;
    cf_intro_on=0;cf_boot_go=pending;memset(&cfb,0,sizeof cfb);song.master_q12=master;cu.pop=0;
    ua_reset();ua.cap_alt=1;ua.cap_hold=0;ua_rate_set(rate);
    unsigned writes=emu_flash_writes;uint32_t patterns=cfs_fnv(&sq_st,sizeof sq_st);
    unsigned midi=mo_w;
    if(key)cf_post(CFE_KEY,7,0,1);
    unsigned audible=0;
    for(unsigned b=0;b<(CF_BOOT_SAMPLES+FS/5u)/CTL;b++) {
        int32_t out[CTL*2];unsigned w=ua.cw;audio_block(out,CTL);
        for(unsigned i=0;i<CTL*2;i++){int v=out[i]>>8;CHECK(v>-32768 && v<32767);audible|=v!=0;
            if(!key){int16_t pcm=v;fwrite(&pcm,sizeof pcm,1,f);}}
        if(key){fwrite(out,sizeof out,1,f);
            for(unsigned i=w;i<ua.cw;i++)fwrite(ua.cap+(i&(UA_CAP_RING-1))*6,6*sizeof(int16_t),1,f);
            while(mo_r!=mo_w){uint32_t ev=midi_out_q[mo_r++%MQ];fwrite(&ev,sizeof ev,1,f);}}
        else for(unsigned p=w;p<ua.cw;p++)for(unsigned ch=2;ch<6;ch++)CHECK(!ua.cap[(p&(UA_CAP_RING-1))*6+ch]);
        ua.cr=ua.cw;
    }
    CHECK((master?audible:!audible) && !cfb.on && !cf_boot_go);
    if(!key)CHECK(midi==mo_w);
    CHECK(writes==emu_flash_writes && patterns==cfs_fnv(&sq_st,sizeof sq_st));fclose(f);
}
int main(int argc,char **argv)
{
    emu_fw_options(0,1,0,1);emu_hal.ready=2;emu_fw_init(0);CHECK(cf_boot_pending==1);cf_boot_pending=0;
    if(argc==6 || argc==7)physical(atoi(argv[2]),argv[3],atoi(argv[4]),atoi(argv[5]),argc==7?atoi(argv[6]):4096);
    else {state_and_dsp();controls();}
    printf("RAKT boot sound: %u checks, %u failures\n",checks,failures);return failures!=0;
}
