/* SPDX-License-Identifier: GPL-3.0-only
 * One foreground list: saved semantic ideas and existing fixed-note takes.
 */
enum { CSL_LIST, CSL_MANAGE, CSL_CONFIRM, CSL_RENAME };
enum { CSL_LOAD, CSL_DUPLICATE, CSL_RENAME_ACTION, CSL_DELETE, CSL_NACTION };
static const char *const CSL_ACTION[]={"Load","Duplicate","Rename","Delete"};
static struct { uint8_t row, mode, action, choice, cursor;char name[CF_SONG_NAME+1]; } cslu;
static void cgu_open_take(unsigned pat);
/* 0 = Save current; 1..6 = persistent ideas; 7..9 = recorder slots. */
static unsigned csl_rows(uint8_t rows[1+CSL_CAPACITY+SQ_NPAT])
{
    unsigned n=0;rows[n++]=0;
    for(unsigned i=0;i<CSL_CAPACITY;i++)if(csl_index[i].used || csl_index[i].unavailable || (csl_tx.pending && csl_tx.slot==i))rows[n++]=1+i;
    for(unsigned i=0;i<SQ_NPAT;i++){
        unsigned pat=sq_view_slot(i);
        if(sq_has(pat) || (sq_unavailable&(1u<<pat)))rows[n++]=1+CSL_CAPACITY+pat;
    }
    return n;
}
static unsigned csl_selected(void)
{ uint8_t rows[1+CSL_CAPACITY+SQ_NPAT];unsigned n=csl_rows(rows);return rows[cslu.row%n]; }
static void csl_open(void)
{ cslu.row=0;cslu.mode=CSL_LIST;cslu.action=0;cu.scr=SCR_SONGS;cu.pop=POP_NONE; }
static void csl_select(int d)
{
    if(cslu.mode!=CSL_LIST)return;
    uint8_t rows[1+CSL_CAPACITY+SQ_NPAT];int n=csl_rows(rows);
    cslu.row=(uint8_t)((cslu.row+d%n+n)%n);
}
static int csl_available(void)
{
    if(cr_safe || cfs_restore_lock){cf_message("Songs unavailable: restart");return 0;}
    if(csl_tx.pending){cf_message(csl_tx.error?"Write failed: retry pending":"Song pending: stop/release to save");return 0;}
    if(sqk.on || cf_sn.sq_rec!=255){cf_message("Finish recording first");return 0;}
    return 1;
}
static void csl_seal(void)
{ csl_io.check=cfs_fnv(&csl_io,__builtin_offsetof(csl_record_t,check)); }
static void csl_capture_patch(unsigned part)
{
    cfk_rec_t *r=&csl_io.patch[part];const track_t *t=&trk[part];const eng_deep_t *d=cfk_deep(t);
    memset(r,0,sizeof *r);r->magic=CFK_MAGIC;r->part=part;r->edited=1;r->np=P_COUNT;
    r->engine=cf_snd[part].engine;r->kind=cf_snd[part].kind;r->fk=cf_snd[part].fk;r->slot=cf_snd[part].slot;
    memcpy(r->name,cf_snd[part].name,sizeof r->name);
    for(unsigned i=0;i<P_COUNT;i++)r->p[i]=t->p[i];
    if(d){r->blen=d->blob_size;d->blob_get(t,r->blob);}
    r->check=cfs_fnv(r,__builtin_offsetof(cfk_rec_t,check));
    csl_io.sound[part]=cf_snd[part];csl_io.trim[part]=t->trim;
}
static void csl_save_current(void)
{
    if(!csl_available())return;
    int slot=csl_free();if(slot<0){cf_message("Songs full: delete a saved idea");return;}
    if(cpa_setup.flags || cpa_edit.flags){cf_message("Wait for queued setup to play");return;}
    for(unsigned p=0;p<2;p++)if(trk[p].xf_on || trk[p].eng_req!=trk[p].engine){cf_message("Wait for sound change to finish");return;}
    cfd_ui_poll();cf_song_t song;
    if((cfdu.studio || cpg.idea==CF_P_DRAFT) && cfwd.have)song=cfwd.song;
    else if(cpg.idea==CF_P_ENGINE){if(!cge_snapshot(&song)){cf_message("Engine snapshot unavailable");return;}}
    else if(!cf_song_clone(&song,cf_progression_current())){cf_message("No progression to save");return;}
    if(!cfdu.studio){
        song.tonic=cs.chord.tonic;song.scale=cs.chord.scale;song.bpm=cs.bpm;
        song.rhythm=cpg.rhythm<DM_N?DM_RHY[cpg.rhythm].id:cpg.rhythm==CPA_CLICK?65534:65535;
        if(cpg.rhythm<DM_N){song.kit=dm_kit(cpg.rhythm);song.swing=dm_swing(cpg.rhythm);}
        song.drums_only=cpg.mode;song.repeat_off=cpg.repeat_off;
    }
    memset(&csl_io,0,sizeof csl_io);csl_io.magic=CSL_MAGIC;csl_io.version=1;csl_io.size=sizeof csl_io;
    csl_io.settings_size=sizeof csl_io.setup;csl_io.sound_size=sizeof csl_io.patch[0];
    if(!cf_song_encode(&song,csl_io.phrase)){cf_message("Song setup cannot be saved");return;}
    csl_io.setup=cs;csl_io.setup.arp_keep=cu.arp_on;csl_io.harmony=ch;for(unsigned p=0;p<2;p++)csl_capture_patch(p);csl_seal();
    if(!csl_valid(&csl_io)){cf_message("Song setup cannot be saved");return;}
    csl_stage(slot,0);cslu.mode=CSL_LIST;
    uint8_t rows[1+CSL_CAPACITY+SQ_NPAT];unsigned n=csl_rows(rows);for(unsigned i=0;i<n;i++)if(rows[i]==1u+(unsigned)slot)cslu.row=i;
    cf_message("Pending: stop/release to save");
}
static int csl_recall(unsigned slot)
{
    if(!csl_available())return 0;
    if(!cfd_audio_quiet() || cf_sn.dm_on){cf_message("Stop/release notes to load");return 0;}
    if(cfwd.have && cfwd.dirty){cf_message("Save working draft before loading");return 0;}
    if(!csl_read(slot)){cf_message("Song unavailable: restore backup");return 0;}
    cf_song_t song;cf_song_decode(&song,csl_io.phrase,sizeof csl_io.phrase);
    /* Restore musical settings explicitly; system, USB and appearance stay local. */
    const cf_set_t *s=&csl_io.setup;
    cs.chord=s->chord;cs.glide_ms=s->glide_ms;cs.glide_always=s->glide_always;cs.vel=s->vel;
    cs.bass_on=s->bass_on;cs.octaver=s->octaver;cs.bass_oct=s->bass_oct;cs.bass_level=s->bass_level;
    cs.level_chord=s->level_chord;cs.level_layer=s->level_layer;cs.level_drums=s->level_drums;
    cs.metro_level=s->metro_level;cs.click=s->click;cs.env_link=s->env_link;cs.retrig=s->retrig;cs.arp=s->arp;cs.fx=s->fx;
    ch=csl_io.harmony;cu.arp_on=s->arp_keep;
    for(unsigned p=0;p<2;p++) {
        const cfk_rec_t *r=&csl_io.patch[p];cf_pool_load(p,r->engine,0);cfk_own[p]=0;
        /* A recalled patch is detached from mutable user preset slots. Saving
         * this sound later must create a sound, not overwrite its old source. */
        fm1_irq_off();for(unsigned i=0;i<P_COUNT;i++)if(!param_kept(i) || i==P_PAN || i==P_MUTE || (i>=P_DIST && i<=P_REV))trk[p].p[i]=r->p[i];trk[p].trim=csl_io.trim[p];fm1_irq_on();
        const eng_deep_t *d=cfk_deep(&trk[p]);if(d)d->blob_set(&trk[p],r->blob);
        cf_edited(p);cs.snd[p]=cf_snd_pos(p);
    }
    cf_song_draft_begin(&cfwd,&song);memset(&cfds,0,sizeof cfds);cfd_changed();cfdu.focus=0;
    if(cfdu.studio){cu.scr=SCR_PEDIT;cu.pop=POP_NONE;return 1;}
    cfd_view_update();cpg.idea=CF_P_DRAFT;cs.chord.tonic=song.tonic;cs.chord.scale=song.scale;
    cfd_select_source();cf_send_config();cu_spell_update();
    cu.scr=SCR_HOME;home_boot_main=1;cf_home_bank=CF_HB_CORE;cf_home_bank_ms=0;cu.pop=POP_NONE;
    return 1;
}
static void csl_execute(void)
{
    unsigned row=csl_selected();
    if(!row){csl_save_current();return;}
    if(row>CSL_CAPACITY) {
        unsigned pat=row-1-CSL_CAPACITY;
        if(cslu.mode==CSL_CONFIRM) {
            if(cslu.choice && (!csl_available() || !cfd_audio_quiet())){cf_message("Stop/release notes to delete");return;}
            if(cslu.choice)sq_clear_take(pat);
            cslu.mode=CSL_LIST;return;
        }
        if(cslu.mode==CSL_MANAGE && cslu.action==CSL_DELETE){cslu.mode=CSL_CONFIRM;cslu.choice=0;return;}
        if(!csl_available())return;
        if(cpa.on || cpa_commanded || sqp.on || cf_sn.sq_on || cf_sn.sq_rec!=255 || cf_sn.dm_on){
            cf_message("Stop playback to open the take");return;
        }
        if(!sq_has(pat)){cf_message("Recording unavailable");return;}
        if(CF_STUDIO && !CF_SCENES){cgu_open_take(pat);return;}
        sqk.last=pat;sqk.song=0;squ.pat=pat;sq_activate(pat);cu.scr=SCR_SQ;cu.pop=POP_NONE;return;
    }
    unsigned slot=row-1;
    if(cslu.mode==CSL_LIST){(void)csl_recall(slot);return;}
    if(!csl_available())return;
    if(cslu.mode==CSL_CONFIRM) {
        if(cslu.choice)csl_stage(slot,1);cslu.mode=CSL_LIST;return;
    }
    if(cslu.mode==CSL_RENAME) {
        if(!csl_read(slot))return;
        cf_song_t song;cf_song_decode(&song,csl_io.phrase,sizeof csl_io.phrase);
        unsigned end=CF_SONG_NAME;while(end && cslu.name[end-1]==' ')end--;cslu.name[end]=0;
        if(!end){cf_message("Enter a Song name");return;}
        memset(song.name,0,sizeof song.name);memcpy(song.name,cslu.name,end);
        cf_song_encode(&song,csl_io.phrase);csl_seal();csl_stage(slot,0);cslu.mode=CSL_LIST;return;
    }
    if(cslu.action==CSL_LOAD){(void)csl_recall(slot);return;}
    if(cslu.action==CSL_DELETE){cslu.mode=CSL_CONFIRM;cslu.choice=0;return;}
    if(cslu.action==CSL_DUPLICATE) {
        int to=csl_free();if(to<0){cf_message("Songs full: delete a saved idea");return;}
        if(csl_read(slot)){csl_stage(to,0);cslu.mode=CSL_LIST;}return;
    }
    if(cslu.action==CSL_RENAME_ACTION && csl_index[slot].used) {
        memset(cslu.name,' ',CF_SONG_NAME);cslu.name[CF_SONG_NAME]=0;
        memcpy(cslu.name,csl_index[slot].name,str_len(csl_index[slot].name));cslu.cursor=0;cslu.mode=CSL_RENAME;
    }
}
static void csl_turn(int k,int d)
{
    if(cslu.mode==CSL_LIST){if(k==0)csl_select(d);return;}
    if(cslu.mode==CSL_CONFIRM){if(!k)cslu.choice=d>0;return;}
    if(cslu.mode==CSL_RENAME){if(!k)cslu.cursor=wrapi(cslu.cursor+d%CF_SONG_NAME,CF_SONG_NAME);if(k==1)cslu.name[cslu.cursor]=(char)(32+wrapi(cslu.name[cslu.cursor]-32+d%95,95));return;}
    if(!k){if(csl_selected()>CSL_CAPACITY)cslu.action=cslu.action==CSL_DELETE?CSL_LOAD:CSL_DELETE;else cslu.action=wrapi(cslu.action+d%CSL_NACTION,CSL_NACTION);}
}
static void csl_edit(void)
{ if(!csl_selected())return;if(cslu.mode!=CSL_LIST){cslu.mode=CSL_LIST;return;}cslu.mode=CSL_MANAGE;cslu.action=CSL_LOAD; }
