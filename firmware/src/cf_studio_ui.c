/* SPDX-License-Identifier: GPL-3.0-only */
static const char *const CGS_PART_NAME[]={"Chords","Bass","Layer","Drums"};

#if CHASE_LIBRARY && FELUCCA_VA
static unsigned cgs_profile_tone_count(unsigned part)
{
    const cgs_profile_t *profile=&CGS_PROFILES[cgs.genre];
    unsigned n=0;
    while(n<NELEM(profile->tone[part]) && profile->tone[part][n])n++;
    return n;
}
static unsigned cgs_profile_voice(unsigned part,unsigned pick)
{
    const char *name=CGS_PROFILES[cgs.genre].tone[part][pick];
    if(name)for(unsigned i=0;i<VA_NPRESETS;i++)if(cb_streq(VA_PRESETS[i].name,name))return i;
    return PF_NONE;
}
#include "cf_studio_palettes.h"
#endif

static unsigned cgs_near(unsigned pc, unsigned center)
{
    return center-6+(pc+18-center%12)%12;
}
static cf_song_t cgs_custom_harmony;

static void cgs_voice_plan(cpa_plan_t *p,cgs_pitches_t *pitch)
{
    p->studio=1;
    int authored=p->idea==CF_P_DRAFT;
    const cf_progression_t *source=cf_progression_get(p->idea);
    int color=!authored && source && !cf_progression_source(source);
    for(unsigned i=0, center=41;i<p->n;i++){
        cpa_slot_t *s=&p->slot[i];
        unsigned anchor=cgs_near(s->bass_pc,center);
        if(anchor<34)anchor+=12;
        if(anchor>48)anchor-=12;
        if(authored && s->nb)anchor=s->bass[0];
        pitch->note[i][0]=(uint8_t)anchor;
        center=(unsigned)clampi((int)anchor,34,48);
    }
    for(unsigned i=0;i<p->n;i++){
        cpa_slot_t *s=&p->slot[i];
        uint8_t *notes_by_role=pitch->note[i];
        unsigned anchor=notes_by_role[0], target=pitch->note[(i+1)%p->n][0];
        int root_pc=s->root%12;
        if(color && (s->mask==0x91u || s->mask==0x89u)){
            unsigned extension=s->mask==0x89u?10u:11u;
            if(cf_scale_index(p->scale,root_pc+(int)extension-p->tonic)>=0)
                s->mask|=1u<<extension;
            else if(cf_scale_index(p->scale,root_pc+2-p->tonic)>=0)
                s->mask|=1u<<14;
        }
        unsigned third=(s->mask&(1u<<3))?3:(s->mask&(1u<<4))?4:
            (s->mask&(1u<<5))?5:(s->mask&(1u<<2))?2:0;
        unsigned fifth=(s->mask&(1u<<7))?7:(s->mask&(1u<<6))?6:
            (s->mask&(1u<<8))?8:third;
        unsigned upper=(s->mask&(1u<<11))?11:(s->mask&(1u<<10))?10:fifth;
        int direction=target>=anchor?-1:1;
        int approach=(int)target+direction;
        int scale_approach=approach;
        if(cf_scale_index(p->scale,scale_approach-p->tonic)<0 &&
            cf_scale_index(p->scale,scale_approach+direction-p->tonic)>=0)
            scale_approach+=direction;
        if(target==anchor){
            unsigned center=(unsigned)clampi((int)anchor,6,121);
            approach=(int)cgs_near((s->root+fifth)%12,center);
            scale_approach=(int)cgs_near((s->root+third)%12,center);
        }
        notes_by_role[1]=(uint8_t)cgs_near((s->root+third)%12,46);
        notes_by_role[2]=(uint8_t)cgs_near((s->root+fifth)%12,43);
        notes_by_role[3]=(uint8_t)(anchor<=115?anchor+12:anchor-12);
        notes_by_role[4]=(uint8_t)clampi(approach,0,127);
        notes_by_role[5]=(uint8_t)cgs_near((s->root+upper)%12,49);
        notes_by_role[6]=(uint8_t)clampi(scale_approach,0,127);
        if(authored)continue;

        if(CGS_PROFILES[cgs.genre].composer==CGS_COMPOSE_PUNK && source && !cf_progression_source(source) && fifth>=6u && fifth<=8u){
            unsigned root=cgs_near((unsigned)root_pc,51);
            s->note[0]=(uint8_t)root;
            s->note[1]=(uint8_t)(root+fifth);
            s->note[2]=(uint8_t)(root+12u);
            s->n=3;
            continue;
        }

        /* Keep exact sourced pitch classes. Voice the current mask in a
         * compact register, choosing inversions against the previous chord. */
        unsigned root=cgs_near(s->root%12,59);
        int8_t tones[12]; unsigned count=cf_mask_tones(s->mask,tones);
        unsigned inv=cf_auto_inversion((int)root,s->mask,i?p->slot[i-1].note:0,i?p->slot[i-1].n:0);
        if(inv>=count){root-=12;inv-=count;}
        uint8_t notes[12];
        unsigned n=cf_voice((int)root,s->mask,(int)inv,notes);
        if(n && n<=sizeof s->note){
            if(notes[0]<52)for(unsigned j=0;j<n;j++)notes[j]+=12;
            memcpy(s->note,notes,n);
            s->n=(uint8_t)n;
        }
    }
}
static int cgs_fill_harmony(cpa_plan_t *plan,cf_chord_cfg_t cfg)
{
    int roots[7];
    cfg.quantize=CF_Q_BASIC;
    for(int degree=0;degree<7;degree++){
        int key=cf_progression_key(&cfg,degree);
        if(key<0)return 0;
        roots[degree]=cf_key_root_note(&cfg,key);
    }
    uint32_t stamp[CF_H_SLOTS];
    fm1_irq_off();
    uint16_t slots=ch.mode==CF_H_AUTO && cfc.harmony.mode==ch.mode?cfc_effective(&cfc):0;
    memcpy(stamp,cfc.stamp,sizeof stamp);
    fm1_irq_on();
    plan->tonic=cfg.tonic;plan->scale=cfg.scale;
    for(unsigned i=0;i<plan->n;i++){
        cpa_slot_t *s=&plan->slot[i];
        unsigned degree=plan->degree[i];
        unsigned root=(unsigned)roots[degree];
        unsigned third=(roots[(degree+2u)%7u]+12-root)%12;
        unsigned fifth=(roots[(degree+4u)%7u]+12-root)%12;
        unsigned seventh=(roots[(degree+6u)%7u]+12-root)%12;
        uint32_t mask=1u|(1u<<third)|(1u<<fifth);
        /* Leave the downbeat legible. Passing and cadence chords get color. */
        if((i&1u) || (plan->idea-CF_P_COMPOSED==1 && i))mask|=1u<<seventh;
        uint16_t used,conflict;
        s->mask=cf_harmony_apply(&ch,mask,slots,stamp,&used,&conflict);
        s->mask=cf_harmony_limit(s->mask,4);
        s->root=(uint8_t)root;s->bass_pc=(int8_t)(root%12);s->gate=100;
        for(unsigned j=0;j<CF_H_SLOTS;j++)if((used>>j)&1u && ch.bass[j])
            s->bass_pc=(int8_t)cf_bass_pc((int)root%12,s->mask,ch.bass[j]);
    }
    return 1;
}

