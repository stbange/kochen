#include "core/kochen_solver.h"
#include "core/kochen_physics.h"
#include <math.h>
#include <string.h>

static uint32_t pcg32_next(uint64_t *state) {
    uint64_t old = *state;
    *state = old * 6364136223846793005ULL + 1442695040888963407ULL;
    uint32_t xsh = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t)(old >> 59u);
    return (xsh >> rot) | (xsh << ((-rot) & 31u));
}

static float rand01(uint64_t *st) { return (float)(pcg32_next(st) >> 8) * (1.0f / 16777216.0f); }

static void populate_arch_slots(
    const kochen_archetype_t *arch,
    uint8_t comp_idx,
    float batch_g,
    kochen_patch_t *patch
) {
    for (uint8_t s = 0; s < arch->slot_count && patch->slot_count < KOCHEN_MAX_PATCH_SLOTS; s++) {
        kochen_patch_slot_t *ps = &patch->slots[patch->slot_count++];
        ps->component_idx = comp_idx;
        ps->arch_slot_idx = s;
        ps->role = arch->slots[s].role;
        ps->locked = 0;
        ps->ingredient_id = -1;
        ps->mass_g = arch->slots[s].target_frac * batch_g;
    }
}

kochen_status_t kochen_patch_init(
    const kochen_db_t *db,
    kochen_patch_t *patch,
    uint8_t primary_arch_id,
    int8_t companion_arch_id,
    uint8_t pantry_id,
    kochen_mode_t mode
) {
    if (!db || !patch || primary_arch_id >= db->archetype_count || pantry_id >= db->pantry_count)
        return KOCHEN_ERR_RANGE;
    if (companion_arch_id >= (int8_t)db->archetype_count)
        return KOCHEN_ERR_RANGE;

    memset(patch, 0, sizeof(*patch));
    patch->primary_arch_id = primary_arch_id;
    patch->companion_arch_id = companion_arch_id;
    patch->pantry_id = pantry_id;
    patch->solve_mode = mode;
    patch->novelty = 0.0f;
    patch->batch_mass_g = 500.0f;
    patch->seed = 0x5177C0DEu;

    const kochen_archetype_t *pa = &db->archetypes[primary_arch_id];
    for (int a = 0; a < KOCHEN_SENS_COUNT; a++)
        patch->target_sensory[a] = pa->default_sensory[a];

    populate_arch_slots(pa, 0, patch->batch_mass_g, patch);
    if (companion_arch_id >= 0) {
        const kochen_archetype_t *ca = &db->archetypes[companion_arch_id];
        populate_arch_slots(ca, 1, patch->batch_mass_g * 0.32f, patch);
        for (int a = 0; a < KOCHEN_SENS_COUNT; a++)
            patch->target_sensory[a] = 0.8f * pa->default_sensory[a] + 0.2f * ca->default_sensory[a];
    }
    return KOCHEN_OK;
}

kochen_status_t kochen_patch_set_archetypes(
    const kochen_db_t *db,
    kochen_patch_t *patch,
    uint8_t primary_arch_id,
    int8_t companion_arch_id
) {
    if (!db || !patch || primary_arch_id >= db->archetype_count || companion_arch_id >= (int8_t)db->archetype_count)
        return KOCHEN_ERR_RANGE;

    kochen_patch_slot_t saved[KOCHEN_MAX_PATCH_SLOTS];
    uint8_t saved_n = patch->slot_count;
    memcpy(saved, patch->slots, sizeof(saved));

    patch->primary_arch_id = primary_arch_id;
    patch->companion_arch_id = companion_arch_id;
    patch->slot_count = 0;
    populate_arch_slots(&db->archetypes[primary_arch_id], 0, patch->batch_mass_g, patch);
    if (companion_arch_id >= 0)
        populate_arch_slots(&db->archetypes[companion_arch_id], 1, patch->batch_mass_g * 0.32f, patch);

    for (uint8_t o = 0; o < saved_n; o++) {
        if (!saved[o].locked || saved[o].ingredient_id < 0) continue;
        for (uint8_t n = 0; n < patch->slot_count; n++) {
            if (!patch->slots[n].locked && patch->slots[n].role == saved[o].role &&
                patch->slots[n].component_idx == saved[o].component_idx) {
                patch->slots[n].locked = 1;
                patch->slots[n].ingredient_id = saved[o].ingredient_id;
                break;
            }
        }
    }
    return KOCHEN_OK;
}

