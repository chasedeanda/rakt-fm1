/* SPDX-License-Identifier: GPL-3.0-only
 * Studio outboard gear. The plate, readout, controls and lamps share one
 * layout from the overview through the panel zoom and control close-up. */
static int cgu_unit_blend(int from,int to,int zoom)
{ return from+(to-from)*zoom/100; }

static int cgu_unit_controls(unsigned band)
{
    return band==CGU_BAND_PROGRESSION?1:4;
}

static int cgu_unit_knob_x(cgu_face_t f,int i)
{ return f.x+f.w*(1+2*i)/8; }

static int cgu_unit_knob_y(cgu_face_t f,int zoom)
{ return f.y+cgu_unit_blend(f.h-15,f.h*64/100,zoom); }

static cgu_face_t cgu_unit_lens(cgu_face_t f,int knob,int scale)
{
    int x=cgu_unit_knob_x(f,knob), y=cgu_unit_knob_y(f,100);
    return (cgu_face_t){f.x+(f.x-x)*scale/100,f.y+(f.y-y)*scale/100,
        f.w+f.w*scale/100,f.h+f.h*scale/100};
}

static void cgu_unit_screw(int x,int y,int large)
{
    if(large){
        cgu_metal_disc(x,y,2,7,5,2);
        fb_hline(x-1,y,3,CGU_METAL);
    } else {
        fb_pset(x,y,CGU_METAL+6);
        fb_pset(x,y+1,CGU_METAL+10);
    }
}

static void cgu_unit_vents(int x,int y,int w,int h,int pitch)
{
    fb_rbox(x,y,w,h,1,CGU_METAL+1);
    fb_hline(x+1,y+h-1,w-2,CGU_METAL+4);
    for(int i=2;i+pitch<=w;i+=pitch){
        fb_rect(x+i,y+2,pitch-2,h-4,CGU_METAL+10);
        fb_vline(x+i+pitch-2,y+3,h-5,CGU_METAL+4);
    }
}

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

static void cgu_unit_lamps(const cf_stage_model_t *m,unsigned band,int x,int y,int w,int h,int zoom,unsigned ink)
{
    static const char *const labels[]={"BEAT","OUTPUT","STEP","CHORD SLOT"};
    unsigned cells=8, on=0, fill=0;
    switch(band){
    case CGU_BAND_HARMONY: cells=m->rack_meter;on=m->rack_beat;break;
    case CGU_BAND_SHAPING: fill=m->rack_level;break;
    case CGU_BAND_ARP: cells=16;on=m->rack_arp_step;break;
    case CGU_BAND_PROGRESSION:
        cells=m->count>8?16:8;on=m->slot;break;
    }
    if(cells>8 && zoom<=54){
        cells=(cells+1u)/2u;
        on=(on+1u)/2u;
    }
    if(!cells)return;
    cf_rack_window(x-1,y-1,w+2,h+2,RAKT_GLASS);
    int step=(w-4)/(int)cells, gap=zoom>50?2:1;
    for(unsigned i=0;i<cells;i++){
        int left=x+(w-step*(int)cells)/2+step*(int)i;
        unsigned lit=band==CGU_BAND_SHAPING?(i==7?RAKT_RED:i==6?RAKT_AMBER:RAKT_GREEN):ink;
        fb_rect(left,y+2,step-gap,h-4,(on==i+1u || i<fill)?lit:RAKT_LED_OFF);
    }
    if(zoom>68)cfn_center(&CFN_11,x+w/2,y+h+11,labels[band-1u],FL_INK2);
}

