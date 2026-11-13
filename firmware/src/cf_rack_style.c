/* SPDX-License-Identifier: GPL-3.0-only
 * Shared RAKT hardware finishes and geometry at the LCD's native resolution.
 */
enum {
    RAKT_RAIL=CGU_RACK_PALETTE, RAKT_EDGE, RAKT_NICKEL, RAKT_SHADE, RAKT_GLASS,
    RAKT_VENT, RAKT_LED_OFF, RAKT_AMBER, RAKT_GREEN, RAKT_RED, RAKT_COPPER,
    RAKT_CHORDS, RAKT_BASS, RAKT_LAYER, RAKT_DRUMS,
    RAKT_HARMONY, RAKT_SHAPING, RAKT_GENERATOR_WAVE, RAKT_ARP, RAKT_FX_SPACE
};
enum { RAKT_X=14, RAKT_W=212, RAKT_UNIT_H=64, RAKT_GUTTER=4, RAKT_EAR=12 };
enum { RAKT_SLIDE_STEP_MS=40, RAKT_SLIDE_STEPS=6 };

static const uint8_t RAKT_CONTOUR_FACES[]={FL_BG2,FL_BG3,RAKT_CHORDS,RAKT_BASS,RAKT_LAYER,RAKT_DRUMS,RAKT_HARMONY};

static void cf_rack_colors(void)
{
    static const uint32_t surface[] = {
        0x090B0D, 0x191D21, 0x252B30, 0x30363B, 0xF2EBDD, 0xBEC4C5, 0x7F8A90, 0x060809,
    };
    static const uint32_t hardware[] = {
        0x181D21, 0x485159, 0x8B959B, 0x101417, 0x0B1214, 0x141719,
        0x313C3C, 0xF5B361, 0x9BDFB5, 0xF88066, 0x7C5B41,
        0x594534, 0x263137, 0x3A3451, 0x424753,
        0x3C4246, 0x304C3E, 0x20372D, 0x483A50, 0x26374D,
    };
    _Static_assert(NELEM(hardware)<=CGU_METAL-CGU_RACK_PALETTE, "Rack colors fit");
    memcpy(cf_pal,surface,sizeof surface);
    memcpy(cf_pal+RAKT_RAIL,hardware,sizeof hardware);
    for(unsigned i=0;i<NELEM(RAKT_CONTOUR_FACES);i++)
        cf_pal[CGU_CONTOUR_BASE+i]=cfn_blend(cf_pal[RAKT_CONTOUR_FACES[i]],0xbac9da,11);
}

static void cf_rack_screw(int x,int y)
{
    fb_rbox(x-3,y-3,7,7,2,FL_BLACK);
    fb_rbox(x-2,y-3,5,5,1,RAKT_NICKEL);
    fb_hline(x-2,y-1,5,RAKT_EDGE);
    fb_line(x-1,y,x+1,y-2,RAKT_SHADE);
}

static int cf_rack_wave(int phase,int amplitude)
{
    return (int)(osc_sine((uint32_t)phase<<24)*amplitude/32768);
}

enum { RAKT_CONTOUR_DEFAULT, RAKT_CONTOUR_LIBRARY=NELEM(RAKT_CONTOUR_FACES), RAKT_CONTOUR_HELP };

static void cf_rack_contour_span(int x,int y,int width,int left,int right,uint8_t ink)
{
    int end=x+width<right?x+width:right;
    if(x<left)x=left;
    if(x<end)fb_hline(x,y,end-x,ink);
}

