/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef CF_FX_PRESET_H
#define CF_FX_PRESET_H
#include "cf_fx_types.h"
#define FXP_SLOTS 16u
#define FXP_NAME 21u
#define FXP_MAGIC 0x31505846u /* FXP1, little endian on FM1 */
#define FXP_VERSION 1u
enum { FXP_CLEAN, FXP_VELVET, FXP_BITE, FXP_SLAP, FXP_PING_PONG, FXP_WIDE_DRIFT,
       FXP_PHASE_SWEEP, FXP_RADIO, FXP_SILVER_PLATE, FXP_DEEP_SPACE, FXP_ORBIT,
       FXP_PHONE_LP, FXP_PHONE_HP };
typedef struct { char name[FXP_NAME]; cf_fx_t fx; } fxp_preset_t;
typedef struct {
    uint32_t magic;
    uint16_t version, size, used, reserved;
    fxp_preset_t slot[FXP_SLOTS];
    uint32_t check;
} fxp_bank_t;
int fxp_chain_valid(const cf_fx_t *fx);
int fxp_preset_valid(const fxp_preset_t *p);
void fxp_bank_init(fxp_bank_t *b);
void fxp_bank_seal(fxp_bank_t *b);
int fxp_bank_valid(const fxp_bank_t *b);
/* Mutators validate before changing anything. An empty/deleted slot is all zero. */
int fxp_bank_save(fxp_bank_t *b, unsigned slot, const fxp_preset_t *p);
int fxp_bank_delete(fxp_bank_t *b, unsigned slot);
#endif
