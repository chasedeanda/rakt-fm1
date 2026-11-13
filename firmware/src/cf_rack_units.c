/* SPDX-License-Identifier: GPL-3.0-only
 * Studio outboard gear. The plate, readout, controls and lamps share one
 * layout from the overview through the panel zoom and control close-up. */
static int cgu_unit_blend(int from,int to,int zoom)
{ return from+(to-from)*zoom/100; }

static int cgu_unit_knob_x(cgu_face_t f,int i)
{ return f.x+f.w*(1+2*i)/8; }

static int cgu_unit_knob_y(cgu_face_t f,int zoom)
{ return f.y+cgu_unit_blend(f.h-15,f.h*64/100,zoom); }

static void cgu_perforated_metal(int x,int y,int w,int h)
{
    static const uint8_t hole[][4]={
        {3,4,4,3}, {4,10,10,3}, {4,10,10,3}, {3,5,5,3},
    };
    int left=clampi(fb_cx0-x,0,w), right=clampi(fb_cx1-x,0,w);
    if(left>=right)return;
    for(int row=clampi(fb_cy0-y,0,h),end=clampi(fb_cy1-y,0,h);row<end;row++){
        const uint8_t *shade=hole[row&3];
        int stagger=(row&4)?2:0;
        uint8_t *pixel=fb+(y+row)*FB_W+x+left;
        for(int col=left;col<right;col++)
            *pixel++=(uint8_t)(CGU_METAL+shade[(col+stagger)&3]);
    }
}

static void cgu_unit_grille(int x,int y,int w,int h,int metal)
{
    fb_rbox(x,y,w,h,2,CGU_METAL+10);
    fb_hline(x+2,y+h-1,w-4,metal?CGU_METAL+5:CGU_METAL+3);
    if(metal){
        cgu_perforated_metal(x+2,y+2,w-4,h-4);
        return;
    }
    cgu_grille(x+2,y+2,w-4,h-4);
    for(int stitch=4;stitch+3<w-4;stitch+=6){
        fb_hline(x+stitch,y+3,3,CGU_METAL+4);
        fb_hline(x+stitch,y+h-4,3,CGU_GRILLE+5);
    }
    for(int stitch=7;stitch+3<h-4;stitch+=6){
        fb_vline(x+3,y+stitch,3,CGU_METAL+4);
        fb_vline(x+w-4,y+stitch,3,CGU_GRILLE+5);
    }
}

static void cgu_unit_lamps(const cf_stage_model_t *m,int arp,int x,int y,int w,int h,int zoom,unsigned ink)
{
    unsigned cells=arp && zoom>54?16:8;
    unsigned on=cells==16?m->rack_arp_step:(m->rack_arp_step+1u)/2u;
    cf_rack_window(x-1,y-1,w+2,h+2,RAKT_GLASS);
    int step=(w-4)/(int)cells, gap=step>3 && zoom>50?2:1;
    for(unsigned i=0;i<cells;i++){
        int left=x+(w-step*(int)cells)/2+step*(int)i;
        unsigned lit=arp?ink:i==7?RAKT_RED:i==6?RAKT_AMBER:RAKT_GREEN;
        fb_rect(left,y+2,step-gap,h-4,(arp?on==i+1u:i<m->rack_level)?lit:RAKT_LED_OFF);
    }
    if(zoom>68 && !arp)cfn_center(&CFN_11,x+w/2,y+h+11,"OUTPUT",FL_INK2);
}

static void cgu_unit_dial(int shaping,int x,int y,int r,unsigned pos,unsigned marker)
{
    cgu_metal_disc(x,y+1,r+1,10,10,10);
    if(shaping){
        cgu_metal_disc(x,y,r,4,2,0);
        cgu_metal_disc(x,y,r-2,3,1,0);
        cfn_circle(x,y,r-1,CGU_METAL+2);
    } else { /* Machined cap with a polished rim and a dark pointer. */
        cgu_metal_disc(x,y,r,9,6,3);
        cgu_metal_disc(x,y,r-2,7,6,5);
        marker=CGU_METAL+10;
    }
    cgu_dial_mark(x,y,r,pos,marker,shaping?CGU_METAL+5:CGU_ARP_TICK);
}