static int cgs_rebuild_harmony(cpa_plan_t *plan,cf_chord_cfg_t cfg)
{
    if(!cgs_active())return 0;
    cpa_active_request(plan);
    if(plan->idea==CF_P_DRAFT){
        unsigned kit=plan->kit,swing=plan->swing;
        if(!cfd_prepare_song(plan,&cgs_custom_harmony,cfg,plan->rhythm,plan->mode,0,0))return 0;
        plan->kit=(uint8_t)kit;
        plan->swing=(uint8_t)swing;
        plan->studio=1;
        return 1;
    }
    return cgs_fill_harmony(plan,cfg);
}

static int cgs_pick_custom(cpa_plan_t *plan,cgs_pitches_t *pitch,const cf_song_t *draft,cf_chord_cfg_t cfg)
{
    if(draft->beats!=3 && draft->beats!=4 && draft->beats!=6)return 0;
    unsigned rhythm=dm_nth(draft->beats,0),kit=CGS_PROFILES[cgs.genre].kit[0],swing=0;
    if(cgs_active()){
        fm1_irq_off();
        const cpa_plan_t *groove=(cpa_edit.flags&CPA_GROOVE)?&cpa_edit.plan:&cpa.plan;
        if(groove->beats==draft->beats && groove->rhythm<DM_N){rhythm=groove->rhythm;swing=groove->swing;}
        if(cgs.kit_pinned || cgs.genre==cgs.playing_genre)kit=groove->kit;
        fm1_irq_on();
    } else if(cgs.kit_pinned)kit=cpa_setup.plan.kit;
    if(!cfd_prepare_song(plan,draft,cfg,rhythm,CPA_BOTH,0,0))return 0;
    plan->kit=(uint8_t)(kit%SQ_NKIT);
    plan->swing=(uint8_t)swing;
    cgs_voice_plan(plan,pitch);
    return cpa_valid(plan);
}

/* Harmonic functions are constrained per phrase. Each roll chooses the
 * particular degrees and harmonic rhythm inside those constraints. */
