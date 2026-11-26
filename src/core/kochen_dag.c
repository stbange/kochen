#include "core/kochen_dag.h"
#include "core/kochen_physics.h"
#include <math.h>
#include <string.h>

static int add_node(
    kochen_dag_t *dag,
    uint8_t comp_idx,
    kochen_op_kind_t op,
    float temp_c,
    float dur_sec,
    float out_mass,
    float out_ph
) {
    if (dag->node_count >= KOCHEN_MAX_DAG_NODES) return -1;
    kochen_dag_node_t *n = &dag->nodes[dag->node_count];
    memset(n, 0, sizeof(*n));
    n->node_id = dag->node_count++;
    n->component_idx = comp_idx;
    n->op = op;
    n->temp_c = temp_c;
    n->duration_sec = dur_sec;
    n->out_mass_g = out_mass;
    n->out_ph = out_ph;
    return (int)n->node_id;
}

static void add_parent(kochen_dag_node_t *n, int parent_id) {
    if (parent_id < 0 || n->parent_count >= 4) return;
    for (uint8_t i = 0; i < n->parent_count; i++)
        if (n->parents[i] == (uint8_t)parent_id) return;
    n->parents[n->parent_count++] = (uint8_t)parent_id;
}

static void add_slot_ref(kochen_dag_node_t *n, uint8_t slot_idx) {
    if (n->slot_ref_count < 8) n->slot_refs[n->slot_ref_count++] = slot_idx;
}

static kochen_op_kind_t select_core_op(const kochen_archetype_t *arch) {
    if (strcmp(arch->slug, "collagen_braise") == 0) return KOCHEN_OP_COLLAGEN_BRAISE;
    if (strcmp(arch->slug, "sucrose_caramel") == 0) return KOCHEN_OP_SUCROSE_STAGE;
    if (strcmp(arch->slug, "acid_osmotic_cure") == 0) return KOCHEN_OP_ACID_OSMOTIC_CURE;
    if (strcmp(arch->slug, "crisp_crust_roast") == 0 || strcmp(arch->slug, "low_gluten_batter") == 0)
        return KOCHEN_OP_CRUST_DEHYDRATE_FRY;
    if (arch->max_gel_protein_pct > 0.0f) return KOCHEN_OP_GENTLE_COAGULATE;
    if (arch->min_surfactant_ratio > 0.0f && arch->max_hydration_ratio <= 0.0f)
        return KOCHEN_OP_SHEAR_EMULSIFY;
    return KOCHEN_OP_STARCH_COOK;
}

