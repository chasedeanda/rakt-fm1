/* SPDX-License-Identifier: GPL-3.0-only */
#define CF_STUDIO 1
#define CF_ENGINE_HELPERS_ONLY
#include "cf_engine_test.c"

static unsigned studio_genre_index(const char *id)
{
    for(unsigned genre=0;genre<CGS_NGENRE;genre++){
        const char *profile_id=CGS_PROFILES[genre].id;
        if(profile_id && !strcmp(id,profile_id))return genre;
    }
    return CGS_NGENRE;
}

static void studio_start_profile(uint32_t seed,unsigned genre)
{
    cpa_stop();
    sqp_stop();
    dmp_stop();
    cfp_panic();
    cf_boot_go=0;
    cfb.on=0;
    cf_intro_on=0;
    cu.scr=SCR_HOME;
    home_boot_main=1;
    cf_home_bank=CF_HB_ENGINE;
    cf_scene_set(CF_SCENE_PROGRESSIONS);
    cs.chord.tonic=seed&1u?4:9;
    cs.chord.scale=seed&1u?CF_SC_MAJOR:CF_SC_MINOR;
    cs.beats=4;
    cs.bpm=104;
    cs.clock_in=2;
    cs.midi_out=1;
    cs.ch_chord=0;
    cs.ch_bass=1;
    cs.ch_layer=2;
    cs.ch_drums=3;
    cu.arp_on=0;
    cgs_init(seed);
    cgs.genre=(uint8_t)genre;
    cgs_prepare_start();
    cf_send_config();
    cf_send_metro();
    blocks(16);
    usb.up=usb.config=1;
    mo_r=mo_w;
    cgu_home();
    ui_button(B_PLAY);
}

static void studio_start(uint32_t seed)
{
    studio_start_profile(seed,CGS_DEFAULT_GENRE);
}

static void genre_registry_studio(void)
{
    for(unsigned genre=0;genre<CGS_NGENRE;genre++){
        const cgs_profile_t *profile=&CGS_PROFILES[genre];
        CHECK(profile->id && profile->id[0] &&
              strspn(profile->id,"abcdefghijklmnopqrstuvwxyz0123456789-")==strlen(profile->id));
        if(profile->id)CHECK(studio_genre_index(profile->id)==genre);
        CHECK(profile->name && profile->name[0]);
        CHECK(profile->bpm && profile->primary>CF_P_ALL && profile->primary<CF_P_NTAGS &&
              profile->secondary>CF_P_ALL && profile->secondary<CF_P_NTAGS);
        CHECK(profile->family<CGS_NFAMILY && profile->kit_count>=2 && profile->cabinet<=CGS_CAB_TOLEX);
        for(unsigned kit=0;kit<profile->kit_count;kit++)CHECK(profile->kit[kit]<SQ_NKIT);
        CHECK(profile->bank_duration_scale && profile->evolve_period_scale);
        CHECK(profile->composer<=CGS_COMPOSE_PUNK && profile->layer_floor<=116u);
        CHECK(profile->feel_count==(profile->composer==CGS_COMPOSE_POCKET?0u:4u));
        for(unsigned style=0;style<2;style++){
            CHECK(profile->bass_reply[style]<=CGS_BASS_REPLY_LATE);
            CHECK(profile->layer_reply[style]<=CGS_LAYER_REPLY_DREAM);
        }
        if(profile->form){
            CHECK(!profile->form[0]);
            for(unsigned bar=0;bar<CGS_FORM_BARS;bar++)
                CHECK(!(profile->form[bar]&~(CGS_FORM_LAYER_REST|CGS_FORM_LIGHT_HATS|CGS_FORM_FILL)));
        }
        for(unsigned part=0;part<CGS_NPART;part++)
            for(unsigned style=0;style<2;style++)
                CHECK(profile->style[part][style] && profile->style[part][style][0]);

        if(profile->composer!=CGS_COMPOSE_POCKET)continue;
        CHECK(profile->bass_style<=CGS_BASS_PEDAL && profile->layer_style<=CGS_LAYER_SWELL);
        if(profile->layer_style==CGS_LAYER_SEQUENCE)CHECK(profile->layer_start+10u<CGS_STEPS);
        if(!profile->sparse_drums)CHECK(profile->pocket.hat_gap && profile->pocket.snare<CGS_STEPS);
        for(unsigned style=0;style<2;style++){
            const cgs_riff_t *riff=&profile->bass[style];
            CHECK(riff->pocket>=CGS_FOLLOW && riff->pocket<=CGS_FLOAT);
            for(unsigned i=0;i<NELEM(riff->cell);i++){
                const uint8_t *cell=riff->cell[i];
                CHECK(cell[0]+(unsigned)(riff->pocket==CGS_ANSWER && i==3)<CGS_STEPS);
                CHECK(cell[1]>=CGS_ANCHOR && cell[1]<=CGS_STEP_IN && cell[2] && cell[2]<=31u);
            }
        }
    }
}

static void controls_studio(void)
{
    studio_start(101);
    blocks(1);
    CHECK(cgs_active() && cpa.t==0 && sqp.n && sqp.nb && dmp.on);
    sq_snd_poll();
    CHECK(!strcmp(cf_snd[0].name,"SOFT LENS") && !strcmp(cf_snd[1].name,"SILK PICK"));
    CHECK(!strcmp(cf_snd[2].name,"DREAM PLUCK"));
    CHECK(!cfa_sq[0].on && cs.bpm==104);
    ticks(3*CGS_TICKS+3);
    CHECK(sqp.n && sqp.ln);
    ui_algo(1);
    CHECK(cgs.focus==CGS_BASS);
    ui_button(B_SCL);
    CHECK(cgs.zoom);
    for(unsigned part=0;part<CGS_NPART;part++){
        cgs_clip_t before[CGS_NPART];
        uint32_t seeds[CGS_NPART];
        memcpy(before,cgs.active,sizeof before);
        memcpy(seeds,cgs.seed,sizeof seeds);
        unsigned grid=cf_grid.gen, chord=cpa.slot, time=cpa.q2;
        cgs.focus=(uint8_t)part;
        ui_button(B_EDIT);
        CHECK(cgs.pending==(1u<<part));
        ticks(CGS_TICKS);
        CHECK(cpa.q2==time+2*CGS_TICKS && cpa.slot==chord && cf_grid.gen==grid);
        CHECK(!cgs.pending && memcmp(&before[part],&cgs.active[part],sizeof before[part]));
        for(unsigned p=0;p<CGS_NPART;p++)if(p!=part){
            CHECK(cgs.seed[p]==seeds[p]);
            CHECK(!memcmp(&before[p],&cgs.active[p],sizeof before[p]));
        }
    }
    cgs.focus=CGS_LAYER;
    ui_button(B_GLO);
    ui_button(B_EDIT);
    ui_button(B_GLO);
    blocks(1);
    CHECK((cgs.mute_applied&(1u<<CGS_LAYER)) && !sqp.ln && sqp.n);
    ui_button(B_EDIT);
    ticks(CGS_TICKS);
    CHECK(!sqp.ln);
    ui_button(B_HOME);
    CHECK(!cgs.zoom && cgs.focus==CGS_LAYER);
    unsigned harmony=cpa.plan.idea, grid=cf_grid.gen;
    ui_button(B_EDIT);
    CHECK(cpa_setup.flags && cpa.plan.idea==harmony);
    ticks(CGS_BAR-cpa.t);
    CHECK(cgs_active() && cgs.harmony_generation==1 && cpa.plan.idea!=harmony && cf_grid.gen==grid);
    CHECK(cgs.muted==(1u<<CGS_LAYER));
    midi_event(0x90,0,60,105);
    CHECK(!sqp.n && cr_mnote[0][60]);
    ui_button(B_SCL);
    cgs.focus=CGS_CHORDS;
    ui_button(B_EDIT);
    ticks(CGS_TICKS);
    CHECK(!sqp.n && cr_mnote[0][60] && trk_voice_of(&trk[0],60));
    midi_event(0x80,0,60,0);
    ui_button(B_PLAY);
    blocks(1);
    CHECK(!cgs_active() && !sqp.n && !sqp.nb && !sqp.ln);
    cgs.mix_idle=1;
    ui_knob_turn(2,-100);
    blocks(300);
    CHECK(!cgs.gain[CGS_CHORDS] && !cpa.on);
}

static void ui_save_frame(const char *name)
{
    char path[96];
    snprintf(path,sizeof path,"build/studio/ui/%s.ppm",name);
    FILE *file=fopen(path,"wb");
    CHECK(file);
    if(!file)return;
    fprintf(file,"P6\n%d %d\n255\n",FB_W,FB_H);
    for(unsigned i=0;i<FB_W*FB_H;i++){
        uint32_t color=cf_pal[fb[i]];
        uint8_t rgb[]={(uint8_t)(color>>16),(uint8_t)(color>>8),(uint8_t)color};
        fwrite(rgb,1,sizeof rgb,file);
    }
    fclose(file);
}
static void ui_capture(const char *name)
{
    cf_snapshot();
    cf_ui_draw();
    ui_save_frame(name);
}

static void hover_controls_studio(void)
{
    studio_start(207);
    blocks(2);
    sq_snd_poll();
    cf_motion=0;
    cu.now=1000;
    CHECK(cgs.focus==CGS_CHORDS && !cgs.zoom);
    unsigned key=cs.chord.tonic, scale=cs.chord.scale, bpm=cs.bpm;
    unsigned offset=CGU_RACK_CONTROLS_Y*FB_W;
    cf_ui_draw();
    uint32_t before=cfs_fnv(fb+offset,CGU_FACE_CONTROLS_H*FB_W);
    ui_knob_turn(0,1);
    cf_ui_draw();
    CHECK(cgs.style[CGS_CHORDS]==1 && !cgs.style[CGS_BASS] && !(cpa_edit.flags&CPA_NOTES));
    CHECK(!strcmp(cf_stage_last.value[0],"Soft pulse") && cf_stage_last.emphasis[0] &&
        cfs_fnv(fb+offset,CGU_FACE_CONTROLS_H*FB_W)!=before);
    ui_algo(1);
    cf_ui_draw();
    CHECK(cf_stage_last.focus==CGS_BASS && cf_stage_last.rack_scroll==0 &&
        fb[(CGU_RACK_Y+cgu_rack_offset(CGU_STUDIO,1)+cgu_rack_height(CGU_STUDIO,1)/2)*FB_W+4]==CGU_INK_BASE+CGS_BASS);
    unsigned activity=cgs.activity[CGS_BASS], chords=cgs.activity[CGS_CHORDS];
    ui_knob_turn(1,1);
    CHECK(cgs.activity[CGS_BASS]==activity+1 && cgs.activity[CGS_CHORDS]==chords);
    CHECK(cs.chord.tonic==key && cs.chord.scale==scale && cs.bpm==bpm);
    ui_capture("hover-studio-part");
    cf_stage_model_t hover=cf_stage_last;
    ui_button(B_SCL);
    cf_ui_draw();
    CHECK(!memcmp(hover.label,cf_stage_last.label,sizeof hover.label) &&
        !memcmp(hover.value,cf_stage_last.value,sizeof hover.value));
    ui_select(1);
    cf_ui_draw();
    CHECK(!strcmp(cf_stage_last.label[0],"KEY"));
    ui_button(B_SCL);
    ui_algo(3);
    CHECK(cgu.band_panel==CGU_BAND_HARMONY && !cgs.zoom);
    ui_knob_turn(0,1);
    CHECK(cpa_edit.flags&CPA_NOTES);
    ui_capture("hover-harmony");
    char queued_key[14];
    kpc(queued_key,(key+1)%12);
    CHECK(!strcmp(cf_stage_last.value[0],queued_key) && !strcmp(cf_stage_last.label[3],"METER"));

    ui_select(1);
    CHECK(cgu.tab==CGU_SOUND && cgs.focus==CGS_BASS && !cgs.zoom);
    unsigned bass=cf_snd_pos(CGS_BASS), layer=cf_snd_pos(CGS_LAYER);
    ui_knob_turn(0,1);
    CHECK(cf_snd_pos(CGS_BASS)!=bass && cf_snd_pos(CGS_LAYER)==layer && !cgu.all_sounds);
    int cutoff=trk[CGS_BASS].p[P_E0];
    ui_knob_turn(2,cutoff<ENGINES[cf_snd[CGS_BASS].engine]->edit[0].max?1:-1);
    ui_capture("hover-sound-tone");
    CHECK(trk[CGS_BASS].p[P_E0]!=cutoff && !strcmp(cf_stage_last.value[0],cf_snd[CGS_BASS].name) &&
        !strcmp(cf_stage_last.label[0],"PRESET") && !strcmp(cf_stage_last.label[1],"LEVEL") &&
        cf_stage_last.emphasis[2]);
    ui_algo(1);
    cf_ui_draw();
    CHECK(cf_stage_last.focus==CGS_DRUMS && cf_stage_last.rack_scroll==cgu_rack_offset(CGU_SOUND,1) &&
        fb[(CGU_RACK_Y+cgu_rack_offset(CGU_SOUND,2)-cf_stage_last.rack_scroll+cgu_rack_height(CGU_SOUND,2)/2)*FB_W+4]==CGU_INK_BASE+CGS_DRUMS &&
        fb[(CGU_RACK_CONTROLS_Y+5)*FB_W+10]==CGU_INK_BASE+CGS_DRUMS);
    ui_algo(-1);
    ui_button(B_SCL);
    ui_select(1);
    CHECK(cgu.sound_page==CGU_ENVELOPE);
    ui_button(B_SCL);
    bass=cf_snd_pos(CGS_BASS);
    ui_knob_turn(0,1);
    CHECK(cf_snd_pos(CGS_BASS)!=bass);
    ui_button(B_SCL);
    CHECK(cgu.sound_page==CGU_ENVELOPE);
    ui_button(B_SCL);
    ui_algo(3);
    CHECK(cgu.rack_controls && !cgs.zoom);
    bass=cf_snd_pos(CGS_BASS);
    ui_knob_turn(0,1);
    ui_knob_turn(1,1);
    CHECK(cgu.all_sounds && cf_snd_pos(CGS_BASS)==bass);
    ui_knob_turn(0,-1);

    ui_select(1);
    CHECK(cgu.tab==CGU_RECORD && !cgu.rack_controls);
    unsigned take=cgu.take_slot;
    ui_knob_turn(0,1);
    CHECK(cgu.take_slot==take);
    ui_algo(3);
    ui_knob_turn(0,1);
    cf_ui_draw();
    CHECK(cgu.rack_controls && cgu.take_slot==(take+1)%SQ_NPAT && !strcmp(cf_stage_last.label[0],"TAKE"));
    cgu.take_slot=(uint8_t)take;
    cpa_stop();
    cgu_home();
}