static bool ingredient_eligible(
    const kochen_ingredient_t *ing,
    const kochen_archetype_t *arch,
    const kochen_slot_spec_t *spec,
    const kochen_patch_t *patch
) {
    if ((ing->role_mask & (1u << spec->role)) == 0) return false;
    if ((ing->diet_mask & patch->forbidden_diet_mask) != 0) return false;
    if (patch->solve_mode == KOCHEN_MODE_HOME && (ing->diet_mask & KOCHEN_DIET_MODERNIST) != 0) return false;
    if ((arch->guard_flags & KOCHEN_GUARD_FOAM_NO_LIPID) && spec->stage <= KOCHEN_STAGE_CORE_COOK &&
        spec->role != KOCHEN_ROLE_FAT_MEDIUM && ing->fat_pct > 2.5f) return false;
    if (arch->max_ph <= 4.3f && spec->role == KOCHEN_ROLE_ACID_BRIGHTENER && ing->ph > 3.0f) return false;
    if (spec->role == KOCHEN_ROLE_AQUEOUS_BASE && spec->target_frac >= 0.25f && ing->ph < arch->min_ph - 0.15f)
        return false;
    if (strcmp(arch->slug, "coagulated_curd") == 0 && spec->role == KOCHEN_ROLE_AQUEOUS_BASE && ing->protein_pct < 2.5f)
        return false;
    if (arch->max_gel_protein_pct > 0.0f && arch->target_temp_c >= 70.0f &&
        spec->role == KOCHEN_ROLE_HYDROCOLLOID_GEL && ing->gel_set_c < 50.0f)
        return false;
    if (spec->role == KOCHEN_ROLE_SWEET_CARAMEL && spec->target_frac >= 0.15f && ing->ph < arch->min_ph - 0.10f)
        return false;
    if (arch->min_dispersed_frac >= 0.40f && spec->role == KOCHEN_ROLE_FAT_MEDIUM && ing->fat_pct < 75.0f)
        return false;
    if (arch->max_hydration_ratio > 0.0f && spec->role == KOCHEN_ROLE_STARCH_MATRIX &&
        ing->water_pct > arch->max_hydration_ratio * ing->starch_pct * 0.75f)
        return false;
    return true;
}