static void cgu_unit_dial(unsigned band,int x,int y,int r,unsigned pos,unsigned marker)
{
    if(band==CGU_BAND_PROGRESSION){
        fb_rbox(x-r,y-r/2,r*2+1,r+1,2,CGU_METAL+10);
        fb_hline(x-r+2,y+r/2,r*2-3,CGU_METAL+5);
        int travel=pos?r/2:-r/2;
        fb_rbox(x+travel-r/3,y-r/2+1,r*2/3+1,r-2,2,CGU_METAL+4);
        fb_hline(x+travel-r/3+2,y-r/2+2,r*2/3-3,CGU_METAL+8);
        return;
    }
    cgu_metal_disc(x,y+1,r+1,10,10,10);
    switch(band){
    case CGU_BAND_HARMONY:
        cgu_metal_disc(x,y,r,8,5,1);
        cgu_metal_disc(x,y,r-2,4,1,0);
        if(r>9){
            fb_vline(x-r+1,y-2,5,CGU_METAL+6);
            fb_vline(x+r-1,y-2,5,CGU_METAL+3);
        }
        break;
    case CGU_BAND_SHAPING:
        cgu_metal_disc(x,y,r,4,2,0);
        cgu_metal_disc(x,y,r-2,3,1,0);
        cfn_circle(x,y,r-1,CGU_METAL+2);
        break;
    default: /* Machined cap with a polished rim and a dark pointer. */
        cgu_metal_disc(x,y,r,9,6,3);
        cgu_metal_disc(x,y,r-2,7,6,5);
        marker=CGU_METAL+10;
        break;
    }
    cgu_dial_mark(x,y,r,pos,marker);
}