static void workspaces_studio(void)
{
    studio_start(207);
    blocks(3);
    cgs.focus=CGS_BASS;
    cf_motion=1;
    cf_motion_epoch=0;
    cu.now=1000;
    ui_capture("studio");
    CHECK(cf_stage_last.theory[0] && cf_stage_last.degree[0]);
    CHECK(!strcmp(cf_stage_last.label[0],"STYLE") && !strcmp(cf_stage_last.value[0],"Hook"));
    ui_algo(1);
    CHECK(cgs.focus==CGS_DRUMS && cgu.band_panel==CGU_BAND);
    ui_algo(1);
    CHECK(cgs.focus==CGS_LAYER && cgu.band_panel==CGU_BAND);
    cu.now+=CGU_RACK_SCROLL_MS;
    ui_capture("studio-amps-lower");
    unsigned pending=cf_evq_w;
    for(unsigned panel=CGU_BAND_HARMONY;panel<CGU_BAND_PANELS;panel++){
        ui_algo(1);
        CHECK(cgu.band_panel==panel && cgs.focus==CGS_LAYER && cf_evq_w==pending);
    }
    cu.now+=CGU_RACK_SCROLL_MS;
    ui_capture("studio-rack-lower");
    CHECK(cgu_workspace_tile(&cf_stage_last)==CGU_RACK_COUNT-1);
    cgu_face_t top=cgu_rack_face(CGU_STUDIO,0),bottom=cgu_rack_face(CGU_STUDIO,1);
    cgu_face_t last=cgu_rack_face(CGU_STUDIO,CGU_RACK_COUNT-1);
    cgu_face_t previous_unit=cgu_rack_face(CGU_STUDIO,CGU_RACK_COUNT-2);
    CHECK(top.y+top.h+RAKT_GUTTER==bottom.y && top.x==bottom.x && top.w==bottom.w);
    CHECK(cgu_rack_face(CGU_SOUND,1).y==bottom.y && cgu_rack_face(CGU_SOUND,1).w==bottom.w);
    CHECK(previous_unit.y+previous_unit.h+RAKT_GUTTER==last.y && previous_unit.w==last.w);
    CHECK(last.y-cgu_rack_top(CGU_STUDIO,CGU_RACK_COUNT-1)+last.h<=CGU_RACK_Y+CGU_BAY_H);
    ui_algo(1);
    CHECK(cgs.focus==CGS_CHORDS && cgu.band_panel==CGU_BAND);
    ui_algo(-1);
    CHECK(cgu.band_panel==CGU_BAND_PROGRESSION && cgs.focus==CGS_CHORDS);
    ui_algo(2);
    CHECK(cgs.focus==CGS_BASS && cgu.band_panel==CGU_BAND);
    ui_button(B_SEQ);
    CHECK(cgu.band_panel==CGU_BAND_HARMONY && !cgs.zoom);
    ui_button(B_SEQ);
    CHECK(cgu.band_panel==CGU_BAND_SHAPING);
    ui_button(B_SCL);
    cu.now+=180;
    ui_capture("studio-shaping");
    CHECK(cf_stage_last.zoom==100 && cf_stage_last.bank==CGU_BAND_SHAPING);
    CHECK(!strcmp(cf_stage_last.label[0],"SPACE") && !strcmp(cf_stage_last.label[2],"ENERGY") &&
        !strcmp(cf_stage_last.label[3],"EVOLVE"));
    unsigned space=cgs.space, energy=cgs.energy, evolve=cgs.evolution, genre=cgs.genre;
    ui_knob_turn(0,1);
    ui_knob_turn(2,1);
    ui_knob_turn(3,1);
    CHECK(cgs.space==space+1 && cgs.energy==energy+1 && cgs.evolution==evolve+1 && cgs.genre==genre);
    cgs.space=(uint8_t)space;
    cgs_energy_turn(-1);
    cgs.evolution=(uint8_t)evolve;
    ui_button(B_SCL);
    cu.now+=180;
    CHECK(cgu.band_panel==CGU_BAND_SHAPING && !cgs.zoom);
    ui_button(B_SEQ);
    CHECK(cgu.band_panel==CGU_BAND_GENRE);
    ui_button(B_SCL);
    cu.now+=180;
    ui_capture("studio-genre");
    CHECK(cf_stage_last.zoom==100 && !strcmp(cf_stage_last.label[0],"GENRE"));
    CHECK(!strcmp(cf_stage_last.label[1],"FAMILY") && !strcmp(cf_stage_last.value[1],"Guitar"));
    ui_knob_turn(1,1);
    CHECK(cgs.genre==studio_genre_index("new-wave"));
    ui_knob_turn(1,1);
    CHECK(cgs.genre==studio_genre_index("ambient"));
    ui_knob_turn(1,1);
    CHECK(cgs.genre==CGS_DEFAULT_GENRE);
    ui_button(B_SCL);
    cu.now+=180;
    ui_algo(4);
    CHECK(cgu.band_panel==CGU_BAND && cgs.focus==CGS_BASS);
    char degree[8];
    cf_lab_numeral(CF_SC_MAJOR,0,2,0x89u,degree);
    CHECK(!strcmp(degree,"ii"));
    cf_lab_numeral(CF_SC_MAJOR,0,5,0x91u,degree);
    CHECK(!strcmp(degree,"IV"));
    uint32_t header=cfs_fnv(fb,FB_W*28);
    unsigned clock=cf_grid.gen, q2=cpa.q2;
    ui_button(B_SCL);
    CHECK(cgs.zoom && cgu_zoom()==0);
    cu.now+=25;
    CHECK(cgu_zoom()>0 && cgu_zoom()<100);
    ui_capture("zoom-in");
    CHECK(cfs_fnv(fb,FB_W*28)==header);
    unsigned mid=cgu_zoom();
    ui_button(B_SCL);
    CHECK(!cgs.zoom && cgu_zoom()==mid);
    cu.now+=180;
    CHECK(cgu_zoom()==0);
    ui_button(B_SCL);
    cu.now+=180;
    ui_capture("studio-bass");
    CHECK(cf_stage_last.zoom==100 && cf_stage_last.focus==CGS_BASS);
    for(unsigned direction=0;direction<2;direction++){
        ui_button(B_SCL);
        for(unsigned frame=0;frame<6;frame++){
            char name[24];
            snprintf(name,sizeof name,"camera-%u-%u",direction,frame);
            cu.now+=30;
            ui_capture(name);
            CHECK(cfs_fnv(fb,FB_W*28)==header);
        }
    }
    ui_select(1);
    CHECK(cgu.tab==CGU_STUDIO && cgs.zoom && cgu.part_page==CGU_PART_HARMONY);
    ui_select(-1);
    CHECK(cgu.part_page==CGU_PART);
    ui_button(B_SCL);
    cu.now+=180;
    CHECK(cgu.band_panel==CGU_BAND && !cgs.zoom);
    ui_select(1);
    CHECK(cgu.tab==CGU_SOUND && !cgs.zoom && cgs.focus==CGS_BASS);
    ui_capture("rack-slide");
    CHECK(cf_stage_last.bank_dx>0);
    cu.now+=CGU_RACK_SCROLL_MS;
    ui_capture("sound");
    CHECK(!cf_stage_last.bank_dx);
    ui_button(B_SCL);
    cu.now+=180;
    ui_capture("sound-bass");
    ui_select(1);
    CHECK(cgu.sound_page==CGU_ENVELOPE && cgs.zoom);
    ui_capture("sound-envelope");
    ui_select(1);
    CHECK(cgu.sound_page==CGU_SENDS);
    unsigned send=trk[CGS_BASS].p[P_REV];
    ui_knob_turn(3,send<127?1:-1);
    CHECK(trk[CGS_BASS].p[P_REV]!=send);
    for(int k=0;k<4;k++)ui_knob_turn(k,12);
    ui_capture("sound-fx");
    ui_select(1);
    CHECK(cgu.sound_page==CGU_PRESETS && fxpu.mode==FXP_BROWSE);
    ui_knob_turn(0,1);
    const fxp_preset_t *preset=fxp_pick();
    cf_fx_t previous=cs.fx;
    ui_button(B_EDIT);
    CHECK(cu.scr==SCR_FX && fxpu.mode==FXP_EDIT && cfx_equal(&cs.fx,&previous));
    cf_ui_draw();
    cu.now+=CGU_RACK_SCROLL_MS;
    ui_capture("rakt-effects-editor");
    ui_button(B_HOME);
    CHECK(cgu.tab==CGU_SOUND && cgs.zoom && cgu.sound_page==CGU_PRESETS);
    CHECK(fxpu.mode==FXP_BROWSE && fxp_pick()==preset);
    ui_button(B_SCL);
    CHECK(cgs.zoom && preset && cfx_equal(&cs.fx,&preset->fx));
    ui_capture("sound-fx-presets");
    ui_select(1);
    CHECK(cgu.sound_page==CGU_TONE);
    unsigned patch=cf_snd_pos(CGS_BASS), layer=cf_snd_pos(CGS_LAYER);
    uint32_t seed=cgs.seed[CGS_BASS];
    ui_button(B_EDIT);
    CHECK(cf_snd_pos(CGS_BASS)!=patch && cf_snd_pos(CGS_LAYER)==layer && cgs.seed[CGS_BASS]==seed);
    ui_button(B_SCL);
    ui_select(1);
    CHECK(cgu.tab==CGU_RECORD && cgs.focus==CGS_BASS);
    cu.now+=CGU_RACK_SCROLL_MS;
    ui_capture("record");
    CHECK(cf_stage_last.steps[CGS_DRUMS][0] && cf_stage_last.steps[CGS_BASS][0]);
    ui_button(B_SCL);
    cu.now+=180;
    ui_capture("record-bass");
    ui_select(1);
    CHECK(cgu.tab==CGU_HELP && cgs.focus==CGS_BASS);
    cu.now+=CGU_RACK_SCROLL_MS;
    ui_capture("help");
    ui_algo(1);
    CHECK(cgu.topic==CGU_HARMONY && cgs.focus==CGS_BASS);
    ui_button(B_SCL);
    cu.now+=180;
    ui_capture("help-harmony");
    CHECK(cf_stage_last.learn.automatic==(ch.mode==CF_H_AUTO));
    CHECK(cpa.q2==q2 && cf_grid.gen==clock);
    unsigned mode=ch.mode;
    ui_knob_turn(0,mode==CF_H_AUTO?-1:1);
    CHECK(ch.mode!=mode);
    ui_knob_turn(0,mode==CF_H_AUTO?1:-1);
    ui_note_key_aged(7,1,0);
    blocks(2);
    CHECK(cf_learn_focus==7);
    ui_capture("help-harmony");
    ui_note_key_aged(7,0,0);
    blocks(2);
    ui_button(B_HOME);
    CHECK(cgu.tab==CGU_HELP && !cgs.zoom);
    ui_button(B_HOME);
    CHECK(cgu.tab==CGU_STUDIO && !cgs.zoom && cgs.focus==CGS_BASS);
    cf_motion=0;
    ui_button(B_SCL);
    CHECK(cgu_zoom()==100);
    int octave=cs.chord.octave;
    ui_button(B_OCTUP);
    CHECK(cs.chord.octave==octave+1);
    ui_button(B_REC);
    cf_rec_poll();
    CHECK(cu.scr==SCR_HOME && cgu.tab==CGU_RECORD && sqk.on);
    blocks(2);
    ui_capture("record-manual");
    CHECK(cf_stage_last.rec);
    sq_home_cancel();
    blocks(2);
    cpa_stop();
    ui_button(B_GLO);
    ui_button(B_SAVE);
    CHECK(cu.scr==SCR_GLO && !cgu_glow.active);
    ui_button(B_HOME);
    CHECK(cgs_home());
    ui_button(B_HOME);
    ui_button(B_HOME);
    CHECK(cgs_home() && cgu.tab==CGU_STUDIO && !cgs.zoom);
    ui_select(-1);
    cgu.topic=CGU_HARMONY;
    ui_button(B_SCL);
    ui_button(B_EDIT);
    CHECK(cu.scr==SCR_LAB);
    ui_button(B_HOME);
    CHECK(cgs_home());
}

static void evolve_memory_studio(void)
{
    const unsigned part=CGS_LAYER, bit=1u<<CGS_LAYER;
    studio_start(497);
    blocks(2);
    cgs_clip_t original=cgs.active[part];
    uint32_t seed=cgs.develop_seed[part];
    cgs_request(part,CGS_DEVELOP);
    CHECK(cgs.pending_auto==bit && cgs.pending==bit && cgs.develop_seed[part]==seed);
    ticks(CGS_TICKS);
    cgs_clip_t evolved=cgs.active[part];
    CHECK(!cgs_same_clip(&original,&evolved) && evolved.edits.count==1);
    CHECK(!cgs.pending_auto && cgs.develop_seed[part]!=seed);
    cgs_energy_turn(1);
    ticks(CGS_TICKS);
    cgs_energy_turn(-1);
    ticks(CGS_TICKS);
    CHECK(cgs_same_clip(&cgs.active[part],&evolved) && cgs.active[part].edits.count==1);

    cgs_clip_t shaped, reference=original;
    unsigned optional=0;
    for(unsigned step=0;step<CGS_STEPS;step++)
        if(original.event[1][step])optional=step;
    CHECK(optional>0);
    reference.edits.item[0]=(cgs_edit_t){CGS_EDIT_DROP,(uint8_t)optional,0};
    reference.edits.count=1;
    cgs.energy=0;
    cgs_reshape(&shaped,part,&original,0);
    CHECK(original.event[1][optional] && !shaped.event[1][optional]);
    cgs_reshape(&shaped,part,&reference,0);
    CHECK(shaped.edits.count==1 && !shaped.event[1][optional]);
    cgs.energy=50;
    cgs_reshape(&shaped,part,&reference,0);
    CHECK(shaped.edits.count==1 && !shaped.event[1][optional]);

    for(unsigned i=0;i<12;i++){
        cgs_request(part,CGS_DEVELOP);
        ticks(CGS_TICKS);
        CHECK(cgs.active[part].edits.count<=CGS_MAX_EDITS);
    }
    CHECK(cgs.active[part].edits.count==CGS_MAX_EDITS);
    evolved=cgs.active[part];
    cgs_energy_turn(1);
    ticks(CGS_TICKS);
    cgs_energy_turn(-1);
    ticks(CGS_TICKS);
    CHECK(cgs_same_clip(&cgs.active[part],&evolved));

    /* A queued replacement has its own phrase. Turning Energy before its
     * downbeat must not publish that phrase onto the sounding take early. */
    CHECK(cgs_new_take_for(0,0) && cpa_setup.flags);
    cgs_energy_turn(10);
    CHECK(cgs_same_clip(&cgs.active[part],&evolved) && !(cgs.pending&bit));
    ticks(CGS_BAR-cpa.t);
    CHECK(!cpa_setup.flags && !cgs.active[part].edits.count);
    seed=cgs.develop_seed[part];
    cgs_request(part,CGS_DEVELOP);
    CHECK(cgs.pending_auto==bit);
    cgs_request(part,CGS_REROLL);
    CHECK(!cgs.pending_auto && cgs.pending==bit);
    ticks(CGS_TICKS);
    CHECK(!cgs.active[part].edits.count && cgs.develop_seed[part]==seed);
    cpa_stop();
}