static float score_candidate(
    const kochen_ingredient_t *ing,
    const kochen_archetype_t *arch,
    const kochen_slot_spec_t *spec,
    const kochen_patch_t *patch,
    uint8_t comp_idx,
    uint32_t anchor_vol_mask
) {
    float alpha = fminf(1.0f, fmaxf(0.0f, patch->novelty));
    uint32_t n_pan = kochen_popcount32(ing->pantry_mask);
    float pantry_s = 0.0f;
    if (ing->pantry_mask & (1u << patch->pantry_id)) {
        pantry_s = (n_pan <= 3) ? 1.0f : (n_pan <= 7 ? 0.84f : 0.68f);
    } else if (n_pan >= 9) {
        pantry_s = 0.26f;
    } else if (ing->diet_mask & KOCHEN_DIET_MODERNIST) {
        pantry_s = (patch->solve_mode == KOCHEN_MODE_PRO) ? 0.55f : 0.0f;
    }

    float bridge_s = 0.45f;
    if (anchor_vol_mask != 0 && ing->volatile_mask != 0) {
        float ov = kochen_volatile_jaccard(ing->volatile_mask, anchor_vol_mask);
        if (spec->stage == KOCHEN_STAGE_LATE_FINISH) {
            uint32_t lift = ing->volatile_mask & (KOCHEN_VOL_TERPENES_CITRUS | KOCHEN_VOL_TERPENES_HERBAL |
                                                  KOCHEN_VOL_ALDEHYDES_GREEN | KOCHEN_VOL_ESTERS_FRUITY);
            bridge_s = 0.45f * ov + (lift ? 0.55f : 0.15f);
        } else {
            bridge_s = 0.25f + 0.75f * ov;
        }
    }

    float role_s = 0.5f;
    if (spec->role == KOCHEN_ROLE_SURFACTANT_BINDER && arch->min_surfactant_ratio > 0.0f)
        role_s = fminf(1.0f, 0.25f + 0.30f * ing->surfactant_power + 0.15f * ing->hydrocolloid_power);
    else if (spec->role == KOCHEN_ROLE_HYDROCOLLOID_GEL && arch->max_gel_protein_pct > 0.0f) {
        bool gelling = (ing->prot_class == KOCHEN_PROT_OVALBUMIN || ing->prot_class == KOCHEN_PROT_PLANT_GLOBULIN ||
                        ing->prot_class == KOCHEN_PROT_CASEIN || ing->gel_set_c > 0.0f);
        role_s = gelling ? 0.95f : 0.10f;
    } else if (spec->role == KOCHEN_ROLE_PROTEIN_COLLAGEN)
        role_s = (ing->prot_class == KOCHEN_PROT_COLLAGEN || ing->fiber_pectin_pct > 2.0f) ? 0.95f : 0.35f;
    else if (spec->role == KOCHEN_ROLE_ACID_BRIGHTENER)
        role_s = fminf(1.0f, fmaxf(0.1f, (6.0f - ing->ph) * 0.28f));

    /* Sensory context fit (distinguishes savory vs pastry and heat/richness targets) */
    float sens_fit = 0.0f;
    bool pastry_profile = (patch->target_sensory[KOCHEN_SENS_SWEET_BITTER] >= 0.45f &&
                           patch->target_sensory[KOCHEN_SENS_UMAMI] <= 0.18f);
    if (pastry_profile) {
        if (ing->volatile_mask & KOCHEN_VOL_SULFIDES_ALLIUM) sens_fit -= 0.85f * (1.0f - 0.5f * alpha);
        if (ing->glutamate_mg > 180.0f || (ing->diet_mask & (KOCHEN_DIET_MEAT | KOCHEN_DIET_SEAFOOD)))
            sens_fit -= 0.80f * (1.0f - 0.4f * alpha);
        if (ing->volatile_mask & (KOCHEN_VOL_LACTONES | KOCHEN_VOL_PHENOLS_SPICE | KOCHEN_VOL_FURANS_CARAMEL))
            sens_fit += 0.25f;
    } else if (patch->target_sensory[KOCHEN_SENS_SWEET_BITTER] < 0.25f && spec->role != KOCHEN_ROLE_SWEET_CARAMEL) {
        if (ing->sugar_pct >= 18.0f) sens_fit -= 0.65f;
        if ((ing->volatile_mask & KOCHEN_VOL_PHENOLS_SPICE) && patch->target_sensory[KOCHEN_SENS_UMAMI] < 0.35f &&
            ing->pungency_idx < 0.05f) sens_fit -= 0.25f;
    }

    float heat_tgt = patch->target_sensory[KOCHEN_SENS_HEAT];
    if (ing->pungency_idx > 0.25f && heat_tgt < 0.12f) sens_fit -= 0.45f;
    if (heat_tgt > 0.45f && (spec->role == KOCHEN_ROLE_AROMA_EARLY_FAT || spec->role == KOCHEN_ROLE_UMAMI_FERMENT ||
                             spec->role == KOCHEN_ROLE_FAT_MEDIUM))
        sens_fit += 0.32f * ing->pungency_idx;

    if (spec->role == KOCHEN_ROLE_AQUEOUS_BASE) {
        float rich_tgt = patch->target_sensory[KOCHEN_SENS_RICHNESS];
        sens_fit += 0.25f * (1.0f - fabsf(rich_tgt - fminf(1.0f, ing->fat_pct * 0.03f)));
        if (patch->target_sensory[KOCHEN_SENS_UMAMI] > 0.45f && (ing->glutamate_mg + ing->nucleotide_mg > 40.0f))
            sens_fit += 0.25f;
        if (arch->min_surfactant_ratio > 0.0f && ing->starch_pct > 1.0f && ing->water_pct > 85.0f)
            sens_fit += 0.28f;
    }

    /* Discourage duplicate ingredient within the same component */
    for (uint8_t k = 0; k < patch->slot_count; k++) {
        if (patch->slots[k].component_idx == comp_idx && patch->slots[k].ingredient_id == (int16_t)ing->id)
            sens_fit -= 1.2f;
    }

    return (1.0f - alpha) * 1.15f * pantry_s + alpha * 0.95f * bridge_s + 0.55f * role_s + sens_fit;
}

