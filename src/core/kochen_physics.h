#ifndef KOCHEN_PHYSICS_H
#define KOCHEN_PHYSICS_H

#include "core/kochen_types.h"

void kochen_physics_eval_component(
    const kochen_db_t *db,
    const kochen_archetype_t *arch,
    const kochen_patch_t *patch,
    uint8_t component_idx,
    kochen_diagnostics_t *out_diag
);

void kochen_physics_eval_total(
    const kochen_db_t *db,
    const kochen_patch_t *patch,
    kochen_diagnostics_t *primary_out,
    kochen_diagnostics_t *companion_out,
    kochen_diagnostics_t *total_out
);

uint32_t kochen_popcount32(uint32_t x);
float kochen_volatile_jaccard(uint32_t a, uint32_t b);

#endif /* KOCHEN_PHYSICS_H */