static void evolve_controls_studio(void)
{
    const unsigned part=CGS_LAYER, bit=1u<<CGS_LAYER, drum_bit=1u<<CGS_DRUMS;
    studio_start(497);
    blocks(2);
    cgs_clip_t original=cgs.active[part], drums=cgs.active[CGS_DRUMS];
    uint32_t seed=cgs.develop_seed[part];

    cgs_request(part,CGS_DEVELOP);
    CHECK(cgs.pending_auto==bit);
    cgs_request(CGS_DRUMS,CGS_REROLL);
    CHECK(cgs.pending==(bit|drum_bit));
    cgs_knob_turn(3,-50,0);
    CHECK(!cgs.evolution && cgs.pending==drum_bit && !cgs.pending_auto);
    ticks(CGS_TICKS);
    CHECK(cgs_same_clip(&cgs.active[part],&original) && cgs.develop_seed[part]==seed);
    CHECK(!cgs_same_clip(&cgs.active[CGS_DRUMS],&drums));
    cgs_request(part,CGS_DEVELOP);
    CHECK(!cgs.pending);
    cgs_evolve_turn(50);

    cgs.focus=part;
    ui_button(B_SCL);
    ui_select(2);
    CHECK(cgs.zoom && cgu.part_page==CGU_PART_EVOLVE);
    cf_knob_t knob={0};
    cgu_knob_info(0,&knob);
    CHECK(!strcmp(knob.name,"HOLD") && !strcmp(knob.val,"Free"));
    cgs_request(part,CGS_DEVELOP);
    ui_knob_turn(0,1);
    cgu_knob_info(0,&knob);
    CHECK(cgs.hold==bit && !strcmp(knob.val,"Held") && !cgs.pending_auto);
    cgs_request(part,CGS_DEVELOP);
    ticks(CGS_TICKS);
    CHECK(cgs_same_clip(&cgs.active[part],&original) && cgs.develop_seed[part]==seed);

    ui_knob_turn(0,-1);
    ticks(CGS_TICKS);
    CHECK(!cgs.hold);
    cgs_request(part,CGS_DEVELOP);
    ticks(CGS_TICKS);
    cgs_clip_t evolved=cgs.active[part];
    CHECK(evolved.edits.count==1 && !cgs_same_clip(&evolved,&original));
    ui_knob_turn(0,1);
    cgs_energy_turn(-50);
    ticks(CGS_TICKS);
    CHECK(cgs.hold==bit && cgs_same_clip(&cgs.active[part],&evolved));
    cu.now+=200;
    ui_capture("studio-evolve-held");
    CHECK(cf_stage_last.held_parts==bit && cf_stage_last.shaping==CGU_PART_EVOLVE);
    CHECK(cf_stage_last.phrase_steps==CGS_STEPS && cf_stage_last.beat);
    for(unsigned bar=0;bar<2;bar++){
        uint16_t expected=0;
        for(unsigned step=0;step<CGS_STEPS;step++)
            if(evolved.event[bar][step])expected|=(uint16_t)(1u<<step);
        CHECK(cf_stage_last.steps[part][bar]==expected);
    }
    for(unsigned k=1;k<4;k++){
        CHECK(!cf_stage_last.label[k][0]);
        ui_knob_turn((int)k,1);
    }
    CHECK(cgs.hold==bit && cgs_same_clip(&cgs.active[part],&evolved));

    ui_button(B_PLAY);
    blocks(2);
    CHECK(!cgs_active() && cgs.hold==bit);
    ui_button(B_PLAY);
    blocks(2);
    CHECK(cgs_active() && cgs.hold==bit && cgs_same_clip(&cgs.active[part],&evolved));
    cgs_clip_t at_current_energy;
    cgs_reshape(&at_current_energy,part,&evolved,0);
    ui_knob_turn(0,-1);
    ticks(CGS_TICKS);
    CHECK(!cgs.hold && cgs_same_clip(&cgs.active[part],&at_current_energy));
    ui_knob_turn(0,1);

    ui_select(1);
    CHECK(cgu.part_page==CGU_PART);
    ui_knob_turn(0,1);
    ticks(CGS_TICKS);
    CHECK(cgs.style[part]==1 && cgs.hold==bit && !cgs.active[part].edits.count);
    ui_button(B_EDIT);
    CHECK((cgs.pending&bit) && !(cgs.pending_auto&bit));
    ticks(CGS_TICKS);
    CHECK(cgs.hold==bit && !cgs.active[part].edits.count);

    cgs_request(CGS_CHORDS,CGS_DEVELOP);
    CHECK(cgs.pending_auto==(1u<<CGS_CHORDS));
    uint32_t old_revision=cgs.take_revision;
    CHECK(cgs_new_take(0) && cpa_setup.flags);
    CHECK(!cgs.pending_auto && cgs.hold==bit);
    ticks(CGS_BAR-cpa.t);
    CHECK(!cgs.hold && !cgs.active[part].edits.count);
    cgs_develop_part(part,old_revision);
    CHECK(!cgs.pending_auto);

    cgs_request(part,CGS_DEVELOP);
    CHECK(cgs.pending_auto==bit);
    cgs_harmony_turn(3,1);
    CHECK(cpa_setup.flags && !cgs.pending_auto);
    ticks(CGS_BAR-cpa.t);
    CHECK(cpa.plan.beats==6 && !cgs.active[part].edits.count);
    cgs_request(part,CGS_DEVELOP);
    CHECK(cgs.pending_auto==bit);
    cpa_realtime(0xFCu);
    CHECK(!cgs.pending_auto && !cgs.pending && cpa.paused);
    cpa_realtime(0xFBu);
    CHECK(!cpa.paused && !cgs.active[part].edits.count);
    cgs_request(part,CGS_DEVELOP);
    CHECK(cgs.pending_auto==bit);
    cpa_stop();
    CHECK(!cgs.pending_auto && !cgs.pending && !cgs.active[part].edits.count);
    cgu_home();
}

static void evolve_meter_studio(void)
{
    for(unsigned meter=3;meter<=6;meter+=3){
        unsigned steps=cgs_steps(meter);
        CHECK(steps==12);
        cgs_clip_t layer={0};
        cgs_edit_t layer_edit={0};
        uint32_t layer_seed=497;
        CHECK(!cgs_make_edit(&layer,CGS_LAYER,steps,0,&layer_seed,&layer_edit));
        layer.event[1][7]=CGS_THIRD+(3u<<3);
        CHECK(cgs_make_edit(&layer,CGS_LAYER,steps,0,&layer_seed,&layer_edit));
        CHECK(layer_edit.step>7 && layer_edit.step<steps);
        cgs_apply_edit(&layer,CGS_LAYER,steps,layer_edit);
        CHECK(layer.event[1][7]==CGS_THIRD+(3u<<3));
        for(uint32_t seed=1;seed<=32;seed++){
            cgs_clip_t clip={0};
            clip.event[1][steps-1]=CGS_FIFTH+(3u<<3);
            uint32_t random=seed;
            cgs_edit_t edit={0};
            CHECK(cgs_bass_edit(&clip,steps,0,&random,&edit));
            CHECK(edit.step==steps-1 && edit.kind!=CGS_EDIT_BASS_LATER);
            cgs_apply_edit(&clip,CGS_BASS,steps,edit);
            CHECK(clip.event[1][steps-1] && !clip.event[1][steps]);
            CHECK(!cgs_bass_edit(&clip,steps,1u<<(steps-1),&random,&edit));
        }
    }
}

static void chord_gaps_studio(void)
{
    studio_start_profile(497,studio_genre_index("indie"));
    blocks(2);
    cgs_clip_t chords={0}, layer={0};
    for(unsigned b=0;b<2;b++){
        chords.event[b][0]=8;
        chords.event[b][3]=5;
        layer.event[b][1]=CGS_THIRD+(9u<<3);
        layer.feel[b][1]=112;
    }
    unsigned feel=0;
    cgs_prepare_response_timing(&chords,CGS_CHORDS);
    CHECK(!chords.response_timing[0][0] && !chords.response_timing[0][1] && !chords.response_timing[0][3]);
    CHECK(!cgs_gap_reply(&layer,chords.response_timing[0],0,1,&feel));
    unsigned reply=cgs_gap_reply(&layer,chords.response_timing[0],0,2,&feel);
    CHECK(reply==(CGS_THIRD+(3u<<3)) && (feel&7u)==CGS_REPLY_CLEARANCE);
    CHECK((feel&~7u)==112u);
    CHECK(!cgs_gap_reply(&layer,chords.response_timing[0],0,4,&feel));

    chords.event[0][0]=4;
    cgs_prepare_response_timing(&chords,CGS_CHORDS);
    CHECK(cgs_gap_reply(&layer,chords.response_timing[0],0,1,&feel));
    CHECK(!cgs_gap_reply(&layer,chords.response_timing[0],0,2,&feel));

    for(unsigned meter=3;meter<=6;meter+=3){
        cgs.meter=(uint8_t)meter;
        cgs_prepare_response_timing(&chords,CGS_CHORDS);
        CHECK(chords.response_timing[0][1] && !chords.response_timing[0][3]);
        for(unsigned b=0;b<2;b++)for(unsigned step=12;step<CGS_STEPS;step++)
            CHECK(!chords.response_timing[b][step]);
    }
    cgs.meter=4;

    cgs.focus=CGS_LAYER;
    ui_button(B_SCL);
    CHECK(!cgs.active[CGS_LAYER].responds);
    ui_knob_turn(0,1);
    CHECK(cgs.next[CGS_LAYER].responds && !cgs.active[CGS_LAYER].responds);
    ticks(CGS_TICKS);
    CHECK(cgs.active[CGS_LAYER].responds && cgs_layer_gaps(0)==cgs.active[CGS_CHORDS].response_timing[0]);
    ui_select(2);
    cu.now+=200;
    ui_capture("studio-chord-gaps");
    CHECK(cf_stage_last.layer_reply && cf_stage_last.steps[CGS_LAYER][0]);

    cgs.muted|=1u<<CGS_CHORDS;
    CHECK(cgs_layer_gaps(0)==cgs.active[CGS_CHORDS].response_timing[0]);
    cgs.muted=0;
    cgs_hold_turn(CGS_LAYER,1);
    CHECK(cgs.held_reply && !memcmp(cgs.held_gaps,cgs.active[CGS_CHORDS].response_timing,sizeof cgs.held_gaps));
    cgs.focus=CGS_CHORDS;
    cgu_arp_turn(CGU_ARP_BAND);
    CHECK(cgs_layer_gaps(0)==cgs.held_gaps[0]);
    cgs_hold_turn(CGS_LAYER,-1);
    CHECK(!cgs_layer_gaps(0));
    cgu_arp_turn(-CGU_ARP_BAND);
    CHECK(cgs_layer_gaps(0)==cgs.active[CGS_CHORDS].response_timing[0]);

    cgs_knob_turn(0,1,1); /* Ring */
    CHECK(cgs_layer_gaps(0));
    ticks(CGS_TICKS);
    CHECK(!cgs_layer_gaps(0));
    cgs_knob_turn(0,-1,1); /* Chop */
    ticks(CGS_TICKS);
    CHECK(cgs_layer_gaps(0));
    cpa_stop();
    cgu_home();
}

static void kick_accents_studio(void)
{
    studio_start_profile(497,studio_genre_index("new-wave"));
    blocks(2);
    cgs.focus=CGS_BASS;
    ui_button(B_SCL);
    CHECK(!cgs.active[CGS_BASS].responds);
    ui_knob_turn(0,1);
    CHECK(cgs.next[CGS_BASS].responds && !cgs.active[CGS_BASS].responds);
    ticks(CGS_TICKS);
    CHECK(cgs.active[CGS_BASS].responds);
    ui_select(2);
    cu.now+=200;
    ui_capture("studio-kick-accents");
    CHECK(cf_stage_last.bass_reply && (cf_stage_last.steps[CGS_BASS][1]&(1u<<8)));
    cgs.focus=CGS_DRUMS;
    cgs_knob_turn(0,1,1); /* Open hats shifts the second-bar kick. */
    CHECK(cgs_bass_kicks(1,0)[8] && !cgs_bass_kicks(1,0)[10]);
    ticks(CGS_TICKS);
    CHECK(cgs_bass_kicks(0,0)[8] && !cgs_bass_kicks(1,0)[8] && cgs_bass_kicks(1,0)[10]);
    cf_stage_model_t phrase={0};
    cgu_phrase_model(&phrase,1u<<CGS_BASS,1);
    CHECK(phrase.bass_reply && (phrase.steps[CGS_BASS][0]&(1u<<8)) &&
          !(phrase.steps[CGS_BASS][1]&(1u<<8)) && (phrase.steps[CGS_BASS][1]&(1u<<10)));

    cgs_clip_t drums={0}, bass={0};
    unsigned weak=CGS_THIRD+(6u<<3), pickup=CGS_PICKUP+(3u<<3), feel=0;
    uint8_t pulled=0;
    drums.event[0][8]=drums.event[0][11]=drums.event[0][14]=CGS_KICK;
    drums.feel[0][8]=3;
    drums.feel[0][11]=6;
    bass.event[0][0]=CGS_ANCHOR+(6u<<3);
    bass.event[0][10]=(uint8_t)weak;
    bass.feel[0][10]=116;
    bass.event[0][15]=(uint8_t)pickup;
    bass.responds=CGS_BASS_REPLY_LOCK;
    cgs_prepare_response_timing(&drums,CGS_DRUMS);
    const uint8_t *kicks=drums.response_timing[0], *notes=bass.event[0], *accents=bass.feel[0];
    CHECK(kicks[8]==4 && !kicks[11] && kicks[14]==1);
    CHECK(cgs_kick_bass(CGS_BASS_REPLY_LOCK,notes,accents,kicks,0,16,&feel,&pulled)==notes[0] && !pulled);
    CHECK(cgs_kick_bass(CGS_BASS_REPLY_LOCK,notes,accents,kicks,8,16,&feel,&pulled)==weak && pulled==11);
    CHECK(feel==147);
    CHECK(!cgs_kick_bass(CGS_BASS_REPLY_LOCK,notes,accents,kicks,10,16,&feel,&pulled));
    CHECK(!cgs_kick_bass(CGS_BASS_REPLY_LOCK,notes,accents,kicks,14,16,&feel,&pulled));
    CHECK(cgs_kick_bass(CGS_BASS_REPLY_LOCK,notes,accents,kicks,15,16,&feel,&pulled)==pickup);
    pulled=0;
    CHECK(!cgs_kick_bass(CGS_BASS_REPLY_LOCK,notes,accents,kicks,8,10,&feel,&pulled) && !pulled);
    drums.event[0][9]=CGS_KICK;
    cgs_prepare_response_timing(&drums,CGS_DRUMS);
    CHECK(!cgs_kick_bass(CGS_BASS_REPLY_LOCK,notes,accents,kicks,8,16,&feel,&pulled) && !pulled);
    drums.event[0][9]=0;
    cgs_prepare_response_timing(&drums,CGS_DRUMS);

    /* Queue new kicks just before step 8. Bass must use the new map
     * before Drums commits it, and sound at the same delay as that kick. */
    cgs.evolution=0;
    cgs.active[CGS_BASS]=bass;
    memset(&cgs.active[CGS_DRUMS],0,sizeof drums);
    cgs.bar=cgs.origin_bar=0;
    cpa.t=8u*CGS_TICKS-1u;
    cpa.elapsed=cpa.t;
    cgs.next[CGS_DRUMS]=drums;
    cgs.pending=1u<<CGS_DRUMS;
    cgs_release(CGS_BASS);
    ticks(1);
    CHECK(cgs.bass_pull==11 && cgs.bass_event==weak && cgs.bass_feel==147 && !sqp.nb);
    ticks(2);
    CHECK(!sqp.nb);
    ticks(1);
    CHECK(sqp.nb && (sqp.doff&1u));

    cgs_hold_turn(CGS_BASS,1);
    CHECK(!memcmp(cgs.held_kicks,drums.response_timing,sizeof cgs.held_kicks));
    memset(&cgs.next[CGS_DRUMS],0,sizeof drums);
    cgs.pending=1u<<CGS_DRUMS;
    ticks(CGS_TICKS-3);
    CHECK(cgs_bass_kicks(0,0)[8]==4 && !cgs.active[CGS_DRUMS].response_timing[0][8]);
    cgs_hold_turn(CGS_BASS,-1);
    CHECK(!cgs_bass_kicks(0,0)[8]);
    ticks(CGS_TICKS);
    CHECK(!cgs.bass_event && cgs.bass_pull==11);

    cpa_stop();
    cgu_home();
}