static const char *cgu_unit_label(const char *name,char label[12])
{
    if(cb_streq(name,"ARP FOR"))return "Trigger";
    if(cb_streq(name,"FX SPACE"))return "FX Space";
    str_cpy(label,name,12);
    for(char *c=label+1;*c;c++)if(*c>='A' && *c<='Z')*c=(char)(*c-'A'+'a');
    return label;
}

static void cgu_unit_readout(int left,int baseline,int width,const char *value,unsigned ink)
{
    const cfn_font_t *font=cfn_width(&CFN_14,value)<=width?&CFN_14:
        cfn_width(&CFN_12,value)<=width?&CFN_12:&CFN_11;
    cfn_fit(font,left,baseline,value,width,ink);
}

static void cgu_amp_dial(unsigned part,int cx,int cy,int r,unsigned pos,unsigned ink)
{
    if(part==CGS_LAYER){
        fb_rbox(cx-r-4,cy-2,2*r+9,5,1,RAKT_GLASS);
        for(int x=cx-r;x<=cx+r;x+=4)fb_vline(x,cy+4,2,RAKT_EDGE);
        int cap=cx-r+(int)pos*2*r/100;
        fb_rbox(cap-3,cy-6,7,12,2,CGU_METAL+3);
        fb_hline(cap-2,cy-4,5,ink);
        return;
    }
    cgu_metal_disc(cx,cy+1,r+1,10,10,10);
    if(part==CGS_BASS){
        cgu_metal_disc(cx,cy,r,4,2,1);
        cfn_circle(cx,cy,r-1,CGU_METAL+4);
    } else if(part==CGS_DRUMS){
        cgu_metal_disc(cx,cy,r,7,5,2);
        cgu_metal_disc(cx,cy,r-2,4,2,1);
    } else {
        cgu_metal_disc(cx,cy,r,9,6,3);
        cgu_metal_disc(cx,cy,r-2,5,3,2);
    }
    cgu_dial_mark(cx,cy,r,pos,ink,CGU_METAL+5);
}

static void cgu_amp_status(const cf_stage_model_t *m,unsigned part,int x,int y,int w)
{
    unsigned bit=1u<<part;
    unsigned on=(m->activity&bit)!=0;
    if(part==CGS_DRUMS){
        int width=44;
        int left=x+(w-4*width)/2;
        for(unsigned i=0;i<4;i++){
            int lx=left+(int)i*width;
            int hit=(m->drum_groups>>i)&1u;
            cf_rack_window(lx,y,width-3,11,RAKT_GLASS);
            fb_rect(lx+3,y+4,3,3,hit?RAKT_RED:RAKT_LED_OFF);
            cfn_fit(&CFN_11,lx+8,y+9,CGU_GLO_DRUMS[i],width-9,hit?FL_INK:FL_INK2);
        }
    } else if(part==CGS_CHORDS){
        cgu_lamp(x+w-22,y+5,on,RAKT_AMBER);
    } else if(part==CGS_BASS){
        unsigned level=(m->part_level[part]*4+99)/100;
        cf_rack_window(x+w-51,y,31,11,RAKT_GLASS);
        for(unsigned i=0;i<4;i++)
            fb_rect(x+w-47+6*(int)i,y+4,4,3,on && i<level?RAKT_GREEN:RAKT_LED_OFF);
    } else {
        unsigned level=(m->part_level[part]*4+99)/100;
        cf_rack_window(x+w-47,y,27,11,RAKT_GLASS);
        for(unsigned i=0;i<4;i++)
            fb_rect(x+w-43+5*(int)i,y+4,3,3,i<level?CGU_INK_BASE+part:RAKT_LED_OFF);
    }
}

