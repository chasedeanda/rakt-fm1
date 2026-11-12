/* SPDX-License-Identifier: GPL-3.0-only
 * Native 240x240 Songs list. Rendering uses the foreground index only.
 */
typedef struct {
    char name[4][28],meta[4][40],title[24],status[24],hint[44],detail[32],message[64];
    char label[4][12],value[4][14];
    uint8_t row,first,n,mode,choice,skin,pending,cursor,disabled[4];
} csl_model_t;
static csl_model_t csl_last;
static uint8_t csl_screen_valid;
static unsigned csl_draws;
static void csl_model(csl_model_t *m)
{
    memset(m,0,sizeof *m);m->skin=cf_skin;m->mode=cslu.mode;m->choice=cslu.choice;m->cursor=cslu.cursor;
    uint8_t rows[1+CSL_CAPACITY+SQ_NPAT];unsigned n=csl_rows(rows);m->row=cslu.row%n;m->first=m->row/4*4;m->n=n;
    str_cpy(m->title,cslu.mode==CSL_CONFIRM?"Delete 1 Song?":cslu.mode==CSL_RENAME?"Rename Song":cslu.mode==CSL_MANAGE?"Manage Song":"Songs",sizeof m->title);
    str_cpy(m->status,csl_tx.error?"WRITE ERROR":csl_tx.pending?"PENDING":"",sizeof m->status);m->pending=csl_tx.pending;
    for(unsigned i=0;i<4 && m->first+i<n;i++) {
        unsigned row=rows[m->first+i];char *name=m->name[i],*meta=m->meta[i];
        if(!row){str_cpy(name,"+ Save current idea",28);str_cpy(meta,csl_free()<0?"Full / EDIT a saved row":"Progression + musical setup",40);continue;}
        if(row<=CSL_CAPACITY) {
            unsigned slot=row-1;const csl_entry_t *e=&csl_index[slot];
            int pending=csl_tx.pending && csl_tx.slot==slot;
            kint(name,slot+1," ");
            char pending_name[CF_SONG_NAME+1];memcpy(pending_name,csl_io.phrase+8,CF_SONG_NAME);pending_name[CF_SONG_NAME]=0;
            str_cpy(name+str_len(name),pending&&!csl_tx.remove?pending_name:e->unavailable?"Unavailable":e->name,28-str_len(name));
            str_cpy(meta,pending?(csl_tx.remove?"P / Delete pending":"P / Pending: stop + release"):e->unavailable?"P / Restore backup":"P / Saved / ",40);
            if(!pending && e->used){kint(meta+str_len(meta),e->n," slots / ");kint(meta+str_len(meta),e->bpm," bpm");}
        } else {
            unsigned pat=row-1-CSL_CAPACITY;kint(name,sq_number(pat)," Recording");
            unsigned bars=sq_perf[pat].bars?sq_perf[pat].bars:sq_st.pat[pat].bars;
            str_cpy(meta,sq_unavailable&(1u<<pat)?"R / Unavailable":sqk.on&&sqk.pat==pat?"R / Recording":sq_slot_check(pat)!=sq_saved[pat]?"R / Pending":"R / Saved",40);
            if(bars){str_cpy(meta+str_len(meta)," / ",40-str_len(meta));kint(meta+str_len(meta),bars," bars");}
        }
    }
    str_cpy(m->hint,cslu.mode==CSL_LIST?"SELECT row / SEL load / EDIT manage":cslu.mode==CSL_MANAGE?"K1 action / SEL do / EDIT back":cslu.mode==CSL_CONFIRM?"K1 choice / SEL confirm":"K1 position / K2 letter / SEL save",sizeof m->hint);
    if(cslu.mode==CSL_LIST && !rows[m->row])str_cpy(m->hint,"SEL saves current / HOME returns",sizeof m->hint);
    if(cslu.mode==CSL_MANAGE)str_cpy(m->detail,CSL_ACTION[cslu.action],sizeof m->detail);
    if(cslu.mode==CSL_CONFIRM)str_cpy(m->detail,cslu.choice?"Delete selected Song":"Cancel",sizeof m->detail);
    if(cslu.mode==CSL_RENAME)str_cpy(m->detail,cslu.name,sizeof m->detail);
    memset(m->disabled,1,sizeof m->disabled);m->disabled[0]=0;
    str_cpy(m->label[0],m->mode==CSL_LIST?"Song":m->mode==CSL_CONFIRM?"Choice":m->mode==CSL_RENAME?"Position":"Action",sizeof m->label[0]);
    if(m->mode==CSL_LIST){kint(m->value[0],m->row+1,"/");kint(m->value[0]+str_len(m->value[0]),m->n,0);}
    else if(m->mode==CSL_CONFIRM)str_cpy(m->value[0],m->choice?"Delete":"Cancel",sizeof m->value[0]);
    else if(m->mode==CSL_RENAME){
        kint(m->value[0],m->cursor+1,0);str_cpy(m->label[1],"Letter",sizeof m->label[1]);
        m->value[1][0]=cslu.name[m->cursor];m->disabled[1]=0;
    } else str_cpy(m->value[0],CSL_ACTION[cslu.action],sizeof m->value[0]);
    if(cu.pop==POP_MSG)str_cpy(m->message,cu.msg,sizeof m->message);
}
static void csl_draw(void)
{
    csl_model_t m;csl_model(&m);cf_stage_valid=cf_acc_valid=cfd_valid=0;
    if(csl_screen_valid && !fb_full && !memcmp(&m,&csl_last,sizeof m))return;
    csl_last=m;csl_screen_valid=1;csl_draws++;
    cfn_page_header(m.title,m.status,C_ACC(0));
    for(unsigned i=0;i<4 && m.first+i<m.n;i++) {
        int y=34+i*37;if(m.row==m.first+i)fb_rect(7,y,226,36,FL_BG2);
        cfn_fit(&CFN_16,12,y+16,m.name[i],216,FL_INK);cfn_fit(&CFN_11,12,y+31,m.meta[i],216,C_ACC(0));
    }
    cfn_hint(m.hint,FL_INK2);
    const uint8_t emphasis[4]={0};cfn_controls(m.label,m.value,m.disabled,emphasis);
    if(m.mode!=CSL_LIST || m.message[0]) {
        cfn_dialog(8,70,224,100,FL_BG3);
        cfn_fit(&CFN_12,16,93,m.message[0]?"Songs":(m.mode==CSL_CONFIRM || m.mode==CSL_MANAGE)?m.name[m.row-m.first]:m.title,208,FL_INK2);
        cfn_fit(&CFN_16,16,120,m.message[0]?m.message:m.detail,208,C_ACC(0));
        if(m.mode==CSL_RENAME){char pos[20];kint(pos,m.cursor+1," / 24");cfn_text(&CFN_12,16,147,pos,FL_INK2);}
        else cfn_fit(&CFN_12,16,147,m.message[0]?"":m.hint,208,FL_INK2);
    }
    cf_present();
}
