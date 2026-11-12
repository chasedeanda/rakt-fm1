/* SPDX-License-Identifier: GPL-3.0-only
 * FX presets in the previously unused QUAD pair, separate from the 188-byte
 * settings. Main-loop only. A whole bank fits a 1024-byte logged entry.
 * Unknown/corrupt payloads are protected from automatic overwrite. */
_Static_assert(sizeof(fxp_bank_t) <= ST_LMAX(OBJ_CFXPRESET), "FX bank fits the log");
static fxp_bank_t fxp_bank;
static uint32_t fxp_saved, fxp_changed, fxp_retry;
static uint8_t fxp_protected, fxp_tidy;
static int fxp_pending(void) { return fxp_bank.check != fxp_saved; }
static void fxp_load(void)
{
    st_hdr_t h;
    int n;
    fxp_bank_init(&fxp_bank);
    fxp_protected = cr_safe != 0; fxp_tidy = 0;
    fxp_changed = fxp_retry = 0;
    if (!cr_safe) {
        st_check_blank(OBJ_CFXPRESET);
        if (st_current(OBJ_CFXPRESET, &h) >= 0 && h.len) {
            n = st_load(OBJ_CFXPRESET, &fxp_bank, sizeof fxp_bank);
            if (n != sizeof fxp_bank || !fxp_bank_valid(&fxp_bank)) {
                fxp_bank_init(&fxp_bank);
                fxp_protected = 1;
            }
        }
    }
    fxp_saved = fxp_bank.check;
}
static int fxp_save_chain(unsigned slot, const char *name, const cf_fx_t *fx)
{
    fxp_preset_t p = {0};
    unsigned i;
    if (cr_safe || cfs_restore_lock || fxp_protected) return 0;
    for (i = 0; i + 1 < FXP_NAME && name[i]; i++) p.name[i] = name[i];
    if (name[i]) return 0;
    p.fx = *fx;
    if (!fxp_bank_save(&fxp_bank, slot, &p)) return 0;
    fxp_changed = cu.now; fxp_retry = 0; return 1;
}
static int __attribute__((unused)) fxp_save_slot(unsigned slot, const char *name)
{ return fxp_save_chain(slot, name, &cs.fx); }

static int fxp_delete_slot(unsigned slot)
{
    if (cr_safe || cfs_restore_lock || fxp_protected || !fxp_bank_delete(&fxp_bank, slot)) return 0;
    fxp_changed = cu.now; fxp_retry = 0; return 1;
}
/* rc follows st_put: 0 committed, 1 deferred, <0 failed. Never called by audio. */
static int fxp_write(uint32_t how)
{
    int rc;
    if (cr_safe || cfs_restore_lock || fxp_protected) return -1;
    if (!fxp_pending()) return 0;
    rc = st_put(OBJ_CFXPRESET, &fxp_bank, sizeof fxp_bank, how);
    if (!rc) { fxp_saved = fxp_bank.check; fxp_tidy = 0; }
    return rc;
}
static void fxp_poll(uint32_t now, int silent)
{
    int quiet;
    if (cr_safe || cfs_restore_lock || fxp_protected || (fxp_retry && (int32_t)(now - fxp_retry) < 0)) return;
    fxp_retry = 0;
    quiet = silent && now - fxp_changed >= CF_SET_QUIET_MS;
    if (!fxp_pending()) {
        if (!fxp_tidy && quiet) {
            int rc = st_tidy(OBJ_CFXPRESET);
            fxp_tidy = rc == 0;
            if (rc < 0) fxp_retry = now + 1000u;
        }
        return;
    }
    if (now - fxp_changed < CF_SET_QUICK_MS) return;
    if (!quiet && felucca_dbg.last_us * 20u > HALF_FRAMES * 1000000u / FS * 17u) return;
    if (fxp_write(quiet ? 0u : ST_NOERASE)) fxp_retry = now + 1000u;
}