static void cgu_amp_head(const cf_stage_model_t *m,unsigned part,cgu_face_t f,int zoom,int selected)
{
    int x=f.x,y=f.y,w=f.w,h=f.h;
    int sound=m->workspace==CGU_SOUND;
    unsigned accent=CGU_INK_BASE+part;
    unsigned ink=(m->pin_mask&(1u<<part))?FL_INK2:accent;
    int left=x+RAKT_EAR+4;
    int top=y+5;
    int drum=part==CGS_DRUMS, layer=part==CGS_LAYER;
    int deck=y+cgu_unit_blend(drum?44:30,58,zoom);
    cf_rack_plate(x,y,w,h,RAKT_EAR,(uint8_t)(RAKT_CHORDS+part));
    if(part==CGS_CHORDS){
        cgu_unit_grille(x+RAKT_EAR,top-2,w-2*RAKT_EAR,deck-top,0);
        fb_hline(x+RAKT_EAR,deck,w-2*RAKT_EAR,RAKT_COPPER);
    } else if(part==CGS_BASS) {
        fb_rect(x+RAKT_EAR,deck-3,w-2*RAKT_EAR,4,RAKT_VENT);
        fb_hline(x+RAKT_EAR,deck+1,w-2*RAKT_EAR,RAKT_EDGE);
    } else if(layer) {
        fb_hline(x+RAKT_EAR+3,deck-2,w-2*RAKT_EAR-6,RAKT_EDGE);
        fb_hline(x+RAKT_EAR+3,deck,w-2*RAKT_EAR-6,RAKT_SHADE);
    } else {
        fb_rect(x+RAKT_EAR,deck-16,w-2*RAKT_EAR,16,RAKT_RAIL);
        fb_hline(x+RAKT_EAR,deck,w-2*RAKT_EAR,RAKT_EDGE);
    }

    char summary[20];
    const char *value=cgu_module_value(m,0,part,summary);
    const char *title=CGS_PART_NAME[part];
    int title_w=cfn_width(&CFN_16,title);
    int readout=left+title_w+14;
    int width=x+w-readout-((drum || part==CGS_CHORDS)?34:55);
    cfn_text(&CFN_16,left+3,top+15,title,FL_INK);
    cf_rack_window(readout,top,width,20,CGU_GLASS);
    cgu_unit_readout(readout+5,top+15,width-10,value,ink);
    cgu_amp_status(m,part,x,drum?deck-14:top+4,w);

    if(selected && zoom>74)cgu_pages(x+12,drum?y+29:y+37,w-24,m->workspace,part,sound?m->sound_page:m->shaping);
    int clip_l=fb_cx0, clip_t=fb_cy0, clip_r=fb_cx1, clip_b=fb_cy1;
    int dx=selected && zoom>74?m->control_dx:0;
    if(dx)fb_clip(x+RAKT_EAR>clip_l?x+RAKT_EAR:clip_l,clip_t,
        x+w-RAKT_EAR<clip_r?x+w-RAKT_EAR:clip_r,clip_b);
    int sends=sound && m->sound_page==CGU_SENDS;
    int presets=sound && m->sound_page==CGU_PRESETS;
    int radius=cgu_unit_blend(8,14,zoom);
    int cy=y+cgu_unit_blend(h-15,h*(sends?70:65)/100,zoom);
    for(int i=0;i<4;i++){
        unsigned position=selected?m->knob[i]:m->amp_knob[part][i];
        if(selected?!m->label[i][0]:position==255u)continue;
        int cx=cgu_unit_knob_x(f,i)+dx;
        unsigned marker=selected && m->emphasis[i]?ink:FL_INK;
        if(selected && m->disabled[i])marker=CGU_METAL+6;
        if(selected && sends && zoom>90)cgu_send_graph((unsigned)i,position,m->fx_send_shape[i],cx-20,cy-36,40,17,ink);
        cgu_amp_dial(part,cx,cy,radius,position,marker);
        if(!selected || zoom<=74)continue;
        char label[12];
        cgu_knob_caption(cgu_unit_label(m->label[i],label),m->value[i],cx,cy+radius+24,w/4-6,
            FL_INK2,m->disabled[i]?FL_INK2:FL_INK);
    }
    if(selected && zoom>82 && !sound && m->shaping==CGU_PART_EVOLVE){
        int start=x+w/4+5;
        cgu_phrase_window(m,part,start,cy-17,x+w-start-10,ink);
    }
    if(selected && zoom>82 && sound && part==CGS_DRUMS && m->sound_page==CGU_TONE)
        cgu_drum_hit_lights(m,cgu_unit_knob_x(f,2),cy,ink);
    if(selected && presets && zoom>82){
        cgu_unit_readout(x+67+dx,cy-1,w-81,m->value[0],ink);
        char loaded[64]="Loaded ";
        cf_stage_append(loaded,sizeof loaded,m->applied);
        cfn_fit(&CFN_11,x+67+dx,cy+16,loaded,w-81,FL_INK2);
    }
    if(dx)fb_clip(clip_l,clip_t,clip_r,clip_b);
}

