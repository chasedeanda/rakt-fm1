/* SPDX-License-Identifier: GPL-3.0-only
 * FX opens Macros; ALGORITHM opens Individual. SELECT only browses; SEL recalls.
 * SAVE retains explicit user management; a further one-second hold commits. */
#include "cf_fx_factory.c"
enum { FXP_EDIT, FXP_BROWSE, FXP_MANAGE };
static struct {
    uint8_t mode, pick, slot, cursor, action, previous, prev_bypass;
    char name[FXP_NAME];
    cf_fx_t prev;
} fxpu;
enum { FXP_MASTER };
static unsigned fxp_scope;
static struct {
    cf_fx_t prev;
    uint8_t pick, previous, bypass;
} fxp_context[CF_FX_PARTS + 1u];

static const cf_fx_t *fxp_track_chain(unsigned part)
{ return (cfx_studio_rec.mask & (1u << part)) ? &cfx_studio_rec.fx[part] : &cs.fx; }

static const cf_fx_t *fxp_active_chain(void)
{ return fxp_scope ? fxp_track_chain(fxp_scope - 1u) : &cs.fx; }

static int fxp_active_bypass(void)
{
    unsigned bit = fxp_scope ? 1u << (fxp_scope - 1u) : 0;
    return cfx_studio_rec.mask & bit ? (cfx_studio_rec.bypass & bit) != 0 : cfx_bypass;
}

static void fxp_publish(const cf_fx_t *fx, int recall, int bypass)
{
    if (fxp_scope) cfx_studio_save(fxp_scope - 1u, fx, recall, bypass);
    else {
        cge_direct(CGE_FX);
        cs.fx = *fx;
        if (recall) cf_fx_recall(&cs.fx);
        else cf_fx_apply(&cs.fx);
        cf_fx_bypass(bypass);
    }
}

static void fxp_remember(void)
{
    fxpu.prev = *fxp_active_chain();
    fxpu.previous = 1;
    fxpu.prev_bypass = fxp_active_bypass();
}