static void cf_rack_contours(int x,int y,int w,int h,uint8_t face,unsigned variation)
{
    /* Vary the spacing, bend and angle by face. Zero rate draws nested arcs. */
    static const struct { uint8_t spacing, rate, phase; int8_t bend, slope; } patterns[]={
        {22, 2, 75, 14, -3},  /* Mix */
        {17, 0, 49,  7,  2},  /* Saved Takes */
        {18, 2, 12, 17,  4},  /* Chords */
        {15, 3, 96, 10, -5},  /* Bass */
        {19, 0, 19,  9, -3},  /* Layer */
        {14, 4, 42,  8,  3},  /* Drums */
        {21, 0, 73,  6,  2},  /* Song */
        {16, 0, 25, 10, -2},  /* Sound Library */
        {20, 2, 40, 19,  1},  /* Help */
    };
    unsigned material=0;
    while(material<NELEM(RAKT_CONTOUR_FACES) && RAKT_CONTOUR_FACES[material]!=face)material++;
    if(material==NELEM(RAKT_CONTOUR_FACES))return;
    unsigned pattern=variation?variation:material;
    if(pattern>RAKT_CONTOUR_HELP)pattern=RAKT_CONTOUR_HELP;
    int phase=patterns[pattern].phase;
    if(variation>=RAKT_CONTOUR_HELP)phase+=37*(int)(variation-RAKT_CONTOUR_HELP);
    int spacing=patterns[pattern].spacing, rate=patterns[pattern].rate;
    int bend=patterns[pattern].bend, slope=patterns[pattern].slope;
    int left=x>fb_cx0?x:fb_cx0, right=x+w<fb_cx1?x+w:fb_cx1;
    int first=clamp(fb_cy0-y,0,h), end=clamp(fb_cy1-y,0,h);
    if(left>=right || first>=end)return;
    uint8_t ink=(uint8_t)(CGU_CONTOUR_BASE+material);
    if(rate){
        for(int row=first;row<end;row++){
            int flow=row*slope/4+cf_rack_wave(row*rate+phase,bend);
            int ripple=row*(rate+3)+phase;
            for(int center=x-28+flow;center<x+w+28;center+=spacing,ripple+=37){
                int start=center+cf_rack_wave(ripple,6);
                cf_rack_contour_span(start,y+row,2+(ripple&1),left,right,ink);
            }
            if((row&15)==15)CF_PUMP();
        }
    } else {
        int cx=x+w*phase/100, cy=36;
        /* Join neighboring points to close each arc at its crest. */
        for(int radius=spacing;radius<=w/2+80;radius+=spacing){
            int span=radius, limit=radius*radius;
            for(int dy=0;dy<=radius && (cy-dy>=first || cy+dy<end);dy++){
                int previous=span*2;
                while(span*span+dy*dy>limit)span--;
                int reach=span*2;
                for(int side=-1;side<=1;side+=2){
                    int row=cy+side*dy;
                    if(row<first || row>=end)continue;
                    int middle=cx+row*slope/4+cf_rack_wave(row*3+phase,bend);
                    int length=previous-reach+2;
                    cf_rack_contour_span(middle-previous,y+row,length,left,right,ink);
                    cf_rack_contour_span(middle+reach,y+row,length,left,right,ink);
                }
            }
            CF_PUMP();
        }
    }
}

static void cf_rack_plate_variant(int x,int y,int w,int h,int ear,uint8_t face,unsigned variation)
{
    fb_rect(x,y,w,h,RAKT_RAIL);
    fb_rect(x+ear,y+1,w-2*ear,h-2,face);
    if(ear==RAKT_EAR)cf_rack_contours(x+ear,y+1,w-2*ear,h-3,face,variation);
    fb_hline(x+1,y,w-2,RAKT_EDGE);
    fb_hline(x+ear,y+h-2,w-2*ear,RAKT_SHADE);
    fb_hline(x+1,y+h-1,w-2,FL_BLACK);
    int inset=ear/2;
    if(ear==RAKT_EAR)inset--;
    for(int side=0;side<2;side++){
        int screw=side?x+w-1-inset:x+inset;
        if(h>=38){
            cf_rack_screw(screw,y+9);
            cf_rack_screw(screw,y+h-7);
        } else cf_rack_screw(screw,y+h/2);
    }
}

static void cf_rack_plate(int x,int y,int w,int h,int ear,uint8_t face)
{
    cf_rack_plate_variant(x,y,w,h,ear,face,RAKT_CONTOUR_DEFAULT);
}

static void cf_rack_cabinet(int x,int y,int w,int h,int opening_top,int opening_h)
{
    fb_rbox(x,y,w,h,2,RAKT_SHADE);
    fb_rect(x+4,opening_top,w-8,opening_h,FL_BLACK);
    fb_hline(x+6,y,w-12,RAKT_EDGE);
    fb_vline(x+1,y+2,h-4,RAKT_EDGE);
    fb_vline(x+w-2,y+2,h-4,RAKT_RAIL);
}

static void cf_rack_window(int x,int y,int w,int h,uint8_t glass)
{
    fb_rbox(x,y,w,h,2,RAKT_EDGE);
    fb_rbox(x+1,y+1,w-2,h-2,2,glass);
}

static void cf_rack_channel(int cx,int y,unsigned level,int cap)
{
    fb_rbox(cx-14,y,13,37,1,RAKT_GLASS);
    for(unsigned i=0;i<6;i++){
        uint8_t ink=i>=level?RAKT_LED_OFF:i==5?RAKT_RED:i==4?RAKT_AMBER:RAKT_GREEN;
        fb_rect(cx-11,y+30-5*(int)i,7,3,ink);
    }
    fb_vline(cx+7,y+2,33,FL_BLACK);
    for(int mark=4;mark<35;mark+=7)fb_hline(cx+12,y+mark,3,RAKT_EDGE);
    fb_rbox(cx+2,cap,11,7,1,RAKT_EDGE);
    fb_hline(cx+3,cap+1,9,RAKT_NICKEL);
    fb_hline(cx+3,cap+4,9,RAKT_SHADE);
}

static int cf_rack_slide(unsigned elapsed,int distance)
{
    int left=RAKT_SLIDE_STEPS-(int)(elapsed/RAKT_SLIDE_STEP_MS);
    if(left<=0)return 0;
    return distance*left*left/(RAKT_SLIDE_STEPS*RAKT_SLIDE_STEPS);
}

static int cf_rack_arrival(uint32_t start,int distance)
{
    if(!cf_motion || !start || (int32_t)(start-cf_motion_epoch)<=0)return 0;
    return cf_rack_slide(cu.now-start,distance);
}
