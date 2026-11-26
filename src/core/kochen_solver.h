#ifndef KOCHEN_SOLVER_H
#define KOCHEN_SOLVER_H

#include "core/kochen_types.h"

kochen_status_t kochen_patch_init(
    const kochen_db_t *db,
    kochen_patch_t *patch,
    uint8_t primary_arch_id,
    int8_t companion_arch_id,
    uint8_t pantry_id,
    kochen_mode_t mode
);

kochen_status_t kochen_patch_set_archetypes(
    const kochen_db_t *db,
    kochen_patch_t *patch,
    uint8_t primary_arch_id,
    int8_t companion_arch_id
);

kochen_status_t kochen_patch_solve(const kochen_db_t *db, kochen_patch_t *patch);
kochen_status_t kochen_patch_rebalance_masses(const kochen_db_t *db, kochen_patch_t *patch);
kochen_status_t kochen_patch_reroll_slot(const kochen_db_t *db, kochen_patch_t *patch, uint8_t slot_idx);
kochen_status_t kochen_resolve_home_pantry(const kochen_db_t *db, kochen_patch_t *patch);

#endif /* KOCHEN_SOLVER_H */