static const fxp_preset_t *fxp_pick(void)
{
    unsigned i = fxpu.pick;
    if (i < FXP_FACTORY_N) return &FXP_FACTORY[i];
    i -= FXP_FACTORY_N;
    return i < FXP_SLOTS && (fxp_bank.used & (1u << i)) ? &fxp_bank.slot[i] : 0;
}
static const char *fxp_pick_name(void)
{
    const fxp_preset_t *p = fxp_pick();
    return p ? p->name : "Empty user slot";
}
static void fxp_recall(int announce)
{
    const fxp_preset_t *p = fxp_pick();
    if (!p || !fxp_preset_valid(p)) {
        if (announce) cf_message("Empty FX slot");
        return;
    }
    fxp_remember();
    fxp_publish(&p->fx, 1, 0);
    if (announce) cf_message(p->name);
}
static void fxp_recall_selected(void) { fxp_recall(1); }
static void fxp_select(int d)
{
    uint8_t pick = (uint8_t)clampi(fxpu.pick + d, 0, FXP_FACTORY_N + FXP_SLOTS - 1);
    if (pick == fxpu.pick) return;
    fxpu.pick = pick;
    /* Browsing is presentation-only; SEL explicitly recalls. */
}
static void fxp_previous(void)
{
    cf_fx_t previous = fxpu.prev;
    int bypass = fxpu.prev_bypass;
    if (!fxpu.previous) { cf_message("No previous FX chain"); return; }
    fxp_remember();
    fxp_publish(&previous, 1, bypass);
    cf_message("Previous FX chain");
}
static void fxp_clear(void)
{
    cf_fx_t fx = *fxp_active_chain();
    if (fx.on || (fxp_scope && !fxp_active_bypass())) {
        fxp_remember();
        fx.on = 0;
        fxp_publish(&fx, 1, 1);
    } else if (!fxp_scope) cf_fx_bypass(1);
    cf_message("No FX");
}
static void fxp_manage_open(void)
{
    const fxp_preset_t *p = fxp_pick();
    fxpu.mode = FXP_MANAGE; fxpu.action = 0; fxpu.cursor = 0;
    fxpu.slot = 0;
    while (fxpu.slot + 1 < FXP_SLOTS && (fxp_bank.used & (1u << fxpu.slot))) fxpu.slot++;
    if (fxpu.pick >= FXP_FACTORY_N) fxpu.slot = (uint8_t)(fxpu.pick - FXP_FACTORY_N);
    memset(fxpu.name, 0, sizeof fxpu.name);
    str_cpy(fxpu.name, p ? p->name : "MY FX", sizeof fxpu.name);
    cu.save_hold = 0;
}
static void fxp_manage_do(void)
{
    int ok;
    char name[FXP_NAME];
    if (fxpu.action >= 3) {
        if (fxpu.action == 3) fxp_clear(); else fxp_previous();
        fxpu.mode = FXP_BROWSE; return;
    }
    if (fxp_protected || cfs_restore_lock || cr_safe) { cf_message("FX bank is protected"); return; }
    if (!fxpu.action && (fxp_bank.used & (1u << fxpu.slot))) { cf_message("New: choose an empty slot"); return; }
    if (fxpu.action && !(fxp_bank.used & (1u << fxpu.slot))) { cf_message("Choose a saved user slot"); return; }
    memset(name, 0, sizeof name);
    memcpy(name, fxpu.name, FXP_NAME - 1);
    for (int i = FXP_NAME - 2; i >= 0 && (name[i] == ' ' || !name[i]); i--) name[i] = 0;
    ok = fxpu.action == 2 ? fxp_delete_slot(fxpu.slot) : fxp_save_chain(fxpu.slot, name, fxp_active_chain());
    if (!ok) { cf_message("FX name must contain text"); return; }
    fxpu.pick = FXP_FACTORY_N + fxpu.slot; fxpu.mode = FXP_BROWSE;
    cf_message("FX queued for storage");
}
static void fxp_knob_info(int k, cf_knob_t *o)
{
    if (fxpu.mode == FXP_BROWSE) {
        static const char *const N[4] = {"RECALL", "CLEAR FX", "UNDO FX", "EDIT FX"};
        kfmt(o->name, 12, N[k]);
        kfmt(o->val, 14, "Turn +");
    } else {
        static const char *const N[4] = {"SLOT", "LETTER", "CHAR", "ACTION"};
        static const char *const A[5] = {"New", "Replace", "Delete", "No FX", "Previous"};
        kfmt(o->name, 12, N[k]);
        if (k == 0) kint(o->val, fxpu.slot + 1, "");
        else if (k == 1) kint(o->val, fxpu.cursor + 1, "");
        else if (k == 2) { o->val[0] = fxpu.name[fxpu.cursor] ? fxpu.name[fxpu.cursor] : ' '; o->val[1] = 0; }
        else kfmt(o->val, 14, A[fxpu.action]);
    }
}
static void fxp_knob_turn(int k, int d)
{
    if (!d) return;
    cu.save_hold = 0; /* changing a target or action cancels an in-progress hold */
    if (fxpu.mode == FXP_BROWSE) {
        if (k == 0 && d > 0) fxp_recall_selected();
        if (k == 1 && d > 0) fxp_clear();
        if (k == 2 && d > 0) fxp_previous();
        if (k == 3 && d > 0) fxpu.mode = FXP_EDIT;
    } else {
        if (k == 0) fxpu.slot = (uint8_t)clampi(fxpu.slot + d, 0, FXP_SLOTS - 1);
        if (k == 1) fxpu.cursor = (uint8_t)clampi(fxpu.cursor + d, 0, FXP_NAME - 2);
        if (k == 2) {
            unsigned i = 0;
            char c = fxpu.name[fxpu.cursor];
            while (CF_NAME_CHARS[i] && CF_NAME_CHARS[i] != c) i++;
            if (!CF_NAME_CHARS[i]) i = 0;
            for (unsigned j = 0; j < fxpu.cursor; j++) if (!fxpu.name[j]) fxpu.name[j] = ' ';
            fxpu.name[fxpu.cursor] = CF_NAME_CHARS[wrapi((int)i + d, sizeof CF_NAME_CHARS - 1)];
        }
        if (k == 3) fxpu.action = (uint8_t)clampi(fxpu.action + d, 0, 4);
    }
}