static int cgs_write_harmony(cpa_plan_t *plan,cf_chord_cfg_t cfg,unsigned rhythm)
{
    static const uint8_t functions[][6]={
        {0x01,0x28,0x28,0x01,0,0},
        {0x20,0x0a,0x10,0x01,0x28,0x11},
        {0x01,0x42,0x28,0x01,0x0a,0x11}
    };
    unsigned voice=cgs.feel;
    const uint8_t *phrase=functions[voice];
    unsigned bars=8;
    memset(plan,0,sizeof *plan);
    plan->idea=(uint8_t)(CF_P_COMPOSED+voice);
    plan->beats=4;plan->rhythm=(uint8_t)rhythm;plan->studio=1;
    for(unsigned i=0;i<6 && phrase[i];i++){
        unsigned options=phrase[i], choice=cgs_rand(&cgs.harmony_seed,(options&(options-1u))?2u:1u);
        while(choice--)options&=options-1u;
        unsigned degree=0;
        while(!(options&(1u<<degree)))degree++;
        if(i && degree==plan->degree[i-1u])degree=degree==3?5:3;
        unsigned duration=(!i || i==3u || voice==0)?2u:1u;
        if(voice==0 && i==1 && (cgs.harmony_seed&1u))duration=1;
        if(i==3u && voice==0)duration=bars;
        plan->degree[i]=(uint8_t)degree;
        plan->slot[i].ticks=(uint16_t)(duration*CGS_BAR);
        plan->n++;
        bars-=duration;
    }
    return cgs_fill_harmony(plan,cfg);
}
static void cgs_fit_meter(cpa_plan_t *plan)
{
    plan->beats=cgs.meter;
    if(cgs.meter==4u)return;
    plan->rhythm=(uint8_t)cpa_compatible(plan->rhythm,cgs.meter);
    for(unsigned i=0;i<plan->n;i++)
        plan->slot[i].ticks=(uint16_t)(plan->slot[i].ticks/4u*cgs.meter);
}

static unsigned cgs_weight(unsigned index, unsigned scale)
{
    const cf_progression_t *p=cf_progression_get(index);
    if(p->beats!=4 || (cf_progression_source(p) && p->scale!=scale))return 0;
    const cgs_profile_t *genre=&CGS_PROFILES[cgs.genre];
    return 1+8u*((p->tags>>(genre->primary-1))&1u)+4u*((p->tags>>(genre->secondary-1))&1u)
        +2u*(p->scale==scale);
}
static int cgs_pick(cpa_plan_t *plan, cgs_pitches_t *pitch, cf_chord_cfg_t cfg, unsigned exclude)
{
    if(!cf_harmony_supported(cfg.scale,CF_Q_BASIC))return 0;
    const cgs_profile_t *profile=&CGS_PROFILES[cgs.genre];
    unsigned total=0, chosen=0, kit_rhythm=dm_nth(4,0);
    for(unsigned i=0;i<DM_N;i++)if(DM_RHY[i].id==56)kit_rhythm=i;
    if(profile->write_progressions && cgs.feel<3){
        if(!cgs_write_harmony(plan,cfg,kit_rhythm))return 0;
        plan->kit=cgs.feel==0?DMK_AIR:cgs.feel==1?DMK_BRUSH:DMK_TAPE;
        plan->kit%=SQ_NKIT;
        if(cgs.kit_pinned)plan->kit=cgs_active()?cpa.plan.kit:cpa_setup.plan.kit;
        cgs_fit_meter(plan);
        cgs_voice_plan(plan,pitch);
        return cpa_valid(plan);
    }
    for(unsigned i=0;i<cf_progression_count();i++)
        if(i!=exclude)total+=cgs_weight(i,cfg.scale);
    if(!total)return 0;
    unsigned draw=cgs_rand(&cgs.harmony_seed,total);
    for(unsigned i=0;i<cf_progression_count();i++){
        unsigned w=i==exclude?0:cgs_weight(i,cfg.scale);
        if(draw<w){chosen=i;break;}
        draw-=w;
    }
    for(unsigned n=0;n<cf_progression_count();n++){
        unsigned i=(chosen+n)%cf_progression_count();
        if(i==exclude || !cgs_weight(i,cfg.scale))continue;
        if(!cpa_prepare_full(plan,i,cfg,kit_rhythm,CPA_BOTH,0,0))continue;
        int fits=1;
        for(unsigned j=0;j<plan->n;j++)if(plan->slot[j].n>4)fits=0;
        if(!fits)continue;
        if(profile->bank_duration_scale!=1)
            for(unsigned j=0;j<plan->n;j++)plan->slot[j].ticks*=profile->bank_duration_scale;
        if(!profile->preserve_bank_kit)plan->kit=(uint8_t)(profile->kit[0]%SQ_NKIT);
        if(cgs_active() && (cgs.genre==cgs.playing_genre || cgs.kit_pinned)){
            const cpa_plan_t *groove=(cpa_edit.flags&CPA_GROOVE)?&cpa_edit.plan:&cpa.plan;
            plan->rhythm=groove->rhythm; plan->kit=groove->kit;
            plan->swing=groove->swing; plan->mode=groove->mode;
        } else if(cgs.ready && !cgs_active() && (cgs.genre==cgs.queued_genre || cgs.kit_pinned) && cpa_setup.plan.studio)plan->kit=cpa_setup.plan.kit;
        cgs_fit_meter(plan);
        cgs_voice_plan(plan,pitch);
        return 1;
    }
    return 0;
}
static void cgs_init(uint32_t seed)
{
    memset(&cgs,0,sizeof cgs);
    static const uint8_t activity[]={48,66,39,57}, level[]={78,75,30,75};
    memcpy(cgs.activity,activity,sizeof activity);
    memcpy(cgs.level,level,sizeof level);
    for(unsigned i=0;i<CGS_NPART;i++){
        cgs.seed[i]=(seed+0x85ebca6bu*(i+1u))|1u;
        cgs.develop_seed[i]=(cgs.seed[i]^0x9e3779b8u);
        cgs.gain[i]=32768u;
        cgs.touch[i]=50;
    }
    cgs.harmony_seed=seed|1u;
    cgs.space=66;
    cgs.energy=cgs.evolution=50;
    cgs.genre=CGS_DEFAULT_GENRE;
    cgs.feel=(uint8_t)(seed%4u);
    cgs.meter=cs.beats==3u || cs.beats==6u?cs.beats:4u;
}
enum { CGS_REROLL, CGS_DEVELOP, CGS_RESHAPE, CGS_STYLE };
static const cgs_clip_t *cgs_latest(unsigned p)
{
    if(cpa_setup.flags && cgs_active())return &cgs.take[p];
    return (cgs.pending&(1u<<p))?&cgs.next[p]:&cgs.active[p];
}
static int cgs_same_clip(const cgs_clip_t *a,const cgs_clip_t *b)
{
    return a->responds==b->responds && !memcmp(a,b,offsetof(cgs_clip_t,seed));
}
static void cgs_reshape(cgs_clip_t *clip, unsigned p, const cgs_clip_t *reference, int reset)
{
    uint8_t feel=cgs.feel;
    uint32_t seed=reference->seed;
    cgs.feel=reference->voice;
    cgs_compose(clip,p,&seed);
    cgs.feel=feel;
    if(reset)return;
    clip->edits=reference->edits;
    unsigned steps=cgs_steps(cgs.meter);
    for(unsigned i=0;i<clip->edits.count;i++)cgs_apply_edit(clip,p,steps,clip->edits.item[i]);
    cgs_prepare_response_timing(clip,p);
}