static void cgu_panel_waves(int left,int top,int w,int h,unsigned genre,unsigned density,
                            unsigned swing,unsigned drift,uint8_t ink)
{
    int right=left+w;
    int x0=left>fb_cx0?left:fb_cx0, x1=right<fb_cx1?right:fb_cx1;
    if(x1<=x0)return;
    int y0=clampi(fb_cy0-top,0,h), y1=clampi(fb_cy1-top,0,h);
    int spacing=28-(int)density/8;
    int phase=(int)genre*47, bend=17+(int)swing/5;
    int motion=(int)drift;
    for(int row=y0;row<y1;row++){
        int flow=row*3/4+cf_rack_wave(row*2+phase+motion,bend)+cf_rack_wave(row*5-phase-motion,8);
        int stripe=0;
        for(int center=left-76-h*3/4;center<right+48;center+=spacing,stripe++){
            int ripple=row*4+stripe*(24+(int)genre*3)+phase-motion;
            int x=center+flow+cf_rack_wave(ripple,8+(int)swing/16);
            int width=spacing/4+cf_rack_wave(ripple+56,3);
            int start=x>x0?x:x0, end=x+width<x1?x+width:x1;
            if(end>start)fb_hline(start,top+row,end-start,ink);
        }
        if((row&15)==15)CF_PUMP();
    }
}

static void cgu_glitter_finish(cgu_face_t f,unsigned phase,int blue)
{
    unsigned gloss=blue?CGU_SPACE_GLOSS:CGU_ARP_GLOSS;
    unsigned sheen=blue?CGU_SPACE_SHEEN:CGU_ARP_SHEEN;
    unsigned fleck=blue?CGU_SPACE_SHEEN:CGU_ARP_FLECK;
    unsigned sparkle=blue?CGU_PROGRESSION_INK:CGU_ARP_SPARKLE;
    int left=f.x+RAKT_EAR, width=f.w-2*RAKT_EAR;
    fb_rect(left,f.y+2,width,27,gloss);
    fb_hline(left+1,f.y+2,width-2,sheen);
    fb_hline(left+1,f.y+29,width-2,sheen);
    fb_hline(left+1,f.y+f.h-4,width-2,gloss);

    unsigned grain=blue?73:37;
    for(int row=6;row+12<f.h;row+=12)for(int col=RAKT_EAR+3;col+13<f.w-RAKT_EAR;col+=14){
        grain=grain*1664525u+1013904223u;
        int local_y=row+(int)((grain>>24)&7u);
        if(!blue && f.h>112 && local_y>=70 && local_y<=107 && (grain&3u))continue;
        int x=f.x+col+(int)(grain>>29), y=f.y+local_y;
        unsigned shimmer=((grain>>16)+phase)&(phase?15u:31u);
        fb_pset(x,y,shimmer<=1?sparkle:fleck);
        if(!shimmer){
            fb_hline(x-2,y,5,fleck);
            fb_vline(x,y-2,5,fleck);
            fb_pset(x,y,sparkle);
        }
    }
}