static float component_loss(
    const kochen_db_t *db,
    const kochen_archetype_t *arch,
    const kochen_patch_t *patch,
    uint8_t comp_idx
) {
    kochen_diagnostics_t d;
    kochen_physics_eval_component(db, arch, patch, comp_idx, &d);
    float loss = 0.0f;
    for (int a = 0; a < KOCHEN_SENS_COUNT; a++) {
        float diff = d.sensory[a] - patch->target_sensory[a];
        loss += 1.4f * diff * diff;
    }
    if (arch->max_hydration_ratio > 0.0f) {
        if (d.hydration_ratio < arch->min_hydration_ratio) {
            float e = arch->min_hydration_ratio - d.hydration_ratio; loss += 36.0f * e * e;
        } else if (d.hydration_ratio > arch->max_hydration_ratio) {
            float e = d.hydration_ratio - arch->max_hydration_ratio; loss += 36.0f * e * e;
        }
    }
    if (arch->min_surfactant_ratio > 0.0f) {
        if (d.surfactant_coverage < 1.02f) {
            float e = 1.02f - d.surfactant_coverage; loss += 45.0f * e * e;
        }
        if (d.dispersed_fat_frac < arch->min_dispersed_frac) {
            float e = arch->min_dispersed_frac - d.dispersed_fat_frac; loss += 42.0f * e * e;
        } else if (d.dispersed_fat_frac > arch->max_dispersed_frac) {
            float e = d.dispersed_fat_frac - arch->max_dispersed_frac; loss += 42.0f * e * e;
        }
    }
    if (arch->max_gel_protein_pct > 0.0f) {
        if (d.gel_protein_pct < arch->min_gel_protein_pct) {
            float e = arch->min_gel_protein_pct - d.gel_protein_pct; loss += 40.0f * e * e;
        } else if (d.gel_protein_pct > arch->max_gel_protein_pct) {
            float e = d.gel_protein_pct - arch->max_gel_protein_pct; loss += 40.0f * e * e;
        }
        if (d.nacl_pct < 0.02f) {
            float e = 0.02f - d.nacl_pct; loss += 50.0f * e * e;
        }
    }
    if (d.mixture_ph < arch->min_ph) {
        float e = arch->min_ph - d.mixture_ph; loss += 38.0f * e * e;
    } else if (d.mixture_ph > arch->max_ph) {
        float e = d.mixture_ph - arch->max_ph; loss += 38.0f * e * e;
    }
    bool pastry_prof = (patch->target_sensory[KOCHEN_SENS_SWEET_BITTER] >= 0.45f &&
                        patch->target_sensory[KOCHEN_SENS_UMAMI] <= 0.18f);
    float max_salt = pastry_prof ? 0.55f : ((arch->max_ph <= 4.3f) ? 6.0f : 2.5f);
    if (d.nacl_pct > max_salt) {
        float e = d.nacl_pct - max_salt; loss += 30.0f * e * e;
    }
    return loss;
}