static int compile_component(
    const kochen_db_t *db,
    const kochen_archetype_t *arch,
    const kochen_patch_t *patch,
    uint8_t comp_idx,
    const kochen_diagnostics_t *diag,
    kochen_dag_t *dag
) {
    uint8_t enz_slots[8], dry_slots[8], bloom_slots[8], core_slots[8], bind_slots[8];
    uint8_t shorten_fat_slots[8], post_acid_slots[8], late_slots[8], foam_slots[8];
    uint8_t n_enz = 0, n_dry = 0, n_bloom = 0, n_core = 0, n_bind = 0;
    uint8_t n_sfat = 0, n_pacid = 0, n_late = 0, n_foam = 0;
    float max_enz_kill = 78.0f;

    bool guard_enz = diag->protease_hazard_raw;
    bool guard_dry = (arch->guard_flags & KOCHEN_GUARD_MAILLARD_DRY) != 0;
    bool guard_pec = diag->pectin_acid_hazard_raw;
    bool guard_lip = ((arch->guard_flags & KOCHEN_GUARD_LIPID_SHORTEN) != 0) &&
                     (diag->fat_g > 0.10f * fmaxf(1.0f, diag->starch_g));
    bool guard_foam = (arch->guard_flags & KOCHEN_GUARD_FOAM_NO_LIPID) != 0;
    bool no_heat = (arch->ceiling_temp_c <= 42.0f);

    for (uint8_t i = 0; i < patch->slot_count; i++) {
        const kochen_patch_slot_t *s = &patch->slots[i];
        if (s->component_idx != comp_idx || s->ingredient_id < 0 || s->mass_g <= 0.05f) continue;
        const kochen_ingredient_t *ing = &db->ingredients[s->ingredient_id];
        const kochen_slot_spec_t *sp = &arch->slots[s->arch_slot_idx];

        if (guard_enz && (ing->enzyme_mask & KOCHEN_ENZ_PROTEASE) && n_enz < 8) {
            enz_slots[n_enz++] = i;
            if (ing->enzyme_kill_c + 3.0f > max_enz_kill) max_enz_kill = ing->enzyme_kill_c + 3.0f;
            continue;
        }
        if (guard_pec && ing->ph < 5.0f && n_pacid < 8) {
            post_acid_slots[n_pacid++] = i;
            continue;
        }
        if (guard_lip && sp->role == KOCHEN_ROLE_FAT_MEDIUM && n_sfat < 8) {
            shorten_fat_slots[n_sfat++] = i;
            continue;
        }
        if (guard_foam && ing->fat_pct <= 2.5f &&
            (sp->role == KOCHEN_ROLE_SURFACTANT_BINDER || sp->role == KOCHEN_ROLE_HYDROCOLLOID_GEL ||
             sp->role == KOCHEN_ROLE_SWEET_CARAMEL || sp->role == KOCHEN_ROLE_AQUEOUS_BASE) && n_foam < 8) {
            foam_slots[n_foam++] = i;
            continue;
        }
        if (!no_heat && (ing->heat_labile || sp->stage == KOCHEN_STAGE_LATE_FINISH) && n_late < 8) {
            late_slots[n_late++] = i;
            continue;
        }
        if (guard_dry && ing->water_pct > 15.0f &&
            (sp->role == KOCHEN_ROLE_PROTEIN_COLLAGEN || sp->role == KOCHEN_ROLE_PROTEIN_TENDER ||
             (sp->role == KOCHEN_ROLE_STARCH_MATRIX && arch->target_temp_c >= 180.0f)) && n_dry < 8) {
            dry_slots[n_dry++] = i;
            continue;
        }
        if (!no_heat && !guard_foam &&
            (sp->stage == KOCHEN_STAGE_EARLY_FAT ||
             (sp->role == KOCHEN_ROLE_FAT_MEDIUM && arch->min_surfactant_ratio <= 0.0f) ||
             ((ing->solubility_mask & KOCHEN_SOL_FAT) && sp->role == KOCHEN_ROLE_AROMA_EARLY_FAT)) && n_bloom < 8) {
            bloom_slots[n_bloom++] = i;
            continue;
        }
        if (sp->stage == KOCHEN_STAGE_EMULSIFY_BIND && n_bind < 8) {
            bind_slots[n_bind++] = i;
            continue;
        }
        if (n_core < 8) core_slots[n_core++] = i;
    }

    int last_prep = -1, enz_node = -1, sear_node = -1, bloom_node = -1, foam_node = -1;

    if (n_enz > 0) {
        dag->active_guards |= KOCHEN_GUARD_ENZYME_PROTEASE;
        enz_node = add_node(dag, comp_idx, KOCHEN_OP_PRE_SCALD_ENZYME, max_enz_kill, 120.0f, 0.0f, 6.0f);
        if (enz_node >= 0)
            for (uint8_t k = 0; k < n_enz; k++) {
                add_slot_ref(&dag->nodes[enz_node], enz_slots[k]);
                dag->nodes[enz_node].out_mass_g += patch->slots[enz_slots[k]].mass_g;
            }
    }

    if (n_dry > 0) {
        dag->active_guards |= KOCHEN_GUARD_MAILLARD_DRY;
        float m_dry = 0.0f;
        for (uint8_t k = 0; k < n_dry; k++) m_dry += patch->slots[dry_slots[k]].mass_g;
        int pat_id = add_node(dag, comp_idx, KOCHEN_OP_PAT_DRY_BRINE, 20.0f, 600.0f, m_dry * 0.98f, 6.2f);
        if (pat_id >= 0) {
            for (uint8_t k = 0; k < n_dry; k++) add_slot_ref(&dag->nodes[pat_id], dry_slots[k]);
            float sear_t = fmaxf(165.0f, fminf(220.0f, arch->target_temp_c > 150.0f ? arch->target_temp_c : 185.0f));
            sear_node = add_node(dag, comp_idx, KOCHEN_OP_MAILLARD_SEAR, sear_t, 240.0f, m_dry * 0.90f, 6.0f);
            if (sear_node >= 0) add_parent(&dag->nodes[sear_node], pat_id);
        }
    }

    if (n_bloom > 0) {
        float m_bl = 0.0f;
        for (uint8_t k = 0; k < n_bloom; k++) m_bl += patch->slots[bloom_slots[k]].mass_g;
        bloom_node = add_node(dag, comp_idx, KOCHEN_OP_BLOOM_FAT_AROMA, 122.0f, 90.0f, m_bl, 6.4f);
        if (bloom_node >= 0)
            for (uint8_t k = 0; k < n_bloom; k++) add_slot_ref(&dag->nodes[bloom_node], bloom_slots[k]);
    }

    if (n_foam > 0) {
        dag->active_guards |= KOCHEN_GUARD_FOAM_NO_LIPID;
        float m_fm = 0.0f;
        for (uint8_t k = 0; k < n_foam; k++) m_fm += patch->slots[foam_slots[k]].mass_g;
        foam_node = add_node(dag, comp_idx, KOCHEN_OP_WHIP_CLEAN_FOAM, 22.0f, 300.0f, m_fm, diag->mixture_ph);
        if (foam_node >= 0)
            for (uint8_t k = 0; k < n_foam; k++) add_slot_ref(&dag->nodes[foam_node], foam_slots[k]);
    }

    kochen_op_kind_t core_op = guard_pec ? KOCHEN_OP_HYDRATE_NEUTRAL : select_core_op(arch);
    float core_t = arch->target_temp_c;
    if (core_op == KOCHEN_OP_SHEAR_EMULSIFY && diag->casein_temp_capped) {
        core_t = fminf(core_t, 64.0f);
        dag->active_guards |= KOCHEN_GUARD_CASEIN_CEIL;
    }

    int main_node = last_prep;
    if (n_core > 0 || enz_node >= 0 || sear_node >= 0 || bloom_node >= 0 || foam_node < 0) {
        float core_ph = guard_pec ? fmaxf(6.2f, diag->mixture_ph) : diag->mixture_ph;
        main_node = add_node(dag, comp_idx, core_op, core_t, arch->cook_duration_sec, diag->final_mass_g, core_ph);
        if (main_node >= 0) {
            add_parent(&dag->nodes[main_node], enz_node);
            add_parent(&dag->nodes[main_node], sear_node);
            add_parent(&dag->nodes[main_node], bloom_node);
            add_parent(&dag->nodes[main_node], foam_node);
            for (uint8_t k = 0; k < n_core; k++) add_slot_ref(&dag->nodes[main_node], core_slots[k]);
        }
    } else {
        main_node = foam_node;
    }

    int cur_head = main_node;

    if (n_sfat > 0) {
        dag->active_guards |= KOCHEN_GUARD_LIPID_SHORTEN;
        int lip_id = add_node(dag, comp_idx, KOCHEN_OP_LIPID_SHORTEN_KNEAD, 24.0f, 480.0f, diag->final_mass_g, diag->mixture_ph);
        if (lip_id >= 0) {
            add_parent(&dag->nodes[lip_id], cur_head);
            for (uint8_t k = 0; k < n_sfat; k++) add_slot_ref(&dag->nodes[lip_id], shorten_fat_slots[k]);
            cur_head = lip_id;
        }
    }

    if (n_bind > 0) {
        float bind_t = fminf(arch->ceiling_temp_c - 2.0f, arch->target_temp_c);
        if (diag->casein_temp_capped) {
            bind_t = fminf(bind_t, 64.0f);
            dag->active_guards |= KOCHEN_GUARD_CASEIN_CEIL;
        }
        int bind_id = add_node(dag, comp_idx, KOCHEN_OP_SHEAR_EMULSIFY, bind_t, 120.0f, diag->final_mass_g, diag->mixture_ph);
        if (bind_id >= 0) {
            add_parent(&dag->nodes[bind_id], cur_head);
            for (uint8_t k = 0; k < n_bind; k++) add_slot_ref(&dag->nodes[bind_id], bind_slots[k]);
            cur_head = bind_id;
        }
    }

    if (n_pacid > 0) {
        dag->active_guards |= KOCHEN_GUARD_PECTIN_ACID;
        int pa_id = add_node(dag, comp_idx, KOCHEN_OP_POST_SOFTEN_ACID, 85.0f, 180.0f, diag->final_mass_g, diag->mixture_ph);
        if (pa_id >= 0) {
            add_parent(&dag->nodes[pa_id], cur_head);
            for (uint8_t k = 0; k < n_pacid; k++) add_slot_ref(&dag->nodes[pa_id], post_acid_slots[k]);
            cur_head = pa_id;
        }
    }

    if (n_late > 0) {
        int lt_id = add_node(dag, comp_idx, KOCHEN_OP_OFF_HEAT_VOLATILES, 48.0f, 45.0f, diag->final_mass_g, diag->mixture_ph);
        if (lt_id >= 0) {
            add_parent(&dag->nodes[lt_id], cur_head);
            for (uint8_t k = 0; k < n_late; k++) add_slot_ref(&dag->nodes[lt_id], late_slots[k]);
            cur_head = lt_id;
        }
    }

    return cur_head;
}