/* Manual changes can also update the pending band take. */
static void cgs_publish_clip(unsigned p, const cgs_clip_t *clip, int queued_only)
{
    if(!cgs_active()){
        cgs.active[p]=*clip;
        if(cgs.hold&(1u<<p))cgs_capture_response(p,0);
        return;
    }
    if(cpa_setup.flags)cgs.take[p]=*clip;
    if(queued_only)return;
    if(!cpa_setup.flags || (cgs.queued_genre==cgs.playing_genre && cgs.queued_feel==cgs.playing_feel &&
                           cgs.meter==cpa.plan.beats)){
        cgs.next[p]=*clip;
        cgs.pending|=(uint8_t)(1u<<p);
    }
}

static int cgs_can_develop(unsigned p)
{
    unsigned bit=1u<<p;
    return cgs_active() && !cpa.paused && !cpa.clock_wait && !cpa_setup.flags &&
           cgs.meter==cpa.plan.beats && cgs.genre==cgs.playing_genre && cgs.evolution &&
           !((cgs.pending|cgs.hold|cgs_mute_mask())&bit) && cgs.activity[p]>=35;
}

static void cgs_develop_part(unsigned p, uint32_t revision)
{
    fm1_irq_off();
    if(revision!=cgs.take_revision || !cgs_can_develop(p)){fm1_irq_on();return;}
    const unsigned generation=cgs.generation[p], meter=cpa.plan.beats;
    cgs_clip_t reference=cgs.active[p];
    uint32_t seed=cgs.develop_seed[p];
    unsigned kicks=0;
    if(p==CGS_BASS)
        for(unsigned step=0;step<cgs_steps(meter);step++)
            if(cgs.active[CGS_DRUMS].event[1][step]&CGS_KICK)kicks|=1u<<step;
    fm1_irq_on();

    cgs_clip_t clip=reference;
    if(reference.edits.count==CGS_MAX_EDITS){
        for(unsigned i=1;i<CGS_MAX_EDITS;i++)reference.edits.item[i-1]=reference.edits.item[i];
        reference.edits.count--;
        cgs_reshape(&clip,p,&reference,0);
    }
    cgs_edit_t edit={0};
    unsigned steps=cgs_steps(meter);
    if(!cgs_make_edit(&clip,p,steps,kicks,&seed,&edit))return;
    cgs_apply_edit(&clip,p,steps,edit);
    cgs_prepare_response_timing(&clip,p);
    clip.edits.item[clip.edits.count++]=edit;

    fm1_irq_off();
    if(revision==cgs.take_revision && generation==cgs.generation[p] && meter==cpa.plan.beats &&
       cgs_can_develop(p) && !cgs_same_clip(&cgs.active[p],&clip)){
        unsigned bit=1u<<p;
        cgs.next[p]=clip;
        cgs.next_develop_seed[p]=seed;
        cgs.pending_auto|=(uint8_t)bit;
        cgs.pending|=(uint8_t)bit;
    }
    fm1_irq_on();
}