static void optimize_component_masses(
    const kochen_db_t *db,
    const kochen_archetype_t *arch,
    kochen_patch_t *patch,
    uint8_t comp_idx,
    float target_batch_g
) {
    /* Initialize unlocked slots within bounds and scale toward target_batch_g */
    float locked_sum = 0.0f, unlocked_sum = 0.0f;
    for (uint8_t i = 0; i < patch->slot_count; i++) {
        kochen_patch_slot_t *s = &patch->slots[i];
        if (s->component_idx != comp_idx) continue;
        if (s->ingredient_id < 0) { s->mass_g = 0.0f; continue; }
        const kochen_slot_spec_t *sp = &arch->slots[s->arch_slot_idx];
        if (s->locked && s->mass_g > 0.1f) {
            locked_sum += s->mass_g;
        } else {
            s->mass_g = sp->target_frac * target_batch_g;
            unlocked_sum += s->mass_g;
        }
    }
    if (unlocked_sum > 0.1f && target_batch_g > locked_sum + 5.0f) {
        float scale = (target_batch_g - locked_sum) / unlocked_sum;
        for (uint8_t i = 0; i < patch->slot_count; i++) {
            kochen_patch_slot_t *s = &patch->slots[i];
            if (s->component_idx != comp_idx || s->ingredient_id < 0 || s->locked) continue;
            const kochen_slot_spec_t *sp = &arch->slots[s->arch_slot_idx];
            s->mass_g = fminf(sp->max_frac * target_batch_g, fmaxf(sp->min_frac * target_batch_g, s->mass_g * scale));
        }
    }

    float best_loss = component_loss(db, arch, patch, comp_idx);
    for (int iter = 0; iter < 64; iter++) {
        float step = target_batch_g * (0.09f * powf(0.93f, (float)iter));
        bool improved = false;
        for (uint8_t i = 0; i < patch->slot_count; i++) {
            kochen_patch_slot_t *s = &patch->slots[i];
            if (s->component_idx != comp_idx || s->ingredient_id < 0 || s->locked) continue;
            const kochen_slot_spec_t *sp = &arch->slots[s->arch_slot_idx];
            float lo = sp->min_frac * target_batch_g;
            float hi = sp->max_frac * target_batch_g;
            float orig = s->mass_g;

            for (int dir = -1; dir <= 2; dir++) {
                if (dir == 0) continue;
                float cand = (dir == 2) ? lo : fminf(hi, fmaxf(lo, orig + (float)dir * step));
                if (fabsf(cand - orig) < 0.05f) continue;
                s->mass_g = cand;
                float l = component_loss(db, arch, patch, comp_idx);
                if (l + 1e-6f < best_loss) {
                    best_loss = l;
                    orig = cand;
                    improved = true;
                } else {
                    s->mass_g = orig;
                }
            }
        }
        if (!improved && step < target_batch_g * 0.008f) break;
    }
}

static kochen_status_t solve_component(
    const kochen_db_t *db,
    const kochen_archetype_t *arch,
    kochen_patch_t *patch,
    uint8_t comp_idx,
    float batch_g,
    uint64_t *rng
) {
    uint32_t anchor_vol = 0;
    for (uint8_t i = 0; i < patch->slot_count; i++) {
        const kochen_patch_slot_t *s = &patch->slots[i];
        if (s->component_idx == comp_idx && s->locked && s->ingredient_id >= 0)
            anchor_vol |= db->ingredients[s->ingredient_id].volatile_mask;
    }

    for (uint8_t i = 0; i < patch->slot_count; i++) {
        kochen_patch_slot_t *s = &patch->slots[i];
        if (s->component_idx != comp_idx) continue;
        const kochen_slot_spec_t *spec = &arch->slots[s->arch_slot_idx];
        if (s->locked && s->ingredient_id >= 0) continue;

        int best_id = -1;
        float best_sc = -1e9f;
        for (uint16_t g = 0; g < db->ingredient_count; g++) {
            const kochen_ingredient_t *ing = &db->ingredients[g];
            if (!ingredient_eligible(ing, arch, spec, patch)) continue;
            float sc = score_candidate(ing, arch, spec, patch, comp_idx, anchor_vol);
            if (patch->novelty > 0.01f)
                sc += patch->novelty * 0.24f * (rand01(rng) - 0.5f);
            if (sc > best_sc) {
                best_sc = sc;
                best_id = (int)g;
            }
        }

        if (best_id < 0) {
            if (spec->optional) {
                s->ingredient_id = -1;
                s->mass_g = 0.0f;
                continue;
            }
            return KOCHEN_ERR_INFEASIBLE;
        }
        s->ingredient_id = (int16_t)best_id;
        anchor_vol |= db->ingredients[best_id].volatile_mask;
    }

    optimize_component_masses(db, arch, patch, comp_idx, batch_g);
    return KOCHEN_OK;
}

kochen_status_t kochen_patch_rebalance_masses(const kochen_db_t *db, kochen_patch_t *patch) {
    if (!db || !patch || patch->primary_arch_id >= db->archetype_count) return KOCHEN_ERR_RANGE;
    float base_g = fmaxf(50.0f, patch->batch_mass_g);
    optimize_component_masses(db, &db->archetypes[patch->primary_arch_id], patch, 0, base_g);
    if (patch->companion_arch_id >= 0 && patch->companion_arch_id < (int8_t)db->archetype_count)
        optimize_component_masses(db, &db->archetypes[patch->companion_arch_id], patch, 1, base_g * 0.32f);
    return KOCHEN_OK;
}

