#include "core/kochen_physics.h"
#include <math.h>
#include <string.h>

uint32_t kochen_popcount32(uint32_t x) {
    uint32_t c = 0;
    while (x) { x &= (x - 1u); c++; }
    return c;
}

float kochen_volatile_jaccard(uint32_t a, uint32_t b) {
    uint32_t u = kochen_popcount32(a | b);
    return u ? (float)kochen_popcount32(a & b) / (float)u : 0.0f;
}

static float sat01(float x, float k) {
    if (x <= 0.0f) return 0.0f;
    float v = x / (x + k);
    return v > 1.0f ? 1.0f : v;
}

void kochen_physics_eval_component(
    const kochen_db_t *db,
    const kochen_archetype_t *arch,
    const kochen_patch_t *patch,
    uint8_t component_idx,
    kochen_diagnostics_t *d
) {
    memset(d, 0, sizeof(*d));
    if (!arch) return;

    float sat_g = 0.0f, gluten_g = 0.0f, fiber_g = 0.0f, red_sug_g = 0.0f;
    float nacl_g = 0.0f, glut_mg = 0.0f, nuc_mg = 0.0f;
    float surf_cap = 0.0f, hlb_num = 0.0f, hlb_den = 0.0f, hydro_cap = 0.0f;
    float coag_prot_g = 0.0f, hydro_gel_equiv_g = 0.0f;
    float h_ion_num = 0.0f, h_ion_den = 0.0f;
    float pung_sum = 0.0f, bit_sum = 0.0f, crunch_sum = 0.0f, bright_vol = 0.0f;
    bool has_protease = false, has_gel_sensitive = false, has_acid = false;
    bool has_casein = false, has_chelator = false;
    uint8_t vol_counts[16] = {0};

    for (uint8_t i = 0; i < patch->slot_count; i++) {
        const kochen_patch_slot_t *s = &patch->slots[i];
        if (s->component_idx != component_idx || s->ingredient_id < 0 || s->mass_g <= 0.01f) continue;
        const kochen_ingredient_t *ing = &db->ingredients[s->ingredient_id];
        float m = s->mass_g;
        d->total_mass_g += m;
        d->water_g   += m * ing->water_pct * 0.01f;
        d->fat_g     += m * ing->fat_pct * 0.01f;
        sat_g        += m * ing->fat_sat_pct * 0.01f;
        d->protein_g += m * ing->protein_pct * 0.01f;
        d->starch_g  += m * ing->starch_pct * 0.01f;
        d->sugar_g   += m * ing->sugar_pct * 0.01f;
        red_sug_g    += m * ing->sugar_pct * ing->reducing_sugar_frac * 0.01f;
        fiber_g      += m * ing->fiber_pectin_pct * 0.01f;
        nacl_g       += m * ing->nacl_pct * 0.01f;
        glut_mg      += m * ing->glutamate_mg * 0.01f;
        nuc_mg       += m * ing->nucleotide_mg * 0.01f;

        bool is_curd_arch = (strcmp(arch->slug, "coagulated_curd") == 0);
        if (ing->prot_class == KOCHEN_PROT_GLUTEN) gluten_g += m * ing->protein_pct * 0.01f;
        if (ing->prot_class == KOCHEN_PROT_OVALBUMIN || ing->prot_class == KOCHEN_PROT_WHEY ||
            ing->prot_class == KOCHEN_PROT_PLANT_GLOBULIN ||
            (is_curd_arch && ing->prot_class == KOCHEN_PROT_CASEIN) ||
            (ing->prot_class == KOCHEN_PROT_COLLAGEN && ing->gel_set_c > 0.0f)) {
            coag_prot_g += m * ing->protein_pct * 0.01f;
            has_gel_sensitive = true;
        }
        if (ing->prot_class == KOCHEN_PROT_COLLAGEN) has_gel_sensitive = true;
        if (ing->prot_class == KOCHEN_PROT_CASEIN) has_casein = true;
        if (ing->enzyme_mask & KOCHEN_ENZ_PROTEASE) has_protease = true;
        if (ing->enzyme_mask & KOCHEN_ENZ_CHELATOR) has_chelator = true;
        if (ing->ph < 5.0f && m >= 2.0f) has_acid = true;
        if (ing->diet_mask & KOCHEN_DIET_MODERNIST) d->has_modernist_additive = true;

        surf_cap += m * ing->surfactant_power;
        if (ing->surfactant_power > 0.01f) {
            hlb_num += m * ing->surfactant_power * ing->surfactant_hlb;
            hlb_den += m * ing->surfactant_power;
        }
        hydro_cap += m * ing->hydrocolloid_power;
        if (ing->prot_class == KOCHEN_PROT_NONE && ing->gel_set_c > 0.0f && ing->hydrocolloid_power >= 2.0f) {
            hydro_gel_equiv_g += m * ing->hydrocolloid_power * 0.14f;
            has_gel_sensitive = true;
        }

        float aq_factor = (ing->water_pct > 2.0f || (ing->solubility_mask & KOCHEN_SOL_WATER)) ? 1.0f : 0.02f;
        if (ing->ph < 4.5f) aq_factor *= 1.5f;
        float w_buf = m * ing->buffer_capacity * aq_factor;
        h_ion_num += w_buf * powf(10.0f, -0.55f * ing->ph);
        h_ion_den += w_buf;

        pung_sum   += m * ing->pungency_idx;
        bit_sum    += m * ing->bitterness_idx;
        crunch_sum += m * ing->crunch_potential;
        if (ing->volatile_mask & (KOCHEN_VOL_TERPENES_CITRUS | KOCHEN_VOL_ALDEHYDES_GREEN))
            bright_vol += m * 0.35f;

        for (int b = 0; b < 16; b++)
            if (ing->volatile_mask & (1u << b)) vol_counts[b]++;
    }

    if (d->total_mass_g <= 0.1f) return;

    float evap = fminf(d->water_g * 0.85f, d->water_g * arch->evap_loss_frac);
    float final_water = d->water_g - evap;
    d->final_mass_g = fmaxf(1.0f, d->total_mass_g - evap);
    d->nacl_pct = 100.0f * nacl_g / d->final_mass_g;

    float raw_ph = (h_ion_den > 1e-9f) ? (-log10f(h_ion_num / h_ion_den) / 0.55f) : 7.0f;
    d->mixture_ph = fminf(9.5f, fmaxf(1.8f, raw_ph));

    float dry_struct = d->starch_g + gluten_g + fiber_g * 0.6f;
    d->hydration_ratio = (dry_struct > 0.5f) ? (d->water_g / dry_struct) : 0.0f;
    d->dispersed_fat_frac = (d->fat_g + final_water > 0.5f) ? (d->fat_g / (d->fat_g + final_water)) : 0.0f;
    d->mean_hlb = (hlb_den > 1e-5f) ? (hlb_num / hlb_den) : 0.0f;

    float eff_surf = surf_cap + 0.20f * d->starch_g + 0.35f * hydro_cap;
    if (arch->min_surfactant_ratio > 0.0f && d->fat_g > 0.5f)
        d->surfactant_coverage = eff_surf / (arch->min_surfactant_ratio * d->fat_g);
    else
        d->surfactant_coverage = 1.0f;

    d->gel_protein_pct = 100.0f * (coag_prot_g + hydro_gel_equiv_g) / d->final_mass_g;

    /* Yamaguchi synergistic umami (g/100g equivalent) */
    float g_conc = (glut_mg / d->final_mass_g) * 0.1f;
    float n_conc = (nuc_mg / d->final_mass_g) * 0.1f;
    d->umami_synergy = g_conc + 12.18f * g_conc * n_conc;

    float maillard_bonus = ((arch->guard_flags & KOCHEN_GUARD_MAILLARD_DRY) && d->protein_g > 2.0f)
                           ? (0.12f + 0.08f * sat01(red_sug_g, 3.0f)) : 0.0f;

    d->sensory[KOCHEN_SENS_RICHNESS]     = sat01((d->fat_g + 0.35f * sat_g) / d->final_mass_g, 0.16f);
    d->sensory[KOCHEN_SENS_BRIGHTNESS]   = sat01(fmaxf(0.0f, 6.5f - d->mixture_ph) + bright_vol / d->final_mass_g, 1.35f);
    d->sensory[KOCHEN_SENS_UMAMI]        = fminf(1.0f, sat01(d->umami_synergy, 0.28f) + maillard_bonus);
    d->sensory[KOCHEN_SENS_HEAT]         = sat01(pung_sum / d->final_mass_g, 0.045f);
    d->sensory[KOCHEN_SENS_SWEET_BITTER] = sat01((d->sugar_g + 25.0f * bit_sum) / d->final_mass_g, 0.12f);
    d->sensory[KOCHEN_SENS_CRUNCH]       = fminf(1.0f, sat01(crunch_sum / d->final_mass_g, 0.08f) +
                                                 (arch->target_temp_c >= 175.0f ? 0.22f : 0.0f));

    d->ph_in_window = (d->mixture_ph >= arch->min_ph - 0.06f && d->mixture_ph <= arch->max_ph + 0.06f);
    d->hydration_in_window = (arch->max_hydration_ratio <= 0.0f) ||
                             (d->hydration_ratio >= arch->min_hydration_ratio * 0.92f &&
                              d->hydration_ratio <= arch->max_hydration_ratio * 1.08f);
    d->emulsion_stable = (arch->min_surfactant_ratio <= 0.0f) ||
                         (d->surfactant_coverage >= 0.95f &&
                          d->dispersed_fat_frac >= arch->min_dispersed_frac - 0.04f &&
                          d->dispersed_fat_frac <= arch->max_dispersed_frac + 0.04f);
    d->gel_in_window = (arch->max_gel_protein_pct <= 0.0f) ||
                       (d->gel_protein_pct >= arch->min_gel_protein_pct * 0.90f &&
                        d->gel_protein_pct <= arch->max_gel_protein_pct * 1.12f &&
                        (coag_prot_g <= hydro_gel_equiv_g || d->nacl_pct >= 0.02f));

    d->protease_hazard_raw = has_protease && ((arch->guard_flags & KOCHEN_GUARD_ENZYME_PROTEASE) || has_gel_sensitive);
    d->pectin_acid_hazard_raw = has_acid && ((arch->guard_flags & KOCHEN_GUARD_PECTIN_ACID) != 0);
    d->casein_temp_capped = has_casein && !has_chelator && ((arch->guard_flags & KOCHEN_GUARD_CASEIN_CEIL) != 0);

    for (int b = 0; b < 16; b++)
        if (vol_counts[b] >= 2) d->shared_volatile_mask |= (1u << b);
}

