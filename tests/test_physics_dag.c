#include "tests/test_harness.h"
#include "core/kochen_dag.h"
#include "core/kochen_data.h"
#include "core/kochen_physics.h"
#include "core/kochen_solver.h"

static bool dag_has_op(const kochen_dag_t *dag, kochen_op_kind_t op, int *out_idx) {
    for (uint8_t i = 0; i < dag->node_count; i++) {
        if (dag->nodes[i].op == op) {
            if (out_idx) *out_idx = (int)i;
            return true;
        }
    }
    return false;
}

static void assert_valid_dag_topology(const kochen_patch_t *patch, const kochen_dag_t *dag) {
    ASSERT_TRUE(dag->node_count >= 1);
    uint8_t seen_slot[KOCHEN_MAX_PATCH_SLOTS] = {0};
    for (uint8_t n = 0; n < dag->node_count; n++) {
        const kochen_dag_node_t *nd = &dag->nodes[n];
        ASSERT_EQ_INT(nd->node_id, n);
        for (uint8_t p = 0; p < nd->parent_count; p++)
            ASSERT_TRUE(nd->parents[p] < nd->node_id);
        for (uint8_t r = 0; r < nd->slot_ref_count; r++) {
            uint8_t si = nd->slot_refs[r];
            ASSERT_TRUE(si < patch->slot_count);
            seen_slot[si]++;
        }
    }
    for (uint8_t i = 0; i < patch->slot_count; i++) {
        if (patch->slots[i].ingredient_id >= 0 && patch->slots[i].mass_g > 0.05f)
            ASSERT_EQ_INT(seen_slot[i], 1);
    }
}

static void test_umami_synergy_and_ph(const kochen_db_t *db) {
    int cust_arch = kochen_find_archetype(db, "protein_custard");
    int washoku = kochen_find_pantry(db, "washoku");
    kochen_patch_t p;
    ASSERT_EQ_INT(kochen_patch_init(db, &p, (uint8_t)cust_arch, -1, (uint8_t)washoku, KOCHEN_MODE_HOME), KOCHEN_OK);
    p.target_sensory[KOCHEN_SENS_UMAMI] = 0.70f;
    p.target_sensory[KOCHEN_SENS_SWEET_BITTER] = 0.05f;
    ASSERT_EQ_INT(kochen_patch_solve(db, &p), KOCHEN_OK);

    kochen_diagnostics_t d_dashi, d_water;
    kochen_physics_eval_component(db, &db->archetypes[cust_arch], &p, 0, &d_dashi);

    int water_id = kochen_find_ingredient(db, "water");
    p.slots[0].ingredient_id = (int16_t)water_id;
    kochen_physics_eval_component(db, &db->archetypes[cust_arch], &p, 0, &d_water);
    ASSERT_TRUE(d_dashi.umami_synergy > d_water.umami_synergy + 0.05f);

    /* Verify aqueous pH isn't diluted to neutral by anhydrous oil in mayonnaise/aioli */
    int shear_arch = kochen_find_archetype(db, "shear_emulsion");
    ASSERT_EQ_INT(kochen_patch_init(db, &p, (uint8_t)shear_arch, -1, 0, KOCHEN_MODE_HOME), KOCHEN_OK);
    ASSERT_EQ_INT(kochen_patch_solve(db, &p), KOCHEN_OK);
    kochen_diagnostics_t d_emul;
    kochen_physics_eval_component(db, &db->archetypes[shear_arch], &p, 0, &d_emul);
    ASSERT_TRUE(d_emul.mixture_ph < 5.6f);
    ASSERT_TRUE(d_emul.emulsion_stable);
}