static void cgs_request(unsigned p, int action)
{
    if(action==CGS_DEVELOP){cgs_develop_part(p,cgs.take_revision);return;}
    for(unsigned attempt=0;attempt<2;attempt++){
        cgs_clip_t clip, reference;
        fm1_irq_off();
        cgs_cancel_auto(1u<<p);
        uint32_t revision=cgs.take_revision, seed=cgs.seed[p];
        reference=*cgs_latest(p);
        fm1_irq_on();
        if(action==CGS_RESHAPE || action==CGS_STYLE){
            cgs_reshape(&clip,p,&reference,action==CGS_STYLE);
        } else {
            uint8_t feel=cgs.feel;
            const cgs_profile_t *profile=&CGS_PROFILES[cgs.genre];
            if(profile->composer==CGS_COMPOSE_DREAM)
                cgs.feel=(uint8_t)((reference.voice+1u+cgs_rand(&seed,profile->feel_count-1u))%profile->feel_count);
            else if(profile->composer==CGS_COMPOSE_PUNK)cgs.feel=reference.voice;
            cgs_compose(&clip,p,&seed);
            if(cgs_same_clip(&reference,&clip))cgs_compose(&clip,p,&seed);
            cgs.feel=feel;
            if(cgs_same_clip(&reference,&clip)){
                unsigned step=cgs.meter==4u?13u:10u;
                if(p==CGS_CHORDS && clip.event[1][0]<=1)clip.feel[1][0]^=8u;
                else if(p==CGS_DRUMS)clip.event[1][step]^=CGS_HAT;
                else clip.event[1][step]=clip.event[1][step]?0:
                    (p==CGS_CHORDS?4u:CGS_FIFTH+(3u<<3));
                cgs_prepare_response_timing(&clip,p);
            }
        }
        fm1_irq_off();
        if(revision==cgs.take_revision){
            cgs.seed[p]=seed;
            if(!cgs_same_clip(&reference,&clip) || clip.edits.count!=reference.edits.count)
                cgs_publish_clip(p,&clip,0);
            fm1_irq_on();
            return;
        }
        fm1_irq_on();
    }
}

static void cgs_hold_turn(unsigned p, int d)
{
    unsigned bit=1u<<p;
    fm1_irq_off();
    int reshape=d<0 && (cgs.hold&bit) && cgs_latest(p)->shape_energy!=cgs.energy;
    if(d>0){
        if(!(cgs.hold&bit))cgs_capture_response(p,0);
        cgs.hold|=(uint8_t)bit;
        cgs_cancel_auto(bit);
    } else cgs.hold&=(uint8_t)~bit;
    fm1_irq_on();
    if(reshape)cgs_request(p,CGS_RESHAPE);
}
static void cgs_prepare_start(void)
{
    memcpy(cgs.boot_sound_gen,cf_snd_gen,sizeof cgs.boot_sound_gen);
    for(unsigned p=0;p<CGS_NPART;p++)cgs_compose(&cgs.active[p],p,&cgs.seed[p]);
    if(cgs_pick(&cpa_setup.plan,&cgs.setup_pitch,cs.chord,CF_P_NONE)){
        cgs.ready=1;
        cgs.queued_genre=cgs.genre;
        cgs.queued_feel=cgs.feel;
    }
}
/* Offer the profile's sounds on the first Studio PLAY. A saved, edited,
 * browsed or currently played part keeps the user's patch. */