static const char *cgu_unit_label(const char *name,char label[12])
{
    if(cb_streq(name,"ARP FOR"))return "For";
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
    cgu_dial_mark(cx,cy,r,pos,ink);
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
        cfn_center(&CFN_11,cx,cy+radius+11,cgu_unit_label(m->label[i],label),FL_INK2);
        if(i==0 && ((!sound && !m->shaping) || (sound && m->sound_page==CGU_TONE) || presets))continue;
        int cell=w/4-6;
        const cfn_font_t *font=cfn_width(&CFN_12,m->value[i])<=cell?&CFN_12:&CFN_11;
        unsigned text_ink=m->disabled[i]?FL_INK2:FL_INK;
        if(cfn_width(font,m->value[i])<=cell)
            cfn_center(font,cx,cy+radius+24,m->value[i],text_ink);
        else cfn_fit(font,cx-cell/2,cy+radius+24,m->value[i],cell,text_ink);
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

static void cgu_band_unit(const cf_stage_model_t *m,unsigned band,cgu_face_t f,int zoom,int selected)
{
    unsigned ink=cgu_module_ink(CGS_NPART+band-1u);
    int count=cgu_unit_controls(band);
    int groove=selected && band==CGU_BAND_SHAPING && m->shaping;
    int lens=selected && !groove?m->rack_lens:0;
    if(lens)f=cgu_unit_lens(f,cgu.turn_knob,lens);
    int x=f.x,y=f.y,w=f.w,h=f.h;
    static const uint8_t surfaces[]={RAKT_HARMONY,RAKT_SHAPING,RAKT_ARP,RAKT_PROGRESSION};
    cf_rack_plate(x,y,w,h,RAKT_EAR,surfaces[band-1u]);

    const char *title=groove?"Groove":CGU_BAND_NAMES[band-1u];
    int left=x+16, title_top=y+6;
    int title_w=cfn_width(&CFN_16,title)+11;
    int title_h=21;
    if(title_top+4>=fb_cy0)cfn_text(&CFN_16,left,title_top+16,title,FL_INK);

    char summary[20];
    const char *value=band==CGU_BAND_PROGRESSION?(m->rack_draft?"Draft":"Bank"):
        cgu_module_value(m,band,m->focus,summary);
    int readout_x=left+title_w, readout_w=x+w-16-readout_x;
    cf_rack_window(readout_x-1,title_top-1,readout_w+2,title_h+2,CGU_GLASS);
    if(title_top+4>=fb_cy0)cgu_unit_readout(readout_x+6,title_top+16,readout_w-12,value,ink);

    int meter_w=cgu_unit_blend(67,87,zoom);
    int meter_x=x+w-meter_w-18;
    int meter_y=y+cgu_unit_blend(30,42,zoom);
    int meter_h=cgu_unit_blend(8,13,zoom);
    cgu_unit_lamps(m,band,meter_x,meter_y,meter_w,meter_h,zoom,ink);

    if(zoom>70){
        if(band==CGU_BAND_ARP){
            const char *page=m->shaping?"Timing":"Pattern";
            cfn_text(&CFN_12,left,meter_y+11,page,FL_INK2);
        } else if(band==CGU_BAND_SHAPING && selected){
            cfn_text(&CFN_11,left,meter_y+11,groove?"2/2  Shaping":"1/2  Arrangement",FL_INK2);
        } else {
            int badge=left+1, vent=badge+46;
            fb_rbox(badge,meter_y,36,14,1,CGU_METAL+1);
            fb_hline(badge+1,meter_y,34,CGU_METAL+5);
            fb_hline(badge+1,meter_y+13,34,CGU_METAL+10);
            cgu_unit_screw(badge+2,meter_y+6,0);
            cgu_unit_screw(badge+33,meter_y+6,0);
            cfn_center(&CFN_11,badge+18,meter_y+11,"RAKT",FL_INK);
            cgu_unit_vents(vent,meter_y,meter_x-vent-10,14,5);
        }
    }

    int detail_radius=count==1?19:15;
    int radius=cgu_unit_blend(8,detail_radius,zoom)+lens*detail_radius/100;
    int cy=cgu_unit_knob_y(f,zoom);
    int clearance=meter_y+meter_h+(zoom>68?11:0)+radius+4;
    if(cy<clearance)cy=clearance;
    if(zoom<=38 && cy+radius>y+h-3)cy=y+h-radius-3;
    int value_y=cy+radius+29;
    if(value_y>fb_cy1-4)value_y=fb_cy1-4;
    if(band==CGU_BAND_SHAPING){
        cgu_unit_grille(x+14,cy-radius-2,w-28,2*radius+5,1);
    } else if(band==CGU_BAND_PROGRESSION){
        int start=x+w/4+12;
        cgu_unit_grille(start,cy-radius+1,x+w-start-20,2*radius+2,0);
        if(zoom>74)cgu_unit_readout(start,value_y,x+w-start-19,m->applied,FL_INK);
    }
    for(int i=0;i<count;i++){
        int cx=cgu_unit_knob_x(f,i)+(selected && (band==CGU_BAND_ARP || band==CGU_BAND_SHAPING)?m->control_dx:0);
        unsigned pos=selected?m->knob[i]:m->rack_knob[band-1u][i];
        int active=selected && m->emphasis[i] && !m->disabled[i];
        cgu_unit_dial(band,cx,cy,radius,pos,active?ink:selected && m->disabled[i]?CGU_METAL+6:FL_INK);
        if(zoom<74 || !selected || !m->label[i][0] || value_y>y+h-3)continue;
        if(active && band!=CGU_BAND_PROGRESSION)cfn_circle(cx,cy,radius+5,ink);
        int available=(cx-fb_cx0<fb_cx1-cx?cx-fb_cx0:fb_cx1-cx)*2;
        int cell=w/4-4;
        char label[12];
        const char *name=cgu_unit_label(m->label[i],label);
        int label_width=cfn_width(&CFN_12,name);
        if(label_width<=available)
            cfn_center(&CFN_12,cx,value_y-(groove?23:16),name,active?ink:FL_INK2);
        if(band==CGU_BAND_SHAPING && !groove && i==0)continue;
        const cfn_font_t *font=active?&CFN_16:&CFN_14;
        if(groove)font=&CFN_11;
        unsigned value_ink=m->disabled[i]?FL_INK2:FL_INK;
        if(groove && cfn_width(font,m->value[i])>cell){
            char first[14];
            str_cpy(first,m->value[i],sizeof first);
            char *second=first;
            while(*second && *second!=' ')second++;
            if(*second){
                *second++=0;
                if(cfn_width(font,first)<=cell && cfn_width(font,second)<=cell && cell<=available){
                    cfn_center(font,cx,value_y-11,first,value_ink);
                    cfn_center(font,cx,value_y,second,value_ink);
                    continue;
                }
            }
            if(cell<=available){
                cfn_squeezed(font,cx-cell/2,value_y,m->value[i],cell,value_ink);
                continue;
            }
        }
        if(cfn_width(font,m->value[i])>cell)font=&CFN_12;
        if(cfn_width(font,m->value[i])>cell)font=&CFN_11;
        int width=cfn_width(font,m->value[i]);
        if(width<=cell && width<=available)
            cfn_center(font,cx,value_y,m->value[i],value_ink);
        else if(cell<=available && width>cell)
            cfn_fit(font,cx-cell/2,value_y,m->value[i],cell,FL_INK);
    }
}
