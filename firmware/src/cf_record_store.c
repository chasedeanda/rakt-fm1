/* SPDX-License-Identifier: GPL-3.0-only
 * Three live recordings; old eight-pattern objects are a read-only archive.
 * One A/B object per take makes saves/clears atomic without an undo snapshot.
 * This foreground-only staging buffer is never touched by the audio ISR. */
static uint8_t sq_io[SQ_CHUNK] __attribute__((section(".pool"), aligned(4)));
static uint8_t sq_unavailable; /* corrupt new records must not silently import an old take */
typedef struct { uint32_t magic; uint8_t version, protect, reserved[2]; uint32_t check; } sq_rec_pref_t;
#define SQ_REC_PREF_MAGIC 0x31505243u
static sq_rec_pref_t sq_pref;
static uint8_t sq_pref_dirty;
_Static_assert(sizeof(sq_perf_t)<=ST_PAYLOAD_MAX,"performance fits a recording sector");
static int sq_pref_valid(const sq_rec_pref_t *r)
{
    return r->magic == SQ_REC_PREF_MAGIC && r->version == 1 && r->protect <= 1 &&
        !r->reserved[0] && !r->reserved[1] && r->check == cfs_fnv(r, __builtin_offsetof(sq_rec_pref_t, check));
}
static void sq_pref_load(void)
{
    memset(&sq_pref, 0, sizeof sq_pref); sq_pref.magic = SQ_REC_PREF_MAGIC; sq_pref.version = 1;
    sq_rec_pref_t r;
    if (st_load(OBJ_CFRECPREF, &r, sizeof r) == sizeof r && sq_pref_valid(&r)) sq_pref = r;
    sq_pref_dirty = 0; /* default: automatically replace the oldest completed recording */
}
static void sq_pref_save(void)
{
    if (!sq_pref_dirty || cr_safe || cfs_restore_lock) return;
    sq_pref.check = cfs_fnv(&sq_pref, __builtin_offsetof(sq_rec_pref_t, check));
    if (!st_save(OBJ_CFRECPREF, &sq_pref, sizeof sq_pref)) sq_pref_dirty = 0;
}
static void sq_reorder(void)
{
    for (unsigned i=0;i<SQ_NPAT;i++)sq_order[i]=i;
    for (unsigned i=0;i<SQ_NPAT;i++)for(unsigned j=i+1;j<SQ_NPAT;j++) {
        unsigned a=sq_order[i],b=sq_order[j];
        int ah=sq_perf[a].bars || sq_st.pat[a].nev || sq_drums(&sq_st.pat[a]);
        int bh=sq_perf[b].bars || sq_st.pat[b].nev || sq_drums(&sq_st.pat[b]);
        if ((bh && !ah) || (ah==bh && (int32_t)(sq_serial[b]-sq_serial[a])>0)) {
            sq_order[i]=b;sq_order[j]=a;
        }
    }
}
static unsigned sq_view_slot(unsigned rank)
{
    if (!sqk.on) return sq_order[rank % SQ_NPAT];
    if (!rank) return sqk.pat;
    for(unsigned i=0;i<SQ_NPAT;i++)if(sq_order[i]!=sqk.pat && !--rank)return sq_order[i];
    return sqk.pat;
}
static unsigned sq_number(unsigned physical) { for(unsigned i=0;i<SQ_NPAT;i++)if(sq_view_slot(i)==physical)return i+1;return 1; }
static uint32_t sq_hash_more(uint32_t h, const void *data, uint32_t n)
{
    const uint8_t *p = data;
    while (n--) h = (h ^ *p++) * 16777619u;
    return h;
}
static uint32_t sq_chunk_len(uint32_t k) /* legacy backup lengths, independent of live capacity */
{
    uint32_t size = sizeof(sq_legacy_store_t), off = k * SQ_CHUNK;
    return off >= size ? 0u : size - off < SQ_CHUNK ? size - off : SQ_CHUNK;
}
static void sq_defaults(void)
{
    memset(&sq_st, 0, sizeof sq_st);
    memset(sq_perf,0,sizeof sq_perf);
    sq_meta_known = 0; sq_drafting=0;
    memset(sq_serial,0,sizeof sq_serial);
    sq_st.song.mute = SQ_MUTE_OWN;
    for (unsigned i = 0; i < SQ_NPAT; i++) {
        sq_order[i]=i;
        sq_meta[i].bpm = cs.bpm;
        sq_meta[i].beats = cs.beats;
        sq_meta[i].reserved = 0;
    }
}
static int sq_take_valid(const sq_take_rec_t *r, unsigned slot)
{
    if (r->magic != SQ_TAKE_MAGIC || r->version != 1 || r->slot != slot || r->size != sizeof *r ||
        r->meta.bpm < 30 || r->meta.bpm > 300 || r->meta.beats < 2 || r->meta.beats > 7 || r->meta.reserved ||
        r->pat.bars > SQ_BARS || r->pat.nev > SQ_NEV ||
        r->check != cfs_fnv(&r->serial, sizeof r->serial + sizeof r->meta + sizeof r->pat)) return 0;
    for (unsigned i = 0; i < r->pat.nev; i++) {
        const sq_ev_t *e = &r->pat.ev[i];
        if (e->trk >= SQ_DRUMS || e->bar >= SQ_BARS || e->n > 6 || e->nb > 2 ||
            e->pos >= r->meta.beats * SQ_UB) return 0;
        for (unsigned j = 0; j < e->n; j++) if (e->note[j] > 127) return 0;
        for (unsigned j = 0; j < e->nb; j++) if (e->bass[j] > 127) return 0;
    }
    return 1;
}
static sq_take_rec_t *sq_take_image(unsigned slot)
{
    sq_take_rec_t *r = (sq_take_rec_t *)sq_io;
    r->magic = SQ_TAKE_MAGIC; r->size = sizeof *r; r->version = 1; r->slot = slot;
    r->serial = sq_serial[slot]; r->meta = sq_meta[slot]; r->pat = sq_st.pat[slot];
    r->check = cfs_fnv(&r->serial, sizeof r->serial + sizeof r->meta + sizeof r->pat);
    return r;
}
static uint32_t sq_perf_check(const sq_perf_t *r)
{
    return cfs_fnv(&r->serial,r->size-__builtin_offsetof(sq_perf_t,serial));
}
static int sq_perf_valid(const sq_perf_t *r,unsigned slot,unsigned len)
{
    if(len<SQ_PERF_SIZE(1) || r->magic!=SQ_TAKE_MAGIC || r->version!=2 || r->slot!=slot ||
       !r->n || r->n>SQ_PERF_EVENTS || r->size!=len || len!=SQ_PERF_SIZE(r->n) ||
       !r->bars || r->bars>SQ_BARS || r->reserved || r->meta.reserved || r->padding || r->space>100 ||
       r->meta.bpm<30 || r->meta.bpm>300 || r->meta.beats<2 || r->meta.beats>7 ||
       !r->end || r->end>r->bars*r->meta.beats*SQ_PPQN || r->check!=sq_perf_check(r))return 0;
    for(unsigned i=0;i<3;i++){
        const cf_sref_t *s=&r->sound[i];
        if(s->own>1 || (s->own && (s->engine>=NENGINES || s->kind>PK_USER)))return 0;
    }
    for(unsigned i=0;i<SQ_NTRK;i++)if(r->level[i]>100)return 0;
    unsigned last=0;
    for(unsigned i=0;i<r->n;i++){
        sq_perf_ev_t e=r->ev[i];
        unsigned tick=e.when&SQ_PERF_TICK, part=(e.when>>SQ_PERF_PART_SHIFT)&3u;
        if(tick<last || tick>=r->end)return 0;
        if(part==SQ_DRUMS){
            if((e.data&15u)>=SQ_NLANE || ((e.data>>4)&31u)>=SQ_NKIT || e.data>>9>100)return 0;
        } else if(e.when&SQ_PERF_DRUM_SOFT || e.data>>7>127)return 0;
        last=tick;
    }
    return 1;
}
static uint32_t sq_slot_check(unsigned slot)
{
    return sq_perf[slot].bars?sq_perf[slot].check:sq_take_image(slot)->check;
}
static int sq_legacy_read(uint32_t off, void *dst, uint32_t n)
{
    uint8_t *out = dst;
    while (n) {
        unsigned k = off / SQ_CHUNK, pos = off % SQ_CHUNK;
        int got = st_load(OBJ_CFSEQ0 + k, sq_io, sizeof sq_io);
        unsigned take = SQ_CHUNK - pos; if (take > n) take = n;
        if (got < 0 || pos + take > (unsigned)got) return 0;
        memcpy(out, sq_io + pos, take); out += take; off += take; n -= take;
    }
    return 1;
}
static int sq_legacy_load(void)
{
    struct { uint32_t magic, size, check; sq_song_t song; } head;
    if (!sq_legacy_read(0, &head, sizeof head)) return 0;
    unsigned stride = head.magic == SQ_MAGIC ? sizeof(sq_pat_t) : head.magic == SQ_MAGIC5 ? SQ_PAT5 : 0;
    unsigned first = __builtin_offsetof(sq_legacy_store_t, pat);
    if (!stride || head.size != first + SQ_LEGACY_NPAT * stride || head.song.n > SQ_SONG) return 0;
    uint32_t hash = 2166136261u, off = 12;
    while (off < head.size) {
        unsigned k = off / SQ_CHUNK, pos = off % SQ_CHUNK;
        int n = st_load(OBJ_CFSEQ0 + k, sq_io, sizeof sq_io);
        unsigned end = head.size - k * SQ_CHUNK;
        if (end > SQ_CHUNK) end = SQ_CHUNK;
        if (n != (int)end) return 0;
        hash = sq_hash_more(hash, sq_io + pos, end - pos); off = k * SQ_CHUNK + end;
    }
    if (hash != head.check) return 0;
    for (unsigned i = 0; i < SQ_NPAT; i++) {
        if (!sq_legacy_read(first + i * stride, &sq_st.pat[i], stride)) { sq_defaults(); return 0; }
        if (stride == SQ_PAT5) for (unsigned l = 0; l < SQ_NLANE; l++) sq_st.pat[i].dlev[l] = 100;
        if (!(head.song.mute & SQ_MUTE_OWN)) sq_st.pat[i].mute = head.song.mute & ((1u << SQ_NTRK) - 1u);
        if (sq_st.pat[i].nev > SQ_NEV || sq_st.pat[i].bars > SQ_BARS) { sq_defaults(); return 0; }
        sq_serial[i] = SQ_NPAT-i; /* legacy records have no chronology; retain A..C order */
    }
    sq_meta_known = (1u << SQ_NPAT) - 1u;
    /* Arrangement and D..H are archived, never mapped modulo three. */
    return 1;
}
static void sq_load(void)
{
    sq_defaults(); sq_unavailable = 0; sq_dirty = 0;
    sq_pref_load();
    int legacy = sq_legacy_load();
    for (unsigned i = 0; i < SQ_NPAT; i++) {
        st_hdr_t h;
        int exists = !ST_BLOCKED() && st_current(OBJ_CFTAKE0 + i, &h) >= 0 && h.len;
        int n = st_load(OBJ_CFTAKE0 + i, sq_io, sizeof sq_io);
        const sq_take_rec_t *r = (const sq_take_rec_t *)sq_io;
        const sq_perf_t *take=(const sq_perf_t *)sq_io;
        if(n>0 && sq_perf_valid(take,i,(unsigned)n)){
            memcpy(&sq_perf[i],take,n);
            memset(&sq_st.pat[i],0,sizeof sq_st.pat[i]);
            sq_meta[i]=take->meta;sq_meta_known|=1u<<i;
            sq_serial[i]=take->serial;sq_saved[i]=take->check;
        } else if (n == sizeof *r && sq_take_valid(r, i)) {
            sq_st.pat[i] = r->pat; sq_meta[i] = r->meta; sq_meta_known |= 1u << i;
            sq_serial[i] = r->serial;
            sq_saved[i] = r->check;
        } else if (exists) {
            memset(&sq_st.pat[i], 0, sizeof sq_st.pat[i]);
            sq_unavailable |= 1u << i;
            sq_saved[i] = 0;
        } else {
            sq_saved[i] = sq_take_image(i)->check;
            if (legacy) { sq_saved[i] = ~sq_saved[i]; sq_dirty = 1; } /* migrate A..C independently, leaving the entire archive untouched */
        }
    }
    sq_reorder();
}
static void sq_save_write(void)
{
    if (sqk.on) return; /* never persist an unfinished replacement over its previous take */
    int err = 0;
    for (unsigned i = 0; i < SQ_NPAT; i++) {
        if (sq_unavailable & (1u << i)) continue;
        if(sq_perf[i].bars){
            const sq_perf_t *r=&sq_perf[i];
            if(r->check==sq_saved[i])continue;
            if(st_save(OBJ_CFTAKE0+i,r,r->size))err=1;
            else sq_saved[i]=r->check;
            continue;
        }
        sq_take_rec_t *r = sq_take_image(i);
        if (r->check == sq_saved[i]) continue;
        if (st_save(OBJ_CFTAKE0 + i, r, sizeof *r)) err = 1;
        else sq_saved[i] = r->check;
    }
    sq_dirty = err; sq_retry_ms = err ? cu.now + 1000u : 0;
    sq_pref_save();
}