static void cgs_prepare_sounds(void)
{
    if(cgs.defaults_offered)return;
    cgs.defaults_offered=1;
#if CHASE_LIBRARY && FELUCCA_VA
    if(cfs_loaded)return;
    for(unsigned part=0;part<3;part++){
        if(cgs.boot_sound_gen[part]!=cf_snd_gen[part] || cu.edited[part] || cgs_manual(part) ||
           cf_snd[part].kind==PK_USER)continue;
        unsigned count=cgs_profile_tone_count(part);
        if(!count)continue;
        unsigned pick=CGS_PROFILES[cgs.genre].start_curated?count-1u:0;
        unsigned voice=cgs_profile_voice(part,pick);
        if(voice==PF_NONE)continue;
        cf_stage_t prepared;
        cf_snd_build(part,ENGI_VA,pool_pos_factory(ENGI_VA,voice),&prepared);
        prepared.pat=CGS_PATCH_STAGE;
        prepared.own1=prepared.tw=prepared.taken=0;
        prepared.ready=1;
        fm1_irq_off();
        cf_stage[part]=prepared;
        fm1_irq_on();
    }
#endif
}
static void cgs_sound_boundary(unsigned tag)
{
    int ready=0;
    for(unsigned part=0;part<CGS_DRUMS;part++){
        if(cf_stage[part].pat!=tag || !cf_stage[part].ready)continue;
        if(cgs_manual(part) || cu.edited[part]){cf_stage[part].ready=0;continue;}
        if(tag==CGS_PATCH_ROLL)cgs_release(part);
        ready=1;
    }
    if(ready)cf_stage_in(tag);
}
static void cgs_cancel_rolled_sounds(void)
{
    for(unsigned p=0;p<CGS_DRUMS;p++)
        if(cf_stage[p].pat==CGS_PATCH_ROLL)cf_stage[p].ready=0;
}
static int cgs_take_from(int play,int reroll,const cf_song_t *draft,int capture)
{
    if(cr_safe || cfs_restore_lock || sqk.on || cf_sn.sq_on || cf_sn.sq_rec!=255){
        cf_message("Finish recording/playback first");
        return 0;
    }
    if(cpa_running() && !cgs_active()){
        cf_message("Stop playback before Studio");
        return 0;
    }
    if(cf_evq_w-cf_evq_r>CF_EVQ-6u)return 0;
    cpa_plan_t plan;
    cgs_pitches_t pitch={0};
    int cached=cgs.ready && cpa_setup.plan.studio && cpa_setup.plan.idea!=CF_P_DRAFT;
    unsigned exclude=CF_P_NONE;
    if(reroll && cgs_active())exclude=cpa.plan.idea;
    else if(reroll && cached)exclude=cpa_setup.plan.idea;
    unsigned previous_feel=cgs.feel;
    unsigned previous_meter=cgs.meter;
    int switching=(draft!=NULL)!=cgs.custom;
    int new_draft=draft && draft!=&cgs_custom_harmony;
    if(!draft && cgs.custom)cgs.meter=cgs.bank_meter;
    unsigned feel_count=CGS_PROFILES[cgs.genre].feel_count;
    if(reroll && feel_count>1)
        cgs.feel=(uint8_t)((cgs.feel+1u+cgs_rand(&cgs.harmony_seed,feel_count-1u))%feel_count);
    int prepared=1;
    if(draft){
        cf_chord_cfg_t cfg=cs.chord;
        if(draft!=&cgs_custom_harmony){cfg.tonic=draft->tonic;cfg.scale=draft->scale;}
        else if(cpa_setup.flags && cpa_setup.plan.idea==CF_P_DRAFT){
            cfg.tonic=cpa_setup.plan.tonic;cfg.scale=cpa_setup.plan.scale;
        }
        prepared=cgs_pick_custom(&plan,&pitch,draft,cfg);
    } else if(play && cached && cgs.queued_genre==cgs.genre && cpa_setup.plan.tonic==cs.chord.tonic &&
        cpa_setup.plan.scale==cs.chord.scale && cpa_setup.plan.beats==cgs.meter){
        plan=cpa_setup.plan;
        pitch=cgs.setup_pitch;
    } else prepared=cgs_pick(&plan,&pitch,cs.chord,exclude);
    if(!prepared){
        cgs.feel=(uint8_t)previous_feel;
        cgs.meter=(uint8_t)previous_meter;
        cf_message(draft?"Draft needs 3/4, 4/4 or 6/8":"No compatible Studio progression");
        return 0;
    }
    if(draft && !cgs.custom)cgs.bank_meter=(uint8_t)previous_meter;
    if(draft && draft!=&cgs_custom_harmony)cgs_custom_harmony=*draft;
    cgs.custom=(uint8_t)(draft!=NULL);
    cgs.meter=plan.beats;
    if(cgs_active()){
        cgs_clip_t clip[CGS_NPART];
        for(unsigned p=0;p<CGS_NPART;p++)cgs_compose(&clip[p],p,&cgs.seed[p]);
        fm1_irq_off();
        cgs_cancel_auto((1u<<CGS_NPART)-1u);
        cgs.clear_take_hold=(uint8_t)(reroll || cgs.genre!=cgs.playing_genre);
        memcpy(cgs.take,clip,sizeof clip);
        cpa_setup.plan=plan;
        cgs.setup_pitch=pitch;
        cgs.queued_genre=cgs.genre;
        cgs.queued_feel=cgs.feel;
        cpa_setup.flags=1;
        fm1_irq_on();
        return 1;
    }
    int rebuilt=!cgs.ready || !play || switching || new_draft || cgs.queued_genre!=cgs.genre;
    if(rebuilt){
        for(unsigned p=0;p<CGS_NPART;p++)cgs_compose(&cgs.active[p],p,&cgs.seed[p]);
        if(reroll || cgs.genre!=cgs.playing_genre)cgs.hold=0;
        cgs.ready=1;
    }
    fm1_irq_off();
    cpa_setup.plan=plan;
    cgs.setup_pitch=pitch;
    cgs.queued_genre=cgs.genre;
    cgs.queued_feel=cgs.feel;
    cpa_setup.flags=(uint8_t)play;
    if(rebuilt)for(unsigned p=CGS_BASS;p<=CGS_LAYER;p++)
        if(cgs.hold&(1u<<p))cgs_capture_response(p,0);
    fm1_irq_on();
    if(draft){cs.chord.tonic=plan.tonic;cs.chord.scale=plan.scale;cs.beats=plan.beats;cf_send_config();}
    if(!play){cs.beats=cgs.meter;cf_send_metro();return 1;}
    cgs_prepare_sounds();
    if(!draft && !cfs_loaded && !cge.tempo_pin)cs.bpm=CGS_PROFILES[cgs.genre].bpm;
    cpa_explore();
    cf_send_metro();
    if(!cf_post(CFE_ACCOMP,1,capture,0))return 0;
    cpa_commanded=1;
    return 1;
}
static int cgs_new_take_for(int play,int reroll)
{ return cgs_take_from(play,reroll,cgs.custom?&cgs_custom_harmony:NULL,0); }