static void test_all_six_dag_guards(const kochen_db_t *db) {
    kochen_patch_t p;
    kochen_dag_t dag;
    int scald_i = -1, coag_i = -1, neut_i = -1, acid_i = -1, sear_i = -1, pat_i = -1;

    /* 1. EnzymeGuard: lock raw pineapple into protein_custard */
    int cust_id = kochen_find_archetype(db, "protein_custard");
    int pine_id = kochen_find_ingredient(db, "fresh_pineapple_raw");
    ASSERT_EQ_INT(kochen_patch_init(db, &p, (uint8_t)cust_id, -1, 1, KOCHEN_MODE_HOME), KOCHEN_OK);
    p.slots[4].ingredient_id = (int16_t)pine_id;
    p.slots[4].locked = 1;
    p.slots[4].mass_g = 25.0f;
    ASSERT_EQ_INT(kochen_patch_solve(db, &p), KOCHEN_OK);
    ASSERT_EQ_INT(kochen_dag_compile(db, &p, &dag), KOCHEN_OK);
    assert_valid_dag_topology(&p, &dag);
    ASSERT_TRUE((dag.active_guards & KOCHEN_GUARD_ENZYME_PROTEASE) != 0);
    ASSERT_TRUE(dag_has_op(&dag, KOCHEN_OP_PRE_SCALD_ENZYME, &scald_i));
    ASSERT_TRUE(dag_has_op(&dag, KOCHEN_OP_GENTLE_COAGULATE, &coag_i));
    ASSERT_TRUE(scald_i < coag_i);
    ASSERT_TRUE(dag.nodes[scald_i].temp_c >= 75.0f);

    /* 2. PectinSofteningGuard: lock tamarind into tempered_legume */
    int leg_id = kochen_find_archetype(db, "tempered_legume");
    int tam_id = kochen_find_ingredient(db, "tamarind_pulp_conc");
    ASSERT_EQ_INT(kochen_patch_init(db, &p, (uint8_t)leg_id, -1, 5, KOCHEN_MODE_HOME), KOCHEN_OK);
    p.slots[4].ingredient_id = (int16_t)tam_id;
    p.slots[4].locked = 1;
    p.slots[4].mass_g = 20.0f;
    ASSERT_EQ_INT(kochen_patch_solve(db, &p), KOCHEN_OK);
    ASSERT_EQ_INT(kochen_dag_compile(db, &p, &dag), KOCHEN_OK);
    assert_valid_dag_topology(&p, &dag);
    ASSERT_TRUE((dag.active_guards & KOCHEN_GUARD_PECTIN_ACID) != 0);
    ASSERT_TRUE(dag_has_op(&dag, KOCHEN_OP_HYDRATE_NEUTRAL, &neut_i));
    ASSERT_TRUE(dag_has_op(&dag, KOCHEN_OP_POST_SOFTEN_ACID, &acid_i));
    ASSERT_TRUE(neut_i < acid_i);

    /* 3. MaillardDryGuard on maillard_pan_glaze */
    int sear_arch = kochen_find_archetype(db, "maillard_pan_glaze");
    ASSERT_EQ_INT(kochen_patch_init(db, &p, (uint8_t)sear_arch, -1, 1, KOCHEN_MODE_HOME), KOCHEN_OK);
    ASSERT_EQ_INT(kochen_patch_solve(db, &p), KOCHEN_OK);
    ASSERT_EQ_INT(kochen_dag_compile(db, &p, &dag), KOCHEN_OK);
    assert_valid_dag_topology(&p, &dag);
    ASSERT_TRUE(dag_has_op(&dag, KOCHEN_OP_PAT_DRY_BRINE, &pat_i));
    ASSERT_TRUE(dag_has_op(&dag, KOCHEN_OP_MAILLARD_SEAR, &sear_i));
    ASSERT_TRUE(pat_i < sear_i);

    /* 4. CaseinThermalCeilingGuard (<68C without chelator, relaxed with sodium_citrate) */
    int sfe_id = kochen_find_archetype(db, "starch_fat_emulsion");
    int pec_id = kochen_find_ingredient(db, "pecorino_romano");
    int cit_id = kochen_find_ingredient(db, "sodium_citrate");
    ASSERT_EQ_INT(kochen_patch_init(db, &p, (uint8_t)sfe_id, -1, 0, KOCHEN_MODE_HOME), KOCHEN_OK);
    p.slots[2].ingredient_id = (int16_t)pec_id;
    p.slots[2].locked = 1;
    ASSERT_EQ_INT(kochen_patch_solve(db, &p), KOCHEN_OK);
    ASSERT_EQ_INT(kochen_dag_compile(db, &p, &dag), KOCHEN_OK);
    ASSERT_TRUE((dag.active_guards & KOCHEN_GUARD_CASEIN_CEIL) != 0);

    p.solve_mode = KOCHEN_MODE_PRO;
    p.slots[5].ingredient_id = (int16_t)cit_id;
    p.slots[5].locked = 1;
    p.slots[5].mass_g = 6.0f;
    ASSERT_EQ_INT(kochen_dag_compile(db, &p, &dag), KOCHEN_OK);
    ASSERT_TRUE((dag.active_guards & KOCHEN_GUARD_CASEIN_CEIL) == 0);

    /* 5. LipidShortenGuard on high-butter gluten_dough (Brioche) */
    int dough_id = kochen_find_archetype(db, "gluten_dough");
    int butter_id = kochen_find_ingredient(db, "unsalted_butter");
    ASSERT_EQ_INT(kochen_patch_init(db, &p, (uint8_t)dough_id, -1, 1, KOCHEN_MODE_HOME), KOCHEN_OK);
    p.slots[2].ingredient_id = (int16_t)butter_id;
    p.slots[2].locked = 1;
    p.slots[2].mass_g = 80.0f;
    ASSERT_EQ_INT(kochen_patch_solve(db, &p), KOCHEN_OK);
    ASSERT_EQ_INT(kochen_dag_compile(db, &p, &dag), KOCHEN_OK);
    assert_valid_dag_topology(&p, &dag);
    ASSERT_TRUE((dag.active_guards & KOCHEN_GUARD_LIPID_SHORTEN) != 0);

    /* 6. FoamNoLipidGuard on aerated_foam */
    int foam_id = kochen_find_archetype(db, "aerated_foam");
    ASSERT_EQ_INT(kochen_patch_init(db, &p, (uint8_t)foam_id, -1, 1, KOCHEN_MODE_HOME), KOCHEN_OK);
    ASSERT_EQ_INT(kochen_patch_solve(db, &p), KOCHEN_OK);
    ASSERT_EQ_INT(kochen_dag_compile(db, &p, &dag), KOCHEN_OK);
    assert_valid_dag_topology(&p, &dag);
    ASSERT_TRUE((dag.active_guards & KOCHEN_GUARD_FOAM_NO_LIPID) != 0);
}

int main(void) {
    static kochen_db_t db;
    ASSERT_EQ_INT(kochen_db_load_dir(&db, KOCHEN_SOURCE_DIR "/assets"), KOCHEN_OK);
    test_umami_synergy_and_ph(&db);
    test_all_six_dag_guards(&db);
    printf("test_physics_dag passed (%d assertions)\n", g_tests_run);
    return 0;
}