static void late_pocket_studio(void)
{
    static const unsigned meters[]={3,4,6};
    for(unsigned i=0;i<NELEM(meters);i++){
        unsigned meter=meters[i];
        studio_start_profile(497,studio_genre_index("lo-fi"));
        blocks(2);
        cgs.evolution=0;
        if(meter!=4u){
            cgs.meter=(uint8_t)meter;
            CHECK(cgs_new_take_for(0,0));
            ticks(CGS_BAR-cpa.t);
        }
        unsigned step_ticks=cgs_step_ticks(meter), first=meter==4u?7u:5u, second=meter==4u?11u:8u;
        const cgs_clip_t *bass=&cgs.active[CGS_BASS];
        const uint8_t *kicks=cgs_bass_kicks(0,0);
        CHECK(cpa.plan.beats==meter && bass->responds==CGS_BASS_REPLY_LATE);
        CHECK(kicks[0]==1 && kicks[first]==5 && kicks[second]==5);
        CHECK((bass->event[0][first]&7u)==CGS_THIRD && (bass->event[0][second]&7u)==CGS_FIFTH);
        CHECK((bass->event[1][cgs_steps(meter)-1u]&7u)==CGS_STEP_IN);
        cf_stage_model_t phrase={0};
        cgu_phrase_model(&phrase,1u<<CGS_BASS,1);
        CHECK(phrase.bass_reply==CGS_BASS_REPLY_LATE &&
              phrase.bass_delays[0]==((1u<<first)|(1u<<second)) &&
              phrase.bass_delays[1]==phrase.bass_delays[0]);
        if(meter==4u){
            cgs.focus=CGS_BASS;
            ui_button(B_SCL);
            ui_select(2);
            cu.now+=200;
            ui_capture("studio-late-pocket");
            CHECK(cf_stage_last.bass_reply==CGS_BASS_REPLY_LATE);
        }

        ticks(first*step_ticks+4u-cpa.t);
        CHECK((sqp.doff&1u) && !sqp.nb && (cgs.bass_feel&7u)==7u);
        CHECK((cgs.bass_feel&~7u)==(bass->feel[0][first]&~7u));
        ticks(2);
        CHECK(!sqp.nb);
        ticks(1);
        CHECK(sqp.nb && !(sqp.doff&1u) && !cgs.bass_pull && !cgs.bass_attack);

        cgs_hold_turn(CGS_BASS,1);
        cgs.next[CGS_DRUMS]=cgs.active[CGS_DRUMS];
        cgs.next[CGS_DRUMS].feel[0][second]&=(uint8_t)~7u;
        cgs_prepare_response_timing(&cgs.next[CGS_DRUMS],CGS_DRUMS);
        cgs.pending=1u<<CGS_DRUMS;
        ticks(second*step_ticks-cpa.t);
        CHECK((sqp.doff&1u) && !sqp.nb && (cgs.bass_feel&7u)==7u);
        CHECK(cgs_bass_kicks(0,0)[second]==5 && cgs.active[CGS_DRUMS].response_timing[0][second]==1);
        cgs_hold_turn(CGS_BASS,-1);
        CHECK(cgs_bass_kicks(0,0)[second]==1);

        cgs.focus=CGS_BASS;
        cgs_knob_turn(0,1,1); /* Walk */
        CHECK(bass->responds==CGS_BASS_REPLY_LATE && !cgs.next[CGS_BASS].responds);
        ticks(step_ticks);
        CHECK(!bass->responds);
        cpa_stop();
        cgu_home();
    }
}

static void genre_replies_studio(void)
{
    static const unsigned meters[]={3,4,6};
    for(unsigned i=0;i<NELEM(meters);i++){
        unsigned meter=meters[i];
        studio_start_profile(497,studio_genre_index("lo-fi"));
        blocks(2);
        cgs.evolution=0;
        if(meter!=4u){
            cgs.meter=(uint8_t)meter;
            CHECK(cgs_new_take_for(0,0));
            ticks(CGS_BAR-cpa.t);
        }
        unsigned backbeat=cgs_layer_backbeat(0,0), steps=cgs_steps(meter);
        CHECK(backbeat==(meter==6u?7u:9u));
        cf_stage_model_t phrase={0};
        cgu_phrase_model(&phrase,1u<<CGS_LAYER,1);
        CHECK(phrase.layer_reply==CGS_LAYER_REPLY_SNARE);
        unsigned notes=phrase.steps[CGS_LAYER][0];
        CHECK(notes && !(notes&(notes-1u)) && !(notes&((1u<<backbeat)-1u)));
        CHECK(notes<(1u<<(backbeat+3u)));
        unsigned step=0;
        while(step<steps && !(notes&(1u<<step)))step++;
        if(step<steps){
            unsigned onset=step*cgs_step_ticks(meter);
            ticks(onset-1u-cpa.t);
            CHECK(!sqp.ln);
            ticks(CGS_MAX_ONSET_DELAY+1u);
            CHECK(sqp.ln);
        }
        cgs_hold_turn(CGS_LAYER,1);
        cgs.next[CGS_DRUMS]=cgs.active[CGS_DRUMS];
        cgs.next[CGS_DRUMS].backbeat[0]=1;
        cgs.pending=1u<<CGS_DRUMS;
        ticks(cgs_step_ticks(meter));
        CHECK(cgs_layer_backbeat(0,0)==backbeat);
        cgs_hold_turn(CGS_LAYER,-1);
        CHECK(cgs_layer_backbeat(0,0)==1);
        cgs.focus=CGS_LAYER;
        cgs_knob_turn(0,1,1);
        ticks(cgs_step_ticks(meter));
        CHECK(cgs.active[CGS_LAYER].responds==CGS_LAYER_REPLY_NONE);
        cpa_stop();
        cgu_home();
    }

    studio_start_profile(497,studio_genre_index("dream-pop"));
    blocks(2);
    cgs.focus=CGS_LAYER;
    cgs_knob_turn(0,1,1); /* Sparse motif */
    ticks(2u*CGS_TICKS-1u-cpa.t);
    CHECK(cgs.active[CGS_LAYER].responds==CGS_LAYER_REPLY_DREAM && !cgs_layer_gaps(0) && !sqp.ln);
    ticks(1);
    CHECK(sqp.ln);
    cgs.focus=CGS_CHORDS;
    cgs_knob_turn(0,1,1); /* Soft pulse */
    ticks(CGS_TICKS);
    CHECK(cgs_layer_gaps(0));
    cf_stage_model_t phrase={0};
    cgu_phrase_model(&phrase,1u<<CGS_LAYER,1);
    CHECK(phrase.layer_reply==CGS_LAYER_REPLY_GAPS);
    cpa_stop();
    cgu_home();

    studio_start_profile(497,studio_genre_index("ambient"));
    blocks(2);
    cgs.evolution=0;
    CHECK(cgs.active[CGS_LAYER].responds==CGS_LAYER_REPLY_HARMONY && cgs.harmony_changed);
    sq_snd_poll();
    CHECK(!strcmp(cf_snd[CGS_CHORDS].name,"STILL AIR") && !strcmp(cf_snd[CGS_LAYER].name,"SOFT HALO"));
    CHECK(sqp.nb==1 && !cgs.gate[CGS_BASS]);
    unsigned pedal=sqp.bass[0];
    ticks(SQ_PPQN-1u-cpa.t);
    CHECK(!sqp.ln);
    ticks(1);
    CHECK(sqp.ln);
    ticks(CGS_BAR-cpa.t);
    unsigned hits=sqp.doff;
    for(unsigned t=1;t<CGS_BAR;t++){cpa_tick1();mo_r=mo_w;hits|=sqp.doff;}
    CHECK((hits&(1u<<PF_R)) && sqp.nb==1 && sqp.bass[0]==pedal && !cgs.gate[CGS_BASS]);
    const cpa_slot_t *slot=&cpa.plan.slot[cpa.slot];
    unsigned feel=0;
    cgs.harmony_changed=0;
    cpa.elapsed=SQ_PPQN;
    CHECK(!cgs_harmony_reply(&cgs.active[CGS_LAYER],0,CGS_TICKS,0,SQ_PPQN,slot,&feel));
    cpa_stop();
    cgu_home();

    studio_start_profile(497,studio_genre_index("pop-punk"));
    blocks(2);
    sq_snd_poll();
    CHECK(!strcmp(cf_snd[CGS_LAYER].name,"PICK CHIME"));
    for(unsigned b=0;b<2;b++){
        unsigned notes=0;
        const cgs_clip_t *layer=&cgs.active[CGS_LAYER];
        for(unsigned step=0;step<CGS_STEPS;step++){
            unsigned event=layer->event[b][step];
            if(!event)continue;
            notes++;
            CHECK((event>>3)<=6u && ((event&7u)==CGS_ANCHOR || (event&7u)==CGS_FIFTH));
            CHECK(!(CGS_PUNK_KICK[layer->voice][b]&(1u<<step)));
        }
        CHECK(notes==1);
    }
    cgs.evolution=0;
    ticks(7u*CGS_BAR-cpa.t);
    CHECK(cgs.punk_fill_step>=CGS_STEPS-3u && cgs.punk_fill_step<=CGS_STEPS);
    unsigned fill=cgs.punk_fill_step;
    if(fill<CGS_STEPS){
        ticks(fill*CGS_TICKS+(cgs.active[CGS_DRUMS].feel[1][fill]&7u));
        CHECK(sqp.doff&(1u<<PF_MT));
    }
    cgs_clip_t drums={.voice=0};
    memset(cgs.active[CGS_BASS].event[1],0,sizeof cgs.active[CGS_BASS].event[1]);
    drums.event[1][11]=drums.event[1][13]=CGS_KICK;
    cgs.active[CGS_BASS].event[1][11]=CGS_THIRD;
    cgs.active[CGS_BASS].event[1][13]=CGS_PICKUP;
    CHECK(cgs_punk_turnaround(&drums,1,CGS_STEPS)==CGS_STEPS-3u);
    cgs.active[CGS_BASS].event[1][13]=CGS_FIFTH;
    CHECK(cgs_punk_turnaround(&drums,1,CGS_STEPS)==14u);
    cpa_stop();
    cgu_home();
}

static void harmony_studio(void)
{
    uint32_t seed=0x2353ad17u;
    uint16_t riffs=0;
    for(unsigned i=0;i<128;i++)riffs|=(uint16_t)(1u<<(cgs_riff(i&1u,&seed)-CGS_RIFFS));
    CHECK(riffs==(1u<<(sizeof CGS_RIFFS/sizeof CGS_RIFFS[0]))-1u);
    cpa_plan_t plan;
    cgs_pitches_t pitch={0};
    cf_chord_cfg_t cfg=cs.chord;
    cfg.tonic=4;
    cfg.scale=CF_SC_MAJOR;
    int index=cge_find(133);
    CHECK(index>=0 && cpa_prepare_full(&plan,index,cfg,0,CPA_BOTH,0,0));
    cgs_voice_plan(&plan,&pitch);
    CHECK(plan.n==3 && cpa_valid(&plan));
    const cf_source_chord_t *source=cf_progression_source(cf_progression_get(index));
    for(unsigned i=0;i<plan.n;i++){
        const cpa_slot_t *s=&plan.slot[i];
        const uint8_t *notes=pitch.note[i];
        unsigned target=pitch.note[(i+1)%plan.n][0];
        CHECK(s->mask==source[i].mask && notes[0]%12==s->bass_pc);
        if(notes[0]!=target)
            CHECK(abs((int)notes[4]-(int)target)==1);
    }
    index=cge_find(79);
    CHECK(cpa_prepare_full(&plan,index,cfg,0,CPA_BOTH,0,0));
    cgs_voice_plan(&plan,&pitch);
    CHECK(plan.n==8 && cpa_valid(&plan));
}