kochen_status_t kochen_patch_solve(const kochen_db_t *db, kochen_patch_t *patch) {
    if (!db || !patch || patch->primary_arch_id >= db->archetype_count) return KOCHEN_ERR_RANGE;
    uint64_t rng = ((uint64_t)patch->seed << 32) | (0x9E3779B9u ^ patch->pantry_id);
    pcg32_next(&rng);

    float base_g = fmaxf(50.0f, patch->batch_mass_g);
    kochen_status_t st = solve_component(db, &db->archetypes[patch->primary_arch_id], patch, 0, base_g, &rng);
    if (st != KOCHEN_OK) return st;

    if (patch->companion_arch_id >= 0 && patch->companion_arch_id < (int8_t)db->archetype_count) {
        st = solve_component(db, &db->archetypes[patch->companion_arch_id], patch, 1, base_g * 0.32f, &rng);
        if (st != KOCHEN_OK) return st;
    }
    return KOCHEN_OK;
}

kochen_status_t kochen_patch_reroll_slot(const kochen_db_t *db, kochen_patch_t *patch, uint8_t slot_idx) {
    if (!db || !patch || slot_idx >= patch->slot_count) return KOCHEN_ERR_RANGE;
    kochen_patch_slot_t *s = &patch->slots[slot_idx];
    const kochen_archetype_t *arch = (s->component_idx == 0)
        ? &db->archetypes[patch->primary_arch_id]
        : &db->archetypes[patch->companion_arch_id];
    const kochen_slot_spec_t *spec = &arch->slots[s->arch_slot_idx];

    patch->seed = patch->seed * 1664525u + 1013904223u;
    uint64_t rng = ((uint64_t)patch->seed << 32) | slot_idx;
    pcg32_next(&rng);

    uint32_t anchor_vol = 0;
    for (uint8_t k = 0; k < patch->slot_count; k++)
        if (k != slot_idx && patch->slots[k].ingredient_id >= 0)
            anchor_vol |= db->ingredients[patch->slots[k].ingredient_id].volatile_mask;

    int prev_id = s->ingredient_id;
    int best_id = -1;
    float best_sc = -1e9f;
    for (uint16_t g = 0; g < db->ingredient_count; g++) {
        if ((int)g == prev_id) continue;
        const kochen_ingredient_t *ing = &db->ingredients[g];
        if (!ingredient_eligible(ing, arch, spec, patch)) continue;
        float sc = score_candidate(ing, arch, spec, patch, s->component_idx, anchor_vol) +
                   0.35f * (rand01(&rng) - 0.5f);
        if (sc > best_sc) { best_sc = sc; best_id = (int)g; }
    }
    if (best_id < 0) best_id = prev_id;
    if (best_id < 0) return KOCHEN_ERR_INFEASIBLE;
    s->ingredient_id = (int16_t)best_id;
    return kochen_patch_rebalance_masses(db, patch);
}

kochen_status_t kochen_resolve_home_pantry(const kochen_db_t *db, kochen_patch_t *patch) {
    if (!db || !patch) return KOCHEN_ERR_RANGE;
    uint8_t saved_locks[KOCHEN_MAX_PATCH_SLOTS];
    for (uint8_t i = 0; i < patch->slot_count; i++) {
        saved_locks[i] = patch->slots[i].locked;
        int16_t id = patch->slots[i].ingredient_id;
        if (id >= 0 && (db->ingredients[id].diet_mask & KOCHEN_DIET_MODERNIST)) {
            patch->slots[i].locked = 0;
            patch->slots[i].ingredient_id = -1;
        } else {
            patch->slots[i].locked = 1;
        }
    }
    patch->solve_mode = KOCHEN_MODE_HOME;
    kochen_status_t st = kochen_patch_solve(db, patch);
    for (uint8_t i = 0; i < patch->slot_count; i++)
        patch->slots[i].locked = saved_locks[i];
    return st;
}