static void cgu_band_unit(const cf_stage_model_t *m,unsigned band,cgu_face_t f,int zoom,int selected)
{
    unsigned ink=cgu_band_ink(band);
    int shaping=band==CGU_BAND_SHAPING;
    int arp_detail=!shaping && selected && zoom>68;
    int groove=selected && shaping && m->shaping;
    int lens=selected?m->rack_lens:0;
    int x=f.x,y=f.y,w=f.w,h=f.h;
    cf_rack_plate(x,y,w,h,RAKT_EAR,shaping?RAKT_SHAPING:RAKT_ARP);
    if(shaping)cgu_panel_waves(x+RAKT_EAR,y+1,w-2*RAKT_EAR,h-3,m->genre,m->generator_density,
        m->generator_swing,selected?m->generator_phase:0,RAKT_GENERATOR_WAVE);

    else cgu_glitter_finish(f,m->glitter_phase,0);

    const char *title=shaping?"Generator":"ARP";
    int left=x+16, title_top=y+6;
    int title_w=cfn_width(&CFN_16,title)+11;
    int title_h=21;
    if(title_top+4>=fb_cy0)cfn_text(&CFN_16,left,title_top+16,title,FL_INK);

    char summary[20];
    const char *value=arp_detail?CGU_ARP_NAMES[m->rack_arp_mode]:
        cgu_module_value(m,band,m->focus,summary);
    int readout_x=left+title_w, readout_w=x+w-16-readout_x;
    cf_rack_window(readout_x-1,title_top-1,readout_w+2,title_h+2,CGU_GLASS);
    if(title_top+4>=fb_cy0)cgu_unit_readout(readout_x+6,title_top+16,readout_w-12,value,ink);

    int meter_w=shaping?cgu_unit_blend(54,58,zoom):cgu_unit_blend(67,87,zoom);
    int meter_x=x+w-meter_w-18;
    int meter_y=y+cgu_unit_blend(30,42,zoom);
    int meter_h=cgu_unit_blend(8,13,zoom);
    if(arp_detail){
        cfn_text(&CFN_11,left,y+44,"SELECT",FL_INK);
        cgu_page_tab(left+48,y+31,64,17,"Chords",m->focus==CGS_CHORDS,CGU_INK_BASE+CGS_CHORDS);
        cgu_page_tab(left+112,y+31,64,17,"Layer",m->focus==CGS_LAYER,CGU_INK_BASE+CGS_LAYER);
        cfn_text(&CFN_11,left,y+64,"ARP",FL_INK);
        cgu_page_tab(left+26,y+51,47,17,"Pattern",!m->shaping,ink);
        cgu_page_tab(left+73,y+51,44,17,"Timing",m->shaping,ink);
        meter_y=y+53;
        meter_h=13;
        cgu_unit_lamps(m,1,left+120,meter_y,55,meter_h,zoom,ink);
    } else {
        cgu_unit_lamps(m,!shaping,meter_x,meter_y,meter_w,meter_h,zoom,ink);
        int tab_height=cgu_unit_blend(12,16,zoom);
        int tab_y=meter_y-(tab_height-meter_h)/2;
        int first_width=shaping?75:46, second_width=shaping?47:46;
        int second=selected && m->shaping;
        cgu_page_tab(left-2,tab_y,first_width,tab_height,shaping?"Arrangement":"Pattern",!second,ink);
        cgu_page_tab(left-2+first_width,tab_y,second_width,tab_height,shaping?"Groove":"Timing",second,ink);
    }

    int radius=cgu_unit_blend(8,15,zoom);
    int cy=cgu_unit_knob_y(f,zoom);
    int clearance=meter_y+meter_h+(shaping && zoom>68?11:0)+radius+4;
    if(cy<clearance)cy=clearance;
    if(zoom<=38 && cy+radius>y+h-3)cy=y+h-radius-3;
    int value_y=clampi(cy+radius+29,0,fb_cy1-4);
    for(int i=0;i<4;i++){
        int cx=cgu_unit_knob_x(f,i)+(selected?m->control_dx:0);
        unsigned pos=selected?m->knob[i]:m->rack_knob[band-1u][i];
        int active=selected && m->emphasis[i] && !m->disabled[i];
        int dial_radius=radius+(active?lens*4/CGU_LENS_SCALE:0);
        cgu_unit_dial(shaping,cx,cy,dial_radius,pos,active?ink:selected && m->disabled[i]?CGU_METAL+6:FL_INK);
        if(zoom<74 || !selected || !m->label[i][0] || value_y>y+h-3)continue;
        if(active)cfn_circle(cx,cy,dial_radius+3,ink);
        char label[12];
        const char *setting=shaping && !groove && i==0?CGS_PROFILES[m->genre].name:m->value[i];
        int cell=w/4-6;
        int caption_x=clampi(cx,fb_cx0+cell/2,fb_cx1-cell/2);
        cgu_knob_caption(cgu_unit_label(m->label[i],label),setting,caption_x,value_y,cell,
            active?ink:FL_INK2,m->disabled[i]?FL_INK2:FL_INK);
    }
}