static void arrangements_studio(void)
{
    studio_start(100);
    blocks(2);
    CHECK(cgs_active() && cpa.plan.idea==CF_P_COMPOSED);
    unsigned grid=cf_grid.gen, tonic=cpa.plan.tonic, scale=cpa.plan.scale;
    unsigned families=1u<<cgs.playing_feel;
    for(unsigned roll=0;roll<12;roll++){
        unsigned previous=cgs.playing_feel;
        cgs_clip_t drums=cgs.active[CGS_DRUMS];
        uint32_t now=cpa.q2;
        ui_button(B_EDIT);
        CHECK(cpa_setup.flags && cgs.queued_feel!=previous);
        ticks(CGS_BAR-cpa.t);
        CHECK(cpa.q2>now && cf_grid.gen==grid && cgs.playing_feel!=previous);
        CHECK(cpa.plan.tonic==tonic && cpa.plan.scale==scale);
        CHECK(memcmp(drums.event,cgs.active[CGS_DRUMS].event,sizeof drums.event));
        families|=1u<<cgs.playing_feel;
    }
    CHECK(families==15u);

    cgs.focus=CGS_BASS;
    ui_button(B_GLO);
    CHECK(cgu_glow.active);
    ui_capture("mixer");
    ui_button(B_SCL);
    blocks(1);
    CHECK(cgs.solo==(1u<<CGS_BASS) && cgs.mute_applied==(15u^(1u<<CGS_BASS)));
    midi_event(0x90,0,64,96);
    CHECK(cr_mnote[0][64] && trk_voice_of(&trk[0],64));
    midi_event(0x80,0,64,0);
    ui_note_key_aged(5,1,0); /* A#3 toggles the Drums part */
    ui_note_key_aged(5,0,0);
    ui_button(B_SCL);
    blocks(1);
    CHECK(!cgs.solo && cgs.mute_applied==(1u<<CGS_DRUMS));
    ui_button(B_GLO);
    CHECK(!cgu_glow.active);

    for(unsigned genre=1;genre<CGS_NGENRE;genre++){
        ui_presets(1);
        CHECK(cgs.genre==genre && cpa_setup.flags);
        ticks(CGS_BAR-cpa.t);
        CHECK(cgs.playing_genre==genre && cf_grid.gen==grid && cpa.plan.tonic==tonic);
        if(!CGS_PROFILES[genre].write_progressions)
            CHECK(cpa.plan.slot[0].ticks==CGS_PROFILES[genre].bank_duration_scale*CGS_BAR);
        for(unsigned p=0;p<CGS_DRUMS;p++)
            for(unsigned pick=0;pick<2;pick++)CHECK(cgs_profile_voice(p,pick)<VA_NPRESETS);
    }
    unsigned ambient=studio_genre_index("ambient");
    if(cgs.playing_genre!=ambient){
        ui_presets((int)ambient-(int)cgs.playing_genre);
        ticks(CGS_BAR-cpa.t);
    }
    CHECK(cgs.playing_genre==ambient);
    ui_button(B_SEQ);
    CHECK(cgu.band_panel==CGU_BAND_HARMONY && !cgs.zoom);
    ui_knob_turn(3,1);
    CHECK(cgs.meter==6 && cpa.plan.beats==4);
    ticks(CGS_BAR-cpa.t);
    CHECK(cpa.plan.beats==6 && cpa.plan.slot[0].ticks==12u*SQ_PPQN && cf_grid.gen==grid);
    ui_knob_turn(3,1);
    CHECK(cgs.meter==3 && cpa.plan.beats==6);
    ticks(6u*SQ_PPQN-cpa.t);
    CHECK(cpa.plan.beats==3 && cf_grid.gen==grid);
    ui_knob_turn(0,1);
    CHECK(cpa_edit.flags&CPA_NOTES);
    ticks(cpa.plan.slot[cpa.slot].ticks-cpa.elapsed);
    CHECK(cpa.plan.tonic==(tonic+1)%12 && cf_grid.gen==grid);
    ui_knob_turn(2,1);
    CHECK(cpa_edit.flags&CPA_NOTES);
    ticks(cpa.plan.slot[cpa.slot].ticks-cpa.elapsed);
    CHECK(cpa.plan.scale!=scale && cf_grid.gen==grid);
    ui_select(1);
    cgs.focus=CGS_CHORDS;
    ui_button(B_SCL);
    cf_pool_load(CGS_CHORDS,ENGI_VA,pool_pos_factory(ENGI_VA,cgs_profile_voice(CGS_CHORDS,2)));
    unsigned count=cgu_sound_count(CGS_CHORDS,0);
    unsigned rank=cgu_sound_rank(CGS_CHORDS,0,count,cgu_sound_current(CGS_CHORDS));
    for(unsigned i=0;i<2;i++){
        ui_button(B_EDIT);
        unsigned next=cgu_sound_rank(CGS_CHORDS,0,count,cgu_sound_current(CGS_CHORDS));
        CHECK(next<count && next!=rank);
        rank=next;
    }
    ui_button(B_HOME);
    cpa_stop();
}

static void amp_performance_studio(void)
{
    studio_start(102);
    blocks(2);
    unsigned grid=cf_grid.gen;
    CHECK(cgs.active[CGS_CHORDS].voice==2 && sqp.n);
    unsigned count=sqp.n;
    uint8_t notes[6];
    memcpy(notes,sqp.note,count);
    for(unsigned i=0;i<CGS_STEPS;i++){
        ticks(CGS_TICKS);
        CHECK(sqp.n==count && !memcmp(sqp.note,notes,count));
        voice_t *voice=trk_voice_of(&trk[CGS_CHORDS],notes[0]);
        CHECK(voice && voice->gate && !cgs.gate[CGS_CHORDS]);
    }

    ui_button(B_SCL);
    unsigned sustain=cf_env_val(CGS_CHORDS,2);
    cu.now=2000;
    ui_knob_turn(3,-1);
    CHECK(cf_env_val(CGS_CHORDS,2)==sustain-1);
    cu.now+=35;
    ui_knob_turn(3,-1);
    CHECK(cf_env_val(CGS_CHORDS,2)==sustain-6);
    cu.now+=200;
    ui_knob_turn(3,1);
    CHECK(cf_env_val(CGS_CHORDS,2)==sustain-5);
    ui_capture("amp-chords");

    ui_note_key_aged(7,1,0);
    blocks(2);
    CHECK(cfp.n && !sqp.n);
    unsigned held=cfp.note[0];
    ticks(2u*CGS_BAR);
    voice_t *voice=trk_voice_of(&trk[CGS_CHORDS],held);
    CHECK(cfp.n && voice && voice->gate && cf_grid.gen==grid);
    ui_note_key_aged(7,0,0);
    blocks(2);
    CHECK(!cfp.n);

    cgs.focus=CGS_BASS;
    uint32_t seed=cgs.seed[CGS_BASS], phrase=cgs.active[CGS_BASS].seed;
    ui_knob_turn(1,-4);
    ticks(CGS_TICKS);
    CHECK(cgs.seed[CGS_BASS]==seed && cgs.active[CGS_BASS].seed==phrase);
    cgs.focus=CGS_DRUMS;
    ui_presets(-1);
    blocks(1);
    cf_snapshot();
    CHECK(cf_sn.perf_pending==CFPERF_FILL && !cf_sn.perf_fill);
    ui_capture("amp-drums");
    ticks(CGS_BAR-cpa.t);
    cf_snapshot();
    CHECK(cf_sn.perf_fill && !cf_sn.perf_pending);
    ui_presets(1);
    blocks(1);
    ticks(CGS_BAR-cpa.t);
    cf_snapshot();
    CHECK(cf_sn.perf_dropped && !cf_sn.perf_fill);
    ui_presets(1);
    blocks(1);
    ticks(CGS_BAR-cpa.t);
    CHECK(!cf_moment.dropped && cf_grid.gen==grid);
    cpa_stop();
}

static void indie_form_studio(void)
{
    studio_start_profile(497,studio_genre_index("indie"));
    blocks(2);
    cgs.evolution=0;
    cgs.focus=CGS_LAYER;
    cgs_hold_turn(CGS_LAYER,1);
    cgu_arp_turn(CGU_ARP_BAND);
    ticks(3u*CGS_BAR-cpa.t);
    unsigned grid=cf_grid.gen, layer=1u<<CGS_LAYER;
    CHECK(cgs.bar==3 && !cgs.section && (sqp.aon&layer));
    cgs_clip_t held=cgs.active[CGS_LAYER];
    ticks(CGS_BAR);
    CHECK(cgs.bar==4 && cgs.section==(CGS_FORM_LAYER_REST|CGS_FORM_LIGHT_HATS));
    CHECK(!(sqp.aon&layer) && !sqp.ln && (cgs.hold&layer) && sqp.n && sqp.nb);
    cu.now+=200;
    ui_capture("studio-band-breather");
    CHECK(cf_stage_last.section==CGS_FORM_LAYER_REST);
    unsigned drums=sqp.doff;
    for(unsigned t=1;t<CGS_BAR;t++){
        cpa_tick1();
        mo_r=mo_w;
        drums|=sqp.doff;
    }
    CHECK(!(drums&((1u<<3)|(1u<<4))) && (drums&3u)==3u);
    ticks(1);
    CHECK(cgs.bar==5 && !cgs.section && (sqp.aon&layer));
    CHECK(!memcmp(&held,&cgs.active[CGS_LAYER],sizeof held));

    ticks(2u*CGS_BAR);
    CHECK(cgs.bar==7 && cf_moment.fill==CFPERF_FILL_AUTO && !cf_moment.pending);
    cgs.focus=CGS_DRUMS;
    cu.now+=200;
    ui_capture("studio-band-turnaround");
    CHECK(cf_stage_last.section==CGS_FORM_FILL);
    cgu_glow_open(1);
    unsigned key=0;
    while(key<CF_NKEYS && (cf_key_is_upper(key) || cgu_glow_place(key,0)!=CGU_GLO_FILL_BAR))key++;
    CHECK(cgu_glow_key(key,1));
    CHECK(cgu_glow_key(key,0));
    cgu_glow_open(0);
    blocks(1);
    CHECK(cf_moment.pending==CFPERF_FILL);
    ticks(CGS_BAR-cpa.t);
    CHECK(cgs.bar==8 && !cgs.section && cf_moment.fill==CFPERF_FILL_MANUAL);
    ticks(CGS_BAR);
    CHECK(!cf_moment.fill && cf_grid.gen==grid);
    ticks(5u*CGS_BAR);
    CHECK(cgs.bar==14);
    cf_perf_queue(CFPERF_DROP);
    ticks(CGS_BAR);
    CHECK(cf_moment.dropped && !cf_moment.fill);
    cf_perf_queue(CFPERF_BACK);
    ticks(CGS_BAR);
    CHECK(!cf_moment.dropped && !cf_moment.fill);
    ticks(6u*CGS_BAR);
    cf_perf_queue(CFPERF_FILL);
    ticks(CGS_BAR);
    CHECK(cgs.bar==23 && cf_moment.fill==CFPERF_FILL_MANUAL);

    CHECK(cgs_new_take_for(0,1));
    ticks(CGS_BAR);
    CHECK(cgs.origin_bar==cgs.bar && !cgs.section && !cf_moment.fill && cf_grid.gen==grid);
    cpa_stop();
    cgu_home();
}

static void play_and_mixer_studio(void)
{
    studio_start(102);
    blocks(3);
    unsigned grid=cf_grid.gen;
    ui_button(B_ARP);
    CHECK(cu.scr==SCR_HOME && cgu.band_panel==CGU_BAND_ARP && cgs.zoom && !cfa_k.n);
    ui_select(1);
    cu.now+=180;
    ui_capture("arp-timing");
    CHECK(cgu.arp_timing && !strcmp(cf_stage_last.label[0],"GATE"));
    ui_select(-1);
    CHECK(!cgu.arp_timing);
    ui_knob_turn(0,1);
    blocks(1);
    CHECK(cgu_arp_mode()==CGU_ARP_KEYS && cfa_k.on && !cfa_k.n);
    ui_note_key_aged(7,1,0);
    blocks(2);
    CHECK(cfa_k.n && cfa_k.count && !sqp.n);
    ui_note_key_aged(7,0,0);
    blocks(1);
    ui_knob_turn(0,1);
    blocks(1);
    ticks(CGS_TICKS);
    CHECK(cgu_arp_mode()==CGU_ARP_BAND && !cfa_k.on && cfa_sq[0].n && cfa_sq[0].count);
    ui_capture("arp-band");
    ui_knob_turn(0,-2);
    blocks(1);
    ticks(CGS_TICKS);
    CHECK(!cfa_sq[0].n && sqp.n);
    ui_algo(-1);
    blocks(1);
    CHECK(cgs.focus==CGS_LAYER && cfp_cp==CGS_LAYER);
    ui_note_key_aged(7,1,0);
    blocks(2);
    CHECK(cfp.n && !cfp.nb && trk_voice_of(&trk[CGS_LAYER],cfp.note[0]) && sqp.n);
    ui_note_key_aged(7,0,0);
    blocks(2);
    ui_button(B_HOME);
    CHECK(!cgs.zoom && cgu.band_panel==CGU_BAND_ARP);
    ui_button(B_HOME);
    ui_algo(-2);
    blocks(1);
    CHECK(cgs.focus==CGS_BASS && cfp_bo);
    ui_note_key_aged(7,1,0);
    blocks(2);
    CHECK(cfp.nb && !cfp.n && trk_voice_of(&trk[CGS_BASS],cfp.bass[0]) && sqp.n);
    ui_note_key_aged(7,0,0);
    blocks(2);
    ui_algo(1);
    blocks(1);
    CHECK(cgs.focus==CGS_DRUMS && cfp_cp==CGS_DRUMS);

    unsigned keys[8], n=0;
    for(unsigned i=0;i<CF_NKEYS && n<NELEM(keys);i++)
        if(cf_key_is_upper(i))keys[n++]=i;
    CHECK(n==NELEM(keys));
    if(n!=NELEM(keys))return;
    ui_note_key_aged((int)keys[0],1,0);
    blocks(1);
    CHECK(trk_voice_of(&trk[CGS_DRUMS],36) && !cfp.n);
    ui_note_key_aged((int)keys[0],0,0);

    CHECK(cf_grid.gen==grid);
    cpa_stop();
    cgu_home();
}

static unsigned studio_led(unsigned key)
{
    unsigned pin=cu_led_pos[14u+key], bit=1u<<(pin&7u), column=pin>>3;
    return (fm1_led[column]&bit)?4u:(fm1_led_dim2[column]&bit)?3u:
        (fm1_led_dim[column]&bit)?2u:(fm1_led_faint[column]&bit)?1u:0u;
}

static void glow_scan(unsigned elapsed,uint32_t buttons,uint32_t keys)
{
    emu_hal.buttons=buttons;
    emu_hal.keys=keys;
    emu_fw_tick(fm1_ms+elapsed);
    cu.now=fm1_ms;
    cf_ui_input();
}