void kochen_physics_eval_total(
    const kochen_db_t *db,
    const kochen_patch_t *patch,
    kochen_diagnostics_t *primary_out,
    kochen_diagnostics_t *companion_out,
    kochen_diagnostics_t *total_out
) {
    const kochen_archetype_t *pa = (patch->primary_arch_id < db->archetype_count)
                                   ? &db->archetypes[patch->primary_arch_id] : NULL;
    const kochen_archetype_t *ca = (patch->companion_arch_id >= 0 && patch->companion_arch_id < db->archetype_count)
                                   ? &db->archetypes[patch->companion_arch_id] : NULL;

    kochen_diagnostics_t p_diag, c_diag;
    kochen_physics_eval_component(db, pa, patch, 0, &p_diag);
    kochen_physics_eval_component(db, ca, patch, 1, &c_diag);

    if (primary_out) *primary_out = p_diag;
    if (companion_out) *companion_out = c_diag;
    if (!total_out) return;

    if (!ca || c_diag.total_mass_g <= 0.1f) {
        *total_out = p_diag;
        return;
    }

    *total_out = p_diag;
    float wp = p_diag.final_mass_g, wc = c_diag.final_mass_g, wt = fmaxf(1.0f, wp + wc);
    total_out->total_mass_g = p_diag.total_mass_g + c_diag.total_mass_g;
    total_out->final_mass_g = wt;
    total_out->water_g = p_diag.water_g + c_diag.water_g;
    total_out->fat_g = p_diag.fat_g + c_diag.fat_g;
    total_out->protein_g = p_diag.protein_g + c_diag.protein_g;
    total_out->starch_g = p_diag.starch_g + c_diag.starch_g;
    total_out->sugar_g = p_diag.sugar_g + c_diag.sugar_g;
    total_out->nacl_pct = (p_diag.nacl_pct * wp + c_diag.nacl_pct * wc) / wt;
    total_out->umami_synergy = (p_diag.umami_synergy * wp + c_diag.umami_synergy * wc) / wt;
    for (int a = 0; a < KOCHEN_SENS_COUNT; a++)
        total_out->sensory[a] = (p_diag.sensory[a] * wp + c_diag.sensory[a] * wc) / wt;

    total_out->emulsion_stable = p_diag.emulsion_stable && c_diag.emulsion_stable;
    total_out->ph_in_window = p_diag.ph_in_window && c_diag.ph_in_window;
    total_out->hydration_in_window = p_diag.hydration_in_window && c_diag.hydration_in_window;
    total_out->gel_in_window = p_diag.gel_in_window && c_diag.gel_in_window;
    total_out->protease_hazard_raw = p_diag.protease_hazard_raw || c_diag.protease_hazard_raw;
    total_out->pectin_acid_hazard_raw = p_diag.pectin_acid_hazard_raw || c_diag.pectin_acid_hazard_raw;
    total_out->casein_temp_capped = p_diag.casein_temp_capped || c_diag.casein_temp_capped;
    total_out->has_modernist_additive = p_diag.has_modernist_additive || c_diag.has_modernist_additive;
    total_out->shared_volatile_mask = p_diag.shared_volatile_mask | c_diag.shared_volatile_mask;
}