kochen_status_t kochen_dag_compile(
    const kochen_db_t *db,
    const kochen_patch_t *patch,
    kochen_dag_t *out_dag
) {
    if (!db || !patch || !out_dag || patch->primary_arch_id >= db->archetype_count)
        return KOCHEN_ERR_RANGE;

    memset(out_dag, 0, sizeof(*out_dag));
    kochen_physics_eval_total(db, patch, &out_dag->primary_diag, &out_dag->companion_diag, &out_dag->total_diag);

    const kochen_archetype_t *pa = &db->archetypes[patch->primary_arch_id];
    int p_head = compile_component(db, pa, patch, 0, &out_dag->primary_diag, out_dag);
    if (p_head < 0) return KOCHEN_ERR_CAPACITY;

    if (patch->companion_arch_id >= 0 && patch->companion_arch_id < (int8_t)db->archetype_count &&
        out_dag->companion_diag.total_mass_g > 0.1f) {
        const kochen_archetype_t *ca = &db->archetypes[patch->companion_arch_id];
        int c_head = compile_component(db, ca, patch, 1, &out_dag->companion_diag, out_dag);
        if (c_head < 0) return KOCHEN_ERR_CAPACITY;

        int merge_id = add_node(out_dag, 2, KOCHEN_OP_PLATE_ASSEMBLE, 55.0f, 60.0f,
                                out_dag->total_diag.final_mass_g, out_dag->total_diag.mixture_ph);
        if (merge_id < 0) return KOCHEN_ERR_CAPACITY;
        add_parent(&out_dag->nodes[merge_id], p_head);
        add_parent(&out_dag->nodes[merge_id], c_head);
    }

    return KOCHEN_OK;
}