static void glow_live_studio(void)
{
    studio_start(101);
    blocks(2);
    cgs.evolution=0;
    uint32_t glo=1u<<panel.btn[B_GLO], home=1u<<panel.btn[B_HOME];
    unsigned white[16],black[11],w=0,b=0;
    for(unsigned k=0;k<CF_NKEYS;k++){
        if(cf_key_is_upper(k))black[b++]=k;
        else white[w++]=k;
    }
    CHECK(w==16 && b==11);
    if(w!=16 || b!=11)return;
    uint32_t solo=1u<<white[1], fill=1u<<white[CGU_GLO_FILL], kick=1u<<black[6];

    cgu_tab_turn(1);
    cgu_set_zoom(1);
    cgu_sound_page(CGU_SENDS);
    glow_scan(30,0,1u<<white[7]);
    blocks(1);
    CHECK(cfp.n);
    glow_scan(30,glo,1u<<white[7]);
    CHECK(cgu_glow.active && cgu_glow.view==CGU_GLO_MAP);
    glow_scan(30,0,0);
    CHECK(cgu_glow.active);
    blocks(1);
    CHECK(!cfp.n); /* a note played before GLO still gets its normal note-off */
    ui_capture("glow-map");
    ui_algo(1);
    ui_capture("glow-state");
    ui_algo(1);
    CHECK(cgu_glow.view==CGU_GLO_MAP);
    unsigned bpm=cs.bpm, level=cgs.level[CGS_DRUMS];
    ui_select(1);
    ui_knob_turn(2,1);
    CHECK(cs.bpm==bpm+1 && cgs.level[CGS_DRUMS]==level);
    for(unsigned i=0;i<3;i++){
        glow_scan(475,0,1u<<white[CGU_GLO_TAP]);
        glow_scan(25,0,0);
    }
    CHECK(cs.bpm==120);

    glow_scan(30,0,1u<<black[0]);
    glow_scan(30,0,0);
    blocks(1);
    CHECK(cgs.muted==(1u<<CGS_CHORDS) && cgs.mute_applied==cgs.muted);
    glow_scan(30,0,kick);
    glow_scan(30,0,0);
    CHECK(cgs.drum_mutes==1);
    sqp_drum_offs();
    cgs_drums(CGS_KICK|CGS_SNARE,0,16,0);
    CHECK(sqp.doff==(1u<<PF_S));
    glow_scan(30,0,1u<<white[CGU_GLO_UNMUTE]);
    glow_scan(30,0,0);
    CHECK(!cgs.muted && !cgs.drum_mutes);

    glow_scan(30,0,solo|fill);
    CHECK(cgs.held_solo==(1u<<CGS_BASS) && cgs.fill_held && !cfp.n);
    cf_leds();
    CHECK(studio_led(white[1])==4u && !studio_led(white[15]));
    glow_scan(30,0,fill);
    blocks(1);
    CHECK(!cgs.held_solo && !cgs.mute_applied);
    sqp_drum_offs();
    cgs_drums(0,0,16,0);
    CHECK(sqp.doff&(1u<<PF_S)); /* held fill adds a hit to an empty drum step */
    ui_capture("glow-fill");
    glow_scan(30,glo,fill);
    CHECK(!cgu_glow.active && !cgs.fill_held && cgu_glow.keys==fill);
    CHECK(cgu.tab==CGU_SOUND && cgs.zoom && cgu.sound_page==CGU_SENDS);
    sqp_drum_offs();
    cgs_drums(0,0,16,0);
    CHECK(!sqp.doff);
    glow_scan(30,0,0);
    CHECK(!cgu_glow.keys && !cfp.n);

    ui_button(B_SCL);
    ui_algo(CGS_NPART+1u-cgu_part(cgs.focus));
    CHECK(cgu.rack_controls==CGU_RACK_MIX && !cgs.zoom);
    ui_capture("sound-mix-rack");
    ui_button(B_SCL);
    CHECK(cgu.rack_controls==CGU_RACK_MIX && cgs.zoom);
    for(unsigned i=0;i<CGS_NPART;i++){
        unsigned part=cgu_part(i);
        cf_knob_t knob={0};
        cgu_knob_view(i,&knob,1);
        CHECK(!strcmp(knob.name,CGS_PART_NAME[part]));
        level=cgs.level[part];
        ui_knob_turn(i,1);
        CHECK(cgs.level[part]==level+1);
    }
    ui_capture("sound-mix");
    ui_button(B_SCL);
    CHECK(!cgs.zoom && cgu.rack_controls==CGU_RACK_MIX);

    glow_scan(30,glo,solo);
    CHECK(cgu_glow.active && cgs.held_solo && !cfp.n); /* GLO and a key in the same scan */
    glow_scan(30,0,solo);
    CHECK(cgu_glow.active && cgs.held_solo);
    glow_scan(30,home,solo);
    CHECK(!cgu_glow.active && !cgs.held_solo && cgu_glow.keys==solo);
    glow_scan(30,0,0);
    CHECK(!cgu_glow.keys);

    ticks(17);
    unsigned auto_part=1u<<CGS_LAYER, manual_part=1u<<CGS_BASS;
    cgs.next[CGS_LAYER]=cgs.active[CGS_LAYER];
    cgs.next[CGS_BASS]=cgs.active[CGS_BASS];
    cgs.pending=(uint8_t)(auto_part|manual_part);
    cgs.pending_auto=(uint8_t)auto_part;
    uint32_t revision=cgs.take_revision;
    glow_scan(30,glo,1u<<white[CGU_GLO_PAUSE]);
    blocks(1);
    unsigned phase=cpa.q2;
    CHECK(cpa.paused && cpa.on && !cfm.running && !sqp.n && phase>0);
    CHECK(cgs.pending==manual_part && !cgs.pending_auto && cgs.take_revision==revision+1u);
    blocks(16);
    CHECK(cpa.q2==phase);
    glow_scan(30,0,0);
    glow_scan(30,0,1u<<white[CGU_GLO_PAUSE]);
    blocks(1);
    CHECK(!cpa.paused && cfm.running && cpa.q2>=phase);
    glow_scan(30,0,0);
    glow_scan(30,0,1u<<white[CGU_GLO_RESTART]);
    blocks(1);
    CHECK(cpa.on && cpa.q2==0 && cpa.slot==0);
    glow_scan(30,0,1u<<white[CGU_GLO_FILL_BAR]);
    blocks(1);
    CHECK(cf_moment.pending==CFPERF_FILL && !cf_moment.fill);
    ticks(4*SQ_PPQN-cpa.t);
    CHECK(!cf_moment.pending && cf_moment.fill);
    ticks(4*SQ_PPQN);
    CHECK(!cf_moment.fill);
    glow_scan(30,0,1u<<white[CGU_GLO_DROP]);
    blocks(1);
    ticks(4*SQ_PPQN-cpa.t);
    CHECK(cf_moment.dropped);
    glow_scan(30,0,0);
    glow_scan(30,0,1u<<white[CGU_GLO_DROP]);
    blocks(1);
    ticks(4*SQ_PPQN-cpa.t);
    CHECK(!cf_moment.dropped);

    glow_scan(30,0,0);
    ui_open(SCR_FX);
    glow_scan(30,glo,0);
    glow_scan(30,0,0);
    CHECK(cgu_glow.active && cu.scr==SCR_FX);
    ui_button(B_PLAY);
    blocks(1);
    CHECK(!cgs_active() && cgu_glow.active);
    ui_button(B_PLAY);
    blocks(1);
    CHECK(cgs_active() && cgu_glow.active);
    glow_scan(30,glo,0);
    CHECK(!cgu_glow.active && cu.scr==SCR_FX);
    glow_scan(30,0,0);
    cpa_stop();
    cgu_home();
}

static void sound_and_led_studio(void)
{
    studio_start(102);
    blocks(2);
    unsigned chord=cf_panel_pitch_key(sqp.note[0]);
    cgu_key_fade.part=0;
    cf_snapshot();
    cf_leds();
    CHECK(studio_led(chord)==2u);
    cu.now+=80;
    cf_leds();
    CHECK(studio_led(chord)==4u);
    cgs_release(CGS_CHORDS);
    cf_snapshot();
    cu.now+=90;
    cf_leds();
    CHECK(studio_led(chord)==3u);
    cu.now+=180;
    cf_leds();
    CHECK(studio_led(chord)==0u);
    ui_algo(1);
    blocks(1);
    cf_snapshot();
    unsigned bass=cf_panel_pitch_key(sqp.bass[0]);
    cf_leds();
    CHECK(cgu_key_fade.part==CGS_BASS+1u && (cf_sn.studio_keys&(1u<<bass)) && studio_led(bass));
    ui_algo(1);
    blocks(1);
    unsigned key=0;
    while(key<CF_NKEYS && !cf_key_is_upper(key))key++;
    CHECK(key<CF_NKEYS);
    ui_note_key_aged((int)key,1,0);
    blocks(1);
    cf_snapshot();
    cf_leds();
    CHECK(studio_led(key)>0);
    ui_note_key_aged((int)key,0,0);
    ui_select(1);
    ui_button(B_SCL);
    ui_select(1);
    CHECK(cgu.sound_page==CGU_SENDS);
    ui_select(-1);
    CHECK(cgu.sound_page==CGU_TONE && cgu_sound_page_count(CGS_DRUMS)==3);
    cu.now+=200;
    ui_note_key_aged((int)key,1,0);
    blocks(1);
    ui_note_key_aged((int)key,0,0);
    ui_capture("sound-drums");
    CHECK(cf_stage_last.zoom==100 && !cf_stage_last.label[2][0] && cf_stage_last.drum_hits);
    ui_algo(1);
    cf_pool_load(CGS_LAYER,ENGI_VA,pool_pos_factory(ENGI_VA,cgs_profile_voice(CGS_LAYER,0)));
    ui_knob_turn(0,1);
    ui_knob_turn(0,1);
    CHECK(!strcmp(cf_snd[CGS_LAYER].name,"GLASS THREAD"));
    ui_knob_turn(0,-1);
    CHECK(!strcmp(cf_snd[CGS_LAYER].name,"DISTANT BELL"));
    cpa_stop();
    cgu_home();
}

static void curated_sounds_studio(void)
{
    static const struct {
        const char *id, *tone[CGS_DRUMS];
        unsigned kit;
    } curated[]={
        {"dream-pop",{"CABINET PAD","ROUNDWIRE","GLASS THREAD"},DMK_ROOM},
        {"indie",{"JANGLE ORGAN","PICKUP BASS","GLASS THREAD"},DMK_ROOM},
        {"new-wave",{"PULSE STACK","SYNC RELAY","CRYSTAL STEP"},DMK_XTECH},
        {"pop-punk",{"BARBED AMP","RUST PICK","PICK CHIME"},DMK_ROOM},
        {"lo-fi",{"TAPE EP","FELT SUB","DUST TINES"},DMK_TAPE},
        {"ambient",{"STILL AIR","LOW TIDE","SOFT HALO"},DMK_GLASS},
    };
    for(unsigned i=0;i<NELEM(curated);i++){
        unsigned genre=studio_genre_index(curated[i].id);
        CHECK(genre<CGS_NGENRE);
        if(genre>=CGS_NGENRE)continue;
        studio_start_profile(100,genre);
        /* Test the pending first PLAY as well as mid-take sound changes. */
        if(genre)blocks(2);
        unsigned grid=cf_grid.gen, slot=cpa.slot, pulse=cpa.q2;
        ui_select(1);
        CHECK(cgu.tab==CGU_SOUND && !cgs.zoom && !cgu.all_sounds);
        for(unsigned p=0;p<CGS_NPART;p++){
            unsigned count=cgu_sound_count(p,0), engines=0;
            CHECK(count>=(p==CGS_DRUMS?6u:p==CGS_BASS?10u:16u));
            for(unsigned pick=0;pick<count;pick++){
                cf_ent_t choice=cgu_sound_choice(p,0,pick);
                CHECK(cgu_sound_rank(p,0,count,choice)==pick);
                if(p==CGS_DRUMS)CHECK(choice.pos<SQ_NKIT);
                else {
                    CHECK(cf_ent_part(choice.e,choice.pos)==(int)(p&1u));
                    CHECK(pool_entry(choice.e,choice.pos).kind==PK_FACTORY);
                    engines|=1u<<choice.e;
                }
            }
            if(p==CGS_CHORDS || p==CGS_LAYER)CHECK(engines==((1u<<ENGI_FM6)|(1u<<ENGI_VA)));
            if(p<CGS_DRUMS){
                unsigned voice=CGS_PROFILES[genre].original_voice[p];
                unsigned position=pool_pos_factory(ENGI_VA,voice);
                unsigned expected=131u+genre*CGS_DRUMS+p;
                if(p==CGS_LAYER && !strcmp(curated[i].id,"pop-punk"))expected=STUDIO_VA_CLEAN_OCTAVE;
                CHECK(voice==expected && strlen(VA_PRESETS[voice].name)<=12u);
                CHECK(cgu_sound_rank(p,0,count,(cf_ent_t){ENGI_VA,(uint8_t)position})==cgs_profile_tone_count(p));
                for(const uint8_t *edit=VA_PRESET_EDITS[voice];*edit!=255u;edit+=2){
                    CHECK(edit[0]<VA_NP);
                    va_rng_t range=va_range(edit[0]);
                    CHECK((int8_t)edit[1]>=range.min && (int8_t)edit[1]<=range.max);
                }
                int8_t patch[VA_NP], old[VA_NP];
                va_preset_patch(voice,patch);
                for(unsigned i=0;i<voice;i++){
                    va_preset_patch(i,old);
                    CHECK(memcmp(patch,old,sizeof patch));
                }
            }
            for(unsigned send=0;send<NELEM(CGU_SEND_PARAM);send++)
                trk[p].p[CGU_SEND_PARAM[send]]=(int16_t)(25+4u*p+send);
        }
        ui_algo(CGS_NPART);
        ui_button(B_SCL);
        CHECK(cgu.rack_controls==CGU_RACK_LIBRARY && cgs.zoom && cgs.focus==CGS_CHORDS);
        if(!strcmp(curated[i].id,"pop-punk")){
            cu.now+=200;
            ui_capture("sound-library-rack");
        }
        ui_button(B_SCL);
        CHECK(cpa.q2==pulse && cpa.slot==slot && cgu_kit()==curated[i].kit);
        blocks(2);
        if(genre){
            CHECK(cf_grid.gen==grid);
            ticks(CGS_BAR-cpa.t);
        }
        CHECK(cgs_active() && cpa.plan.kit==curated[i].kit);
        for(unsigned p=0;p<CGS_DRUMS;p++){
            CHECK(!strcmp(cf_snd[p].name,curated[i].tone[p]) && trk[p].p[P_REV]==28+4*p);
            CHECK(trk[p].preset==cgs_profile_voice(p,cgs_profile_tone_count(p)-1u));
        }
        ui_button(B_HOME);
        CHECK(cgu.tab==CGU_SOUND && !cgs.zoom);
        cf_fx_t fx=cs.fx;
        pulse=cpa.q2;
        slot=cpa.slot;
        unsigned rank[CGS_NPART], count[CGS_NPART], longest=0;
        uint32_t heard[CGS_NPART];
        for(unsigned p=0;p<CGS_NPART;p++){
            count[p]=cgu_sound_count(p,0);
            if(count[p]>longest)longest=count[p];
            rank[p]=cgu_sound_rank(p,0,count[p],cgu_sound_current(p));
            CHECK(rank[p]<count[p]);
            heard[p]=rank[p]<32u?1u<<rank[p]:0;
        }
        for(unsigned roll=1;roll<=longest;roll++){
            ui_button(B_EDIT);
            CHECK(cgs_active() && cpa.q2==pulse && cpa.slot==slot);
            CHECK(!memcmp(&cs.fx,&fx,sizeof fx));
            for(unsigned p=0;p<CGS_NPART;p++){
                unsigned next=cgu_sound_rank(p,0,count[p],cgu_sound_current(p));
                CHECK(next<count[p] && next!=rank[p]);
                if(next<32u){
                    if(roll<count[p])CHECK(!(heard[p]&(1u<<next)));
                    heard[p]|=1u<<next;
                }
                for(unsigned send=0;send<NELEM(CGU_SEND_PARAM);send++)
                    CHECK(trk[p].p[CGU_SEND_PARAM[send]]==25+4*p+send);
                rank[p]=next;
            }
        }

    }
    cpa_stop();
}