static int cgs_start_recording(void)
{ return cgs_take_from(1,0,cgs.custom?&cgs_custom_harmony:NULL,1); }

static int cgs_select_progression(int custom,int play)
{
    if(cpa_commanded!=cpa_running()){cf_message("Transport changing: try again");return 0;}
    if(custom && !cfwd.have){cf_message("EDIT to create a progression");return 0;}
    return cgs_take_from(play,0,custom?&cfwd.song:NULL,0);
}
static int cgs_new_take(int play)
{ return cgs_new_take_for(play,!play); }

static unsigned cgs_evolve_period(unsigned genre)
{
    unsigned bars=cgs.evolution>=80?2u:cgs.evolution>=50?4u:8u;
    return bars*CGS_PROFILES[genre].evolve_period_scale;
}

static void cgs_evolve_turn(int d)
{
    fm1_irq_off();
    unsigned was=cgs.evolution;
    cgs.evolution=(uint8_t)clampi(cgs.evolution+d,0,100);
    if(!cgs.evolution)cgs_cancel_auto((1u<<CGS_NPART)-1u);
    else if(!was && cgs_active())
        cgs.developed=(cgs.bar-cgs.origin_bar)/cgs_evolve_period(cgs.playing_genre);
    fm1_irq_on();
}

static int cgs_home(void);
static void cgs_poll(void)
{
    cgs.mix_idle=(uint8_t)((cgs_home() || cgu_studio_fx_screen()) && !cpa.on && !sqp.on && !dmp.on);
    fm1_irq_off();
    if(!cgs_active() || cpa.paused || cpa.clock_wait || cpa_setup.flags || cgs.pending){
        fm1_irq_on();
        return;
    }
    cgs.genre=cgs.playing_genre;
    cgs.feel=cgs.playing_feel;
    cgs.meter=cpa.plan.beats;
    cgs.custom=cpa.plan.idea==CF_P_DRAFT;
    unsigned phrase=(cgs.bar-cgs.origin_bar)/cgs_evolve_period(cgs.genre);
    int due=cgs.evolution && phrase && phrase!=cgs.developed;
    uint32_t revision=cgs.take_revision;
    if(due)cgs.developed=phrase;
    fm1_irq_on();
    if(due)cgs_develop_part((phrase-1u)%CGS_NPART,revision);
}
static int cgs_home(void)
{ return CF_STUDIO && !CF_SCENES && home_stage() && cf_home_bank==CF_HB_ENGINE && !cge_active(); }
static void cgs_harmony_turn(int k,int d)
{
    if(k==1){cpa_tempo(d);return;}
    if(k==3){
        if(cgs.custom){cf_message("Set draft meter on Phrase");return;}
        static const uint8_t meter[]={3,4,6};
        unsigned i=cgs.meter==3?0:cgs.meter==6?2:1;
        unsigned was=cgs.meter;
        cgs.meter=meter[wrapi((int)i+d%3,3)];
        if(cgs.meter!=was && !cgs_new_take_for(0,0))cgs.meter=(uint8_t)was;
        return;
    }
    if(cgs_active()){
        if(cpa_setup.flags && cpa_setup.plan.idea==CF_P_DRAFT){cf_message("Progression queued");return;}
        cpa_home_key(k==0?1:2,d);return;
    }
    if(k==0)cs.chord.tonic=(uint8_t)wrapi(cs.chord.tonic+d%12,12);
    else cs.chord.scale=cpa_scale_step(cs.chord.scale,d);
    cf_send_config();
    cgs_new_take_for(0,0);
}
static void cgs_tempo_info(cf_knob_t *o)
{
    int bpm=cs.clock_in==1u?cf_sn.ext_bpm:cs.bpm;
    cpa_tempo_info(o);
    o->bar=(int16_t)(clampi(bpm-30,0,270)*255/270);
}

