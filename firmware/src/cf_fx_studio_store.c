/* SPDX-License-Identifier: GPL-3.0-only
 * Studio stores snapshots of the selected chains. Replacing a library slot
 * cannot change any musician using an earlier copy of it. */
#define CFX_STUDIO_MAGIC 0x31584653u /* SFX1 */
typedef struct {
    uint32_t magic;
    uint8_t mask, bypass, reserved[2];
    cf_fx_t fx[CF_FX_PARTS];
    uint32_t check;
} cfx_studio_rec_t;
_Static_assert(sizeof(cfx_studio_rec_t) <= ST_LMAX(OBJ_CFSTUDIOFX), "Studio FX fit one log entry");
static cfx_studio_rec_t cfx_studio_rec;
static uint8_t cfx_studio_dirty, cfx_studio_protected;
static uint32_t cfx_studio_changed, cfx_studio_retry;

static int cfx_studio_record_valid(const cfx_studio_rec_t *rec)
{
    if (rec->magic != CFX_STUDIO_MAGIC || rec->mask >= (1u << CF_FX_PARTS) ||
        (rec->bypass & ~rec->mask) || rec->reserved[0] || rec->reserved[1] ||
        rec->check != cfs_fnv(rec, __builtin_offsetof(cfx_studio_rec_t, check))) return 0;
    for (unsigned i = 0; i < CF_FX_PARTS; i++)
        if ((rec->mask & (1u << i)) && !fxp_chain_valid(&rec->fx[i])) return 0;
    return 1;
}

static void cfx_studio_load(void)
{
    st_hdr_t h;
    memset(&cfx_studio_rec, 0, sizeof cfx_studio_rec);
    cfx_studio_rec.magic = CFX_STUDIO_MAGIC;
    cfx_studio_dirty = cfx_studio_protected = 0;
    cfx_studio_retry = 0;
    if (!CF_STUDIO || cr_safe) return;
    for (unsigned i = 0; i < CF_FX_PARTS; i++) {
        cfx_parts[i].own = 0;
        cfx_parts[i].pending = 0;
    }
    st_check_blank(OBJ_CFSTUDIOFX);
    if (st_current(OBJ_CFSTUDIOFX, &h) < 0 || !h.len) return;
    cfx_studio_rec_t rec;
    if (st_load(OBJ_CFSTUDIOFX, &rec, sizeof rec) != sizeof rec || !cfx_studio_record_valid(&rec)) {
        cfx_studio_protected = 1;
        return;
    }
    cfx_studio_rec = rec;
    for (unsigned i = 0; i < CF_FX_PARTS; i++)
        if (rec.mask & (1u << i)) cfx_part_publish(i, &rec.fx[i], 0, (rec.bypass >> i) & 1u);
}

static void cfx_studio_save(unsigned part, const cf_fx_t *fx, int recall, int bypass)
{
    cfx_studio_rec.mask |= 1u << part;
    cfx_studio_rec.bypass &= ~(1u << part);
    cfx_studio_rec.bypass |= (unsigned)bypass << part;
    cfx_studio_rec.fx[part] = *fx;
    cfx_part_publish(part, fx, recall, bypass);
    if (!cfx_studio_protected && !cr_safe && !cfs_restore_lock) {
        cfx_studio_dirty = 1;
        cfx_studio_changed = cu.now;
        cfx_studio_retry = 0;
    }
}

static int cfx_studio_write(uint32_t how)
{
    cfx_studio_rec.check = cfs_fnv(&cfx_studio_rec, __builtin_offsetof(cfx_studio_rec_t, check));
    int rc = st_put(OBJ_CFSTUDIOFX, &cfx_studio_rec, sizeof cfx_studio_rec, how);
    if (!rc) cfx_studio_dirty = 0;
    return rc;
}

static void cfx_studio_poll(uint32_t now, int silent)
{
    if (!cfx_studio_dirty || now - cfx_studio_changed < CF_SET_QUICK_MS ||
        (cfx_studio_retry && (int32_t)(now - cfx_studio_retry) < 0)) return;
    int quiet = silent && now - cfx_studio_changed >= CF_SET_QUIET_MS;
    if (!quiet && felucca_dbg.last_us * 20u > HALF_FRAMES * 1000000u / FS * 17u) return;
    if (cfx_studio_write(quiet ? 0u : ST_NOERASE)) cfx_studio_retry = now + 1000u;
}