static void record_clear_studio(void)
{
    studio_start(100);
    blocks(2);
    unsigned grid=cf_grid.gen;
    for(unsigned i=0;i<2;i++){
        sq_fresh(&sq_st.pat[i],4);
        sq_st.pat[i].bars=1;
        sq_st.pat[i].nev=1;
    }
    ui_select(2);
    CHECK(cgu.tab==CGU_RECORD && !cgs.zoom);
    ui_algo(-1);
    CHECK(cgu.rack_controls && cgs.focus==CGS_CHORDS);
    ui_button(B_SCL);
    CHECK(cgs.zoom);
    ui_knob_turn(0,1);
    CHECK(cgu.take_slot==1);
    cu.now+=200;
    ui_capture("record-takes");
    uint32_t save=1u<<panel.btn[B_SAVE];
    fm1_in.buttons|=save;
    ui_button(B_SAVE);
    cu.now+=1000;
    cgu_clear_poll();
    fm1_in.buttons&=~save;
    cgu_clear_poll();
    CHECK(sq_has(0) && sq_has(1) && !cgu.clear_down);
    fm1_in.buttons|=save;
    ui_button(B_SAVE);
    cu.now+=3000;
    cgu_clear_poll();
    fm1_in.buttons&=~save;
    CHECK(sq_has(0) && !sq_has(1) && cgs_active() && cf_grid.gen==grid);
    cpa_stop();
}

static void custom_progression_studio(void)
{
    studio_start(100);
    blocks(2);
    memset(&cfwd,0,sizeof cfwd);
    memset(&cfds,0,sizeof cfds);
    cfd_view_update();
    unsigned original=cpa.plan.idea,grid=cf_grid.gen;
    ui_algo(-1);
    ui_button(B_SCL);
    CHECK(cgu_progression_panel() && !cgs.custom);
    ui_button(B_EDIT);
    CHECK(cu.scr==SCR_PEDIT && cfdu.studio && cfwd.have && cfwd.song.n==1);
    ui_select(-1);
    CHECK(cu.page[SCR_PEDIT]==CFD_PHRASE);
    ui_knob_turn(0,1);
    ui_button(B_SCL);
    CHECK(cfdu.focus==1 && cfwd.song.n==2);
    ui_select(1);
    ui_knob_turn(0,3);
    ui_knob_turn(1,2);
    ui_select(1);
    ui_knob_turn(0,1);
    ui_knob_turn(1,-8);
    ui_knob_turn(2,-20);
    ui_algo(-1);
    CHECK(cfdu.focus==0 && cu.page[SCR_PEDIT]==CFD_TIMING);
    ui_algo(1);
    CHECK(cfwd.song.slot[1].degree==3 && cfwd.song.slot[1].quality==CF_SONG_MINOR && cfwd.song.slot[1].quarters==8);
    CHECK(!cpa_setup.flags && !cpa_edit.flags && cpa.plan.idea==original);
    ui_capture("studio-draft-editor");

    cpa_plan_t expected;
    CHECK(cfd_prepare(&expected,cs.chord,CPA_OFF,CPA_BOTH,0,0));
    ui_button(B_PLAY);
    CHECK(cgu_progression_panel() && !cfdu.studio && cgs.custom && cpa_setup.flags && cpa.plan.idea==original);
    CHECK(cpa_setup.plan.studio && cpa_setup.plan.idea==CF_P_DRAFT);
    ticks(CGS_BAR-cpa.t);
    CHECK(cpa.plan.idea==CF_P_DRAFT && cpa.plan.n==2 && cf_grid.gen==grid && cpa.plan.rhythm<DM_N);
    CHECK(!memcmp(cpa.plan.slot,expected.slot,2*sizeof expected.slot[0]));
    ticks(CGS_BAR);
    CHECK(cpa.slot==1 && sqp.n && sqp.dmask==0x89u);
    cu.now+=200;
    ui_capture("studio-custom-progressions");

    ui_button(B_HOME);
    CHECK(!cgs.zoom && cgu.band_panel==CGU_BAND_PROGRESSION);
    ui_button(B_SCL);
    ui_button(B_EDIT);
    CHECK(cu.scr==SCR_PEDIT);
    ticks(CGS_TICKS);
    CHECK(cpa.plan.idea==CF_P_DRAFT && cpa.slot==1 && cf_grid.gen==grid);
    ui_button(B_HOME);
    CHECK(cgu_progression_panel() && cgu.progression_pick);
    ui_knob_turn(0,-1);
    ui_button(B_SCL);
    CHECK(cpa_setup.flags && cpa.plan.idea==CF_P_DRAFT);
    ticks(CGS_BAR-cpa.t);
    CHECK(cpa.plan.idea!=CF_P_DRAFT && cf_grid.gen==grid && cfwd.song.n==2);
    cpa_stop();
}

static void punk_takes_studio(void)
{
    studio_start_profile(102,studio_genre_index("pop-punk"));
    blocks(2);
    sq_snd_poll();
    for(unsigned p=0;p<CGS_DRUMS;p++)
        cgu_load_sound(p,ENGI_VA,pool_pos_factory(ENGI_VA,cgs_profile_voice(p,cgs_profile_tone_count(p)-1u)));
    unsigned clock=cf_grid.gen;
    unsigned chord=cf_snd[CGS_CHORDS].fk,bass=cf_snd[CGS_BASS].fk;
    unsigned kit=cgu_kit();
    int16_t send=trk[CGS_BASS].p[P_REV];
    ui_algo(1);
    ui_button(B_SCL);
    ui_button(B_EDIT);
    sq_snd_poll();
    CHECK(cf_stage[CGS_BASS].ready && cf_snd[CGS_BASS].fk==bass);
    ticks(CGS_TICKS);
    sq_snd_poll();
    CHECK(cf_snd[CGS_BASS].fk!=bass && cf_snd[CGS_CHORDS].fk==chord &&
        trk[CGS_BASS].p[P_REV]==send && cgu_kit()==kit);
    CHECK(cgs.active[CGS_BASS].voice==cgs.active[CGS_DRUMS].voice);
    bass=cf_snd[CGS_BASS].fk;
    cu.edited[CGS_BASS]=1;
    ui_button(B_SCL);
    for(unsigned take=0;take<4;take++){
        unsigned pocket=cgs.playing_feel;
        ui_button(B_EDIT);
        sq_snd_poll();
        CHECK(cpa_setup.flags && cf_stage[CGS_CHORDS].ready && !cf_stage[CGS_BASS].ready);
        ticks(CGS_BAR-cpa.t);
        sq_snd_poll();
        CHECK(cgs.playing_feel!=pocket && cf_snd[CGS_CHORDS].fk!=chord && cf_snd[CGS_BASS].fk==bass);
        CHECK(cpa.plan.kit!=kit && cf_grid.gen==clock);
        kit=cpa.plan.kit;
        chord=cf_snd[CGS_CHORDS].fk;
        for(unsigned b=0;b<2;b++)for(unsigned step=0;step<CGS_STEPS;step++){
            if(!(CGS_PUNK_KICK[cgs.playing_feel][b]&(1u<<step)))continue;
            CHECK(cgs.active[CGS_DRUMS].event[b][step]&CGS_KICK);
            CHECK(cgs.active[CGS_BASS].event[b][step]);
        }
    }
    cu.edited[CGS_BASS]=0;
    cpa_stop();
}

static void rack_meter_studio(void)
{
    studio_start(102);
    cf_motion=1;
    cf_visual_want=1;
    for(unsigned tab=0;tab<CGU_NTAB;tab++){
        if(tab)ui_select(1);
        unsigned min=255,max=0;
        for(unsigned i=0;i<24;i++){
            cu.now+=40;
            blocks(FS/(25u*CTL));
            cf_ui_draw();
            unsigned peak=cf_stage_last.signal_peak;
            if(peak<min)min=peak;
            if(peak>max)max=peak;
        }
        CHECK(cf_stage_last.workspace==tab && min<max && max>0);
    }
    cf_motion=0;
    cf_ui_draw();
    CHECK(!cf_stage_last.signal_peak);
    cpa_stop();
}

static void rack_units_studio(void)
{
    studio_start(102);
    cf_motion=cf_visual_want=1;
    cf_motion_epoch=0;
    cu.now+=500;
    unsigned part=cgs.focus;
    ui_button(B_SEQ);
    for(unsigned i=0;i<20;i++){
        cu.now+=40;
        blocks(FS/(25u*CTL));
        cf_ui_draw();
    }
    unsigned grid=cf_grid.gen;
    CHECK(cgu.band_panel==CGU_BAND_HARMONY && cf_stage_last.rack_beat>0 && cf_stage_last.rack_level>0);
    int harmony_top=cgu_rack_top(CGU_STUDIO,CGS_NPART);
    CHECK(cf_stage_last.rack_scroll==harmony_top && fb[65*FB_W+11]==RAKT_EDGE);
    ui_save_frame("rack-harmony");

    ui_algo(1);
    CHECK(cgu.band_panel==CGU_BAND_SHAPING && cgu_rack_scroll()==harmony_top);
    cu.now+=40;
    int shaping_top=cgu_rack_offset(CGU_STUDIO,CGS_NPART);
    CHECK(cgu_rack_scroll()>harmony_top && cgu_rack_scroll()<shaping_top);
    ui_capture("rack-scroll-start");
    cu.now+=CGU_RACK_SCROLL_MS;
    CHECK(cgu_rack_scroll()==shaping_top);
    ui_algo(1);
    cu.now+=CGU_RACK_SCROLL_MS;
    CHECK(cgu.band_panel==CGU_BAND_GENRE && cgu_rack_scroll()==cgu_rack_offset(CGU_STUDIO,CGS_NPART+1));
    ui_capture("rack-scroll-end");
    ui_knob_turn(0,studio_genre_index("new-wave")-studio_genre_index("dream-pop"));
    cu.now+=210;
    ui_capture("rack-genre-new-wave");
    CHECK(cgs.genre==studio_genre_index("new-wave") && fb[65*FB_W+11]==RAKT_EDGE && cpa_setup.flags);
    ui_knob_turn(0,studio_genre_index("lo-fi")-studio_genre_index("new-wave"));
    cu.now+=210;
    ui_capture("rack-genre-lo-fi");
    CHECK(cgs.genre==studio_genre_index("lo-fi") && fb[65*FB_W+11]==RAKT_EDGE);
    CHECK(!strcmp(cf_stage_last.label[0],"GENRE") && !strcmp(cf_stage_last.label[1],"FAMILY"));
    ui_knob_turn(1,1);
    cf_ui_draw();
    CHECK(cgs.genre==studio_genre_index("ambient") && !strcmp(cf_stage_last.value[1],"Experimental") &&
        cf_stage_last.emphasis[1] && !cgs.zoom && cgs.focus==part);

    ui_algo(-1);
    cu.now+=CGU_RACK_SCROLL_MS;
    cf_ui_draw();
    ui_save_frame("rack-zoom-0");
    ui_button(B_SCL);
    for(unsigned i=1;i<=6;i++){
        char name[24];
        snprintf(name,sizeof name,"rack-zoom-%u",i);
        cu.now+=30;
        ui_capture(name);
    }
    CHECK(cgu.band_panel==CGU_BAND_SHAPING && cf_stage_last.zoom==100);
    ui_knob_turn(2,1);
    for(unsigned i=0;i<6;i++){
        char name[24];
        snprintf(name,sizeof name,"rack-lens-%u",i);
        cu.now+=25;
        blocks(FS/(40u*CTL));
        cf_ui_draw();
        ui_save_frame(name);
    }
    CHECK(cf_stage_last.rack_lens==CGU_LENS_SCALE && cf_stage_last.emphasis[2] && cf_stage_last.rack_level>0);
    cgu_face_t plate={12,53,216,142};
    cgu_face_t close=cgu_unit_lens(plate,2,4,CGU_LENS_SCALE);
    CHECK(close.w>plate.w && close.h>plate.h &&
        abs(cgu_unit_knob_x(plate,2,4)-cgu_unit_knob_x(close,2,4))<=1 &&
        abs(cgu_unit_knob_y(plate,100)-cgu_unit_knob_y(close,100))<=1);
    cu.now+=CGU_LENS_HOLD_MS+CGU_LENS_RETURN_MS;
    cf_ui_draw();
    CHECK(!cf_stage_last.rack_lens);
    ui_button(B_SEQ);
    CHECK(cgs.zoom && cgu.band_panel==CGU_BAND_GENRE && cgs.focus==part && cf_grid.gen==grid);
    ui_button(B_SEQ);
    CHECK(cgu.band_panel==CGU_BAND_ARP && !cgu.arp_timing);
    ui_select(1);
    CHECK(cgu.arp_timing);
    ui_button(B_SEQ);
    CHECK(cgu.band_panel==CGU_BAND_PROGRESSION && cgs.zoom && cf_grid.gen==grid);
    ui_button(B_HOME);
    CHECK(!cgs.zoom && cgu.band_panel==CGU_BAND_PROGRESSION);

    cf_motion=0;
    ui_algo(-4);
    ui_button(B_SCL);
    ui_knob_turn(0,1);
    cf_ui_draw();
    CHECK(cgu.band_panel==CGU_BAND_HARMONY && cf_stage_last.zoom==100 &&
        !cf_stage_last.rack_lens && !cf_stage_last.rack_beat && !cf_stage_last.rack_level);
    cpa_stop();
    cgu_home();
}