static void cgs_knob_info(int k,cf_knob_t *o,unsigned p,int part_detail)
{
    if(part_detail){
        kfmt(o->name,12,k==0?"STYLE":k==1?"ACTIVITY":k==2?"LEVEL":p==CGS_CHORDS?"SUSTAIN":p==CGS_DRUMS?"PUNCH":"TOUCH");
        if(k==0){
            kfmt(o->val,14,CGS_PROFILES[cgs.genre].style[p][cgs.style[p]]);
            o->bar=cgs.style[p]?255:0;
        }
        else {
            unsigned value=k==1?cgs.activity[p]:k==2?cgs.level[p]:p==CGS_CHORDS?cf_env_val(p,2):cgs.touch[p];
            kint(o->val,value,"%");o->bar=(int16_t)(value*255u/100u);
        }
    } else {
        kfmt(o->name,12,k==0?"SPACE":k==1?"TEMPO":k==2?"ENERGY":"EVOLVE");
        if(k==1)cgs_tempo_info(o);
        else {
            unsigned value=k==0?cgs.space:k==2?cgs.energy:cgs.evolution;
            kint(o->val,value,"%");o->bar=(int16_t)(value*255u/100u);
        }
    }
}
static void cgs_genre_set(unsigned genre)
{
    unsigned was=cgs.genre;
    if(genre==was)return;
    cgs.genre=(uint8_t)genre;
    if(cgs_new_take(0)){
        fm1_irq_off();
        cgs_cancel_rolled_sounds();
        fm1_irq_on();
    } else cgs.genre=(uint8_t)was;
}
static void cgs_genre_turn(int d)
{
    cgs_genre_set((unsigned)wrapi((int)cgs.genre+d%CGS_NGENRE,CGS_NGENRE));
}

static void cgs_family_turn(int d)
{
    unsigned family=CGS_PROFILES[cgs.genre].family;
    unsigned next=(unsigned)wrapi((int)family+d%CGS_NFAMILY,CGS_NFAMILY);
    if(next==family)return;
    for(unsigned genre=0;genre<CGS_NGENRE;genre++)
        if(CGS_PROFILES[genre].family==next){cgs_genre_set(genre);return;}
}

static void cgs_energy_turn(int d)
{
    unsigned value=(unsigned)clampi(cgs.energy+d,0,100);
    if(value==cgs.energy)return;
    cgs.energy=(uint8_t)value;
    for(unsigned attempt=0;attempt<2;attempt++){
        cgs_clip_t clips[CGS_NPART];
        unsigned all=(1u<<CGS_NPART)-1u, changed=0;
        fm1_irq_off();
        uint32_t revision=cgs.take_revision;
        int queued=cgs_active() && cpa_setup.flags;
        unsigned reshape=queued && cgs.clear_take_hold?all:all&~cgs.hold;
        cgs_cancel_auto(reshape);
        for(unsigned p=0;p<CGS_NPART;p++)
            if(reshape&(1u<<p))clips[p]=*cgs_latest(p);
        fm1_irq_on();
        for(unsigned p=0;p<CGS_NPART;p++){
            if(!(reshape&(1u<<p)))continue;
            cgs_clip_t clip;
            cgs_reshape(&clip,p,&clips[p],0);
            if(!cgs_same_clip(&clips[p],&clip)){
                clips[p]=clip;
                changed|=1u<<p;
            }
        }
        fm1_irq_off();
        if(revision==cgs.take_revision){
            for(unsigned p=0;p<CGS_NPART;p++)
                if(changed&(1u<<p))cgs_publish_clip(p,&clips[p],queued);
            fm1_irq_on();
            return;
        }
        fm1_irq_on();
    }
}
static void cgs_knob_turn(int k,int d,int part_detail)
{
    if(!d)return;
    if(!part_detail){
        if(k==0)cgs.space=(uint8_t)clampi(cgs.space+d,0,100);
        else if(k==1)cpa_tempo(d);
        else if(k==2)cgs_energy_turn(d);
        else cgs_evolve_turn(d);
        return;
    }
    unsigned p=cgs.focus;
    if(k==3){
        if(p==CGS_CHORDS){
            cf_env_set_val(p,2,clampi((int)cf_env_val(p,2)+d,0,100));
            cf_edited(p);
            if(cs.env_link)cf_env_follow();
        } else cgs.touch[p]=(uint8_t)clampi(cgs.touch[p]+d,0,100);
    } else if(k==2){
        cgs.level[p]=(uint8_t)clampi(cgs.level[p]+d,0,100);
    } else {
        if(k==0 && cgs.style[p]==(d>0))return;
        if(k==0)cgs.style[p]=(uint8_t)(d>0);
        if(k==1){
            unsigned activity=(unsigned)clampi(cgs.activity[p]+d,0,100);
            if(activity==cgs.activity[p])return;
            cgs.activity[p]=(uint8_t)activity;
        }
        cgs_request(p,k==0?CGS_STYLE:CGS_RESHAPE);
    }
}