/* A transient cancel guard in POOL, not an edit/undo history. The three live
 * patterns stay in ordinary RAM. Flash and backup staging use the separate sq_io. */
static struct { sq_take_rec_t take; cf_sref_t refs[3]; uint8_t active, known, perf_bars; } sq_cancel
    __attribute__((section(".pool"), aligned(4)));
static void sq_begin_take(unsigned slot)
{
    sq_cancel.take=*sq_take_image(slot); memcpy(sq_cancel.refs,cfps.r[slot],sizeof sq_cancel.refs);
    sq_cancel.perf_bars=sq_perf[slot].bars;sq_perf[slot].bars=0;
    sq_drafting=1; sq_cancel.active=1; sq_cancel.known=(sq_meta_known>>slot)&1u;
    memset(cfps.r[slot],0,sizeof cfps.r[slot]);
}
static void sq_cancel_take(unsigned slot)
{
    if (!sq_cancel.active) return;
    sq_st.pat[slot]=sq_cancel.take.pat;sq_meta[slot]=sq_cancel.take.meta;sq_serial[slot]=sq_cancel.take.serial;
    sq_perf[slot].bars=sq_cancel.perf_bars;
    sq_meta_known=(sq_meta_known&~(1u<<slot))|((unsigned)sq_cancel.known<<slot);
    memcpy(cfps.r[slot],sq_cancel.refs,sizeof sq_cancel.refs);sq_cancel.active=0;sq_drafting=0;sq_reorder();
}
static void sq_finish_take(unsigned slot)
{
    uint32_t serial=sq_serial[sq_order[0]]+1u;
    sq_serial[slot]=serial;sq_cancel.active=0;sq_drafting=0;sq_reorder();
}