static void rack_navigation(void)
{
    studio_start(189);
    cf_motion=1;
    cf_motion_epoch=0;
    cu.now+=1000;
    for(unsigned tab=0;tab<CGU_NTAB;tab++){
        for(unsigned unit=0;unit<cgu_rack_tiles(tab);unit++){
            cgu_face_t face=cgu_rack_face(tab,unit);
            CHECK(face.x==RAKT_X && face.w==RAKT_W);
            if(unit)CHECK(cgu_rack_height(tab,unit-1)+RAKT_GUTTER+face.h<=CGU_BAY_H);
        }
    }
    CHECK(cgu_rack_scroll()==0 && cgs.focus==CGS_CHORDS);
    ui_capture("walk-00");
    ui_algo(1);
    CHECK(cgs.focus==CGS_BASS && !cgu_rack_scroll());
    ui_algo(1);
    CHECK(cgs.focus==CGS_DRUMS && !cgu_rack_scroll());
    cu.now+=RAKT_SLIDE_STEP_MS;
    int moving=cgu_rack_scroll();
    CHECK(moving>0 && moving<cgu_rack_offset(CGU_STUDIO,1));
    ui_algo(-1);
    CHECK(cgs.focus==CGS_BASS && cgu_rack_scroll()==moving);
    cu.now+=CGU_RACK_SCROLL_MS;
    CHECK(cgu_rack_scroll()==cgu_rack_offset(CGU_STUDIO,1));
    ui_algo(1);
    for(unsigned i=1;i<=6;i++){
        char name[24];
        cu.now+=RAKT_SLIDE_STEP_MS;
        snprintf(name,sizeof name,"walk-scroll-%u",i);
        ui_capture(name);
    }
    ui_algo(1);
    for(unsigned i=1;i<=6;i++){
        char name[24];
        cu.now+=RAKT_SLIDE_STEP_MS;
        snprintf(name,sizeof name,"walk-layer-%u",i);
        ui_capture(name);
    }
    ui_button(B_SEQ);
    cu.now+=CGU_RACK_SCROLL_MS;
    int before=cgu_rack_scroll();
    ui_button(B_SCL);
    cu.now+=CGU_RACK_SCROLL_MS;
    ui_button(B_HOME);
    CHECK(!cgs.zoom && cgu.band_panel==CGU_BAND_HARMONY && cgu_rack_scroll()==before);
    ui_select(1);
    CHECK(cgu.tab==CGU_SOUND && !cgs.zoom);
    ui_algo(2);
    CHECK(cgu.rack_controls==CGU_RACK_MIX);
    ui_button(B_SCL);
    cu.now+=CGU_RACK_SCROLL_MS;
    ui_capture("walk-mix");
    CHECK(cgs.zoom);
    ui_select(-1);
    CHECK(cgu.tab==CGU_STUDIO && !cgs.zoom && cgu.band_panel==CGU_BAND_HARMONY);
    ui_select(1);
    CHECK(cgu.tab==CGU_SOUND && !cgs.zoom && cgu.rack_controls==CGU_RACK_MIX);
    for(unsigned i=0;i<=6;i++){
        char name[24];
        snprintf(name,sizeof name,"walk-slide-%u",i);
        ui_capture(name);
        cu.now+=RAKT_SLIDE_STEP_MS;
    }
    ui_algo(-4);
    ui_button(B_SCL);
    cu.now+=CGU_RACK_SCROLL_MS;
    ui_select(1);
    ui_capture("walk-page-0");
    CHECK(cf_stage_last.control_dx==16);
    cu.now+=CGU_RACK_SCROLL_MS;
    ui_capture("walk-page-1");
    CHECK(!cf_stage_last.control_dx);
    unsigned page=cgu.sound_page;
    ui_button(B_LFO);
    CHECK(cu.scr==SCR_EDIT);
    cf_ui_draw();
    cu.now+=CGU_RACK_SCROLL_MS;
    ui_capture("rakt-voice-editor");
    ui_button(B_HOME);
    CHECK(cu.scr==SCR_HOME && cgu.tab==CGU_SOUND && cgs.zoom && cgu.sound_page==page);
    ui_button(B_GLO);
    ui_button(B_SAVE);
    CHECK(cu.scr==SCR_GLO);
    cf_ui_draw();
    cu.now+=CGU_RACK_SCROLL_MS;
    ui_capture("rakt-settings");
    ui_button(B_HOME);
    CHECK(cu.scr==SCR_HOME && cgu.tab==CGU_SOUND && cgs.zoom && cgu.sound_page==page);
    cf_motion=0;
    ui_select(1);
    ui_capture("walk-motion-off");
    CHECK(!cf_stage_last.control_dx && !cf_stage_last.bank_dx);
    cpa_stop();
    cgu_home();
}

static void native_controls_studio(void)
{
    studio_start(102);
    cf_motion=0;
    cu.scr=SCR_ENV;
    cu.pop=0;
    cf_ui_draw();

    /* Each readout must draw after the preceding column's text clip. */
    for(unsigned column=0;column<4;column++)
        CHECK(fb[220*FB_W+column*60+5]==CGU_GLASS);

    unsigned draws=cfn_page_draws, tiles=fb_tiles_sent;
    cf_ui_draw();
    CHECK(cfn_page_draws==draws && fb_tiles_sent==tiles);
}

static void drain(FILE *trace, unsigned sample, uint8_t held[4][128], unsigned *clocks)
{
    while(mo_r!=mo_w){
        uint32_t entry=midi_out_q[mo_r++%MQ];
        unsigned status=entry>>8&255, note=entry>>16&127, velocity=entry>>24&127;
        unsigned part=status&15, type=status&0xf0;
        if(status==0xf8)(*clocks)++;
        if(part>=4 || (type!=0x90 && type!=0x80))continue;
        if(type==0x90 && velocity){CHECK(!held[part][note]);held[part][note]=1;}
        else held[part][note]=0;
        fprintf(trace,"%u,%u,%u,%u,%u\n",sample,part,type,note,velocity);
    }
}
static unsigned studio_render_voice(unsigned part,int original)
{
    unsigned count=cgs_profile_tone_count(part), voice=CGS_PROFILES[cgs.genre].original_voice[part];
    if(original)return voice?voice:PF_NONE;
    return count?cgs_profile_voice(part,count-1u):PF_NONE;
}

static void render_voice(unsigned genre,unsigned part,int original,const char *path)
{
    static const uint8_t notes[][4]={{55,60,64,71},{36,0,0,0},{72,0,0,0}};
    static const uint8_t phrase[][3]={{50,57,62},{36,43,40},{72,79,76}};
    cgs.genre=(uint8_t)genre;
    unsigned voice=studio_render_voice(part,original);
    CHECK(voice<VA_NPRESETS);
    if(voice>=VA_NPRESETS)return;

    cf_boot_go=0;cfb.on=0;cf_intro_on=0;cf_ready=0;cf_evq_r=cf_evq_w;
    song.master_q12=4096;
    fx_output_set(1,1);
    cf_fx_t fx=CFX_DEFAULT;
    fx.on=0;
    cf_fx_apply(&fx);
    track_t *track=&trk[part];
    cf_pool_load(part,ENGI_VA,pool_pos_factory(ENGI_VA,voice));
    blocks(64);
    CHECK(track->engine==ENGI_VA && track->preset==voice);
    track->p[P_LEVEL]=92;
    track->p[P_PAN]=0;

    FILE *file=fopen(path,"wb");
    CHECK(file);
    if(!file)return;
    int32_t samples[CTL*2];
    unsigned peak=0;
    unsigned count=part==CGS_CHORDS?4u:1u;
    unsigned hold_end=FS*5u/2u/CTL, phrase_at=FS*5u/CTL, step=FS/2u/CTL;
    for(unsigned block=0;block<FS*9u/CTL;block++){
        if(!block)for(unsigned i=0;i<count;i++)trk_note_on(track,notes[part][i],88);
        if(block==hold_end)for(unsigned i=0;i<count;i++)trk_note_off(track,notes[part][i]);
        if(block>=phrase_at && block<phrase_at+3u*step){
            unsigned i=(block-phrase_at)/step, t=(block-phrase_at)%step;
            if(!t)trk_note_on(track,phrase[part][i],72u+i*20u);
            if(t==step/2u)trk_note_off(track,phrase[part][i]);
        }
        audio_block(samples,CTL);
        ua.cr=ua.cw;
        CHECK(fwrite(samples,sizeof samples,1,file)==1);
        for(unsigned i=0;i<NELEM(samples);i++){
            unsigned mag=(unsigned)abs(samples[i]>>8);
            if(mag>peak)peak=mag;
        }
    }
    fclose(file);
    CHECK(peak>100 && peak<32767);
    printf("voice=%u name=%s\n",voice,VA_PRESETS[voice].name);
}

static void render_studio(uint32_t seed, unsigned regenerate, const char *path, unsigned rate, unsigned cap, unsigned bars, unsigned genre, unsigned kit)
{
    studio_start_profile(seed,genre);
    if(kit<SQ_NKIT)cpa_setup.plan.kit=(uint8_t)kit;
    if(regenerate==6)cgs.solo=1u<<CGS_CHORDS;
    if(regenerate==7)cgs.solo=1u<<CGS_DRUMS;
    if(regenerate==8 || regenerate==9)for(unsigned p=0;p<CGS_DRUMS;p++){
        unsigned voice=studio_render_voice(p,regenerate==9);
        if(voice==PF_NONE)continue;
        cf_stage[p].ready=0;
        cf_pool_load(p,ENGI_VA,pool_pos_factory(ENGI_VA,voice));
    }
    ua_reset();
    ua.cap_alt=(uint8_t)cap;
    ua.cap_hold=0;
    ua_rate_set(rate);
    int32_t out[CTL*2];
    char name[512];
    snprintf(name,sizeof name,"%s.raw",path);
    FILE *raw=fopen(name,"wb");
    snprintf(name,sizeof name,"%s.csv",path);
    FILE *trace=fopen(name,"w");
    CHECK(raw && trace);
    if(!raw || !trace)return;
    fprintf(trace,"sample,part,status,note,velocity\n");
    unsigned sample=0, sent=0, clocks=0, progression=0, grid=0, kills=voice_kills, sheds=shed_count;
    uint8_t held[4][128]={{0}};
    while(cpa.q2<2u*bars*CGS_BAR){
        audio_block(out,CTL);
        if(!sample){
            CHECK(cgs_active());
            if(!cgs_active())break;
            const cf_progression_t *idea=cf_progression_get(cpa.plan.idea);
            progression=idea?idea->id:cpa.plan.idea;
            grid=cf_grid.gen;
        }
        drain(trace,sample,held,&clocks);
        CHECK(fwrite(out,sizeof *out,CTL*2,raw)==CTL*2);
        ua.cr=ua.cw;
        sample+=CTL;
        if(regenerate==6 && !sent && cgs.bar==2 && cpa.t>=CGS_TICKS){
            ui_note_key_aged(7,1,0);
            sent=1;
        }
        if(regenerate==6 && sent==1 && cgs.bar==3 && cpa.t>=CGS_BAR/2){
            ui_note_key_aged(7,0,0);
            sent=2;
        }
        if(regenerate==5 && sent<bars/8u-1u && cpa.q2>=2u*((sent*8u+7u)*CGS_BAR+13u*CGS_TICKS)){
            ui_button(B_EDIT);
            sent++;
        }
        if(regenerate>0 && regenerate<=CGS_NPART && !sent && cpa.q2>=2u*(CGS_BAR+13u*CGS_TICKS)){
            cgs_request(regenerate-1u,0);
            sent=1;
        }
        cgs_poll();
    }
    CHECK(cgs_active() && cf_grid.gen==grid);
    CHECK(!regenerate || regenerate>=5 || (sent && cgs.generation[regenerate-1u]));
    if(regenerate==5)CHECK(cgs.harmony_generation==bars/8u-1u);
    if(regenerate==6)CHECK(sent==2);
    cpa_stop();
    drain(trace,sample,held,&clocks);
    for(unsigned part=0;part<4;part++)for(unsigned n=0;n<128;n++)CHECK(!held[part][n]);
    fclose(raw);
    fclose(trace);
    printf("seed=%u progression=%u bpm=%u clocks=%u frames=%u voice_kills=%u sheds=%u\n",
           seed,progression,cs.bpm,clocks,sample,voice_kills-kills,shed_count-sheds);
}
static unsigned studio_render_genre(const char *name)
{
    char *end;
    unsigned long index=strtoul(name,&end,10);
    if(end!=name && !*end && index<CGS_NGENRE)return (unsigned)index;
    return studio_genre_index(name);
}

int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--list-genres")){
        for(unsigned genre=0;genre<CGS_NGENRE;genre++){
            const cgs_profile_t *profile=&CGS_PROFILES[genre];
            printf("%s\t%s\t%u\n",profile->id,profile->name,8u*profile->evolve_period_scale);
        }
        return 0;
    }
    emu_fw_options(NULL,1,0,1);
    emu_hal.ready=2;
    emu_fw_init(0);
    if(argc>=8 && argc<=10 && !strcmp(argv[1],"--render")){
        unsigned genre=argc>=9?studio_render_genre(argv[8]):CGS_DEFAULT_GENRE;
        if(genre>=CGS_NGENRE){
            fprintf(stderr,"Unknown Studio genre: %s\n",argv[8]);
            return 2;
        }
        render_studio((uint32_t)strtoul(argv[2],0,0),atoi(argv[3]),argv[4],atoi(argv[5]),atoi(argv[6]),atoi(argv[7]),
                      genre,argc==10?(unsigned)atoi(argv[9]):SQ_NKIT);
    } else if(argc==6 && !strcmp(argv[1],"--render-voice")){
        unsigned genre=studio_render_genre(argv[2]),part=(unsigned)atoi(argv[3]);
        CHECK(genre<CGS_NGENRE && part<CGS_DRUMS);
        if(genre<CGS_NGENRE && part<CGS_DRUMS)render_voice(genre,part,atoi(argv[4]),argv[5]);
    } else {
        genre_registry_studio();
        controls_studio();
        evolve_memory_studio();
        evolve_controls_studio();
        evolve_meter_studio();
        chord_gaps_studio();
        kick_accents_studio();
        late_pocket_studio();
        genre_replies_studio();
        harmony_studio();
        hover_controls_studio();
        workspaces_studio();
        arrangements_studio();
        amp_performance_studio();
        indie_form_studio();
        play_and_mixer_studio();
        glow_live_studio();
        sound_and_led_studio();
        curated_sounds_studio();
        record_clear_studio();
        custom_progression_studio();
        punk_takes_studio();
        rack_meter_studio();
        rack_units_studio();
        rack_navigation();
        native_controls_studio();
    }
    printf("RAKT Studio: %u checks, %u failures\n",checks,failures);
    return failures!=0;
}
