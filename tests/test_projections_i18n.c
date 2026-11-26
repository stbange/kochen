#include "tests/test_harness.h"
#include "core/kochen_codec.h"
#include "core/kochen_dag.h"
#include "core/kochen_data.h"
#include "core/kochen_project.h"
#include "core/kochen_solver.h"

static void test_home_pro_identity_and_resolve(const kochen_db_t *db) {
    kochen_patch_t p;
    int sfe_id = kochen_find_archetype(db, "starch_fat_emulsion");
    int shear_id = kochen_find_archetype(db, "shear_emulsion");
    int cit_id = kochen_find_ingredient(db, "sodium_citrate");

    ASSERT_EQ_INT(kochen_patch_init(db, &p, (uint8_t)sfe_id, (int8_t)shear_id, 0, KOCHEN_MODE_PRO), KOCHEN_OK);
    ASSERT_EQ_INT(kochen_patch_solve(db, &p), KOCHEN_OK);
    /* Force modernist sodium_citrate into surfactant slot */
    p.slots[2].ingredient_id = (int16_t)cit_id;
    p.slots[2].mass_g = 12.0f;

    kochen_dag_t dag;
    ASSERT_EQ_INT(kochen_dag_compile(db, &p, &dag), KOCHEN_OK);
    ASSERT_TRUE(dag.total_diag.has_modernist_additive);

    kochen_projection_t proj_pro, proj_home;
    ASSERT_EQ_INT(kochen_project_dag(db, &p, &dag, KOCHEN_MODE_PRO, KOCHEN_LANG_EN, &proj_pro), KOCHEN_OK);
    ASSERT_EQ_INT(kochen_project_dag(db, &p, &dag, KOCHEN_MODE_HOME, KOCHEN_LANG_EN, &proj_home), KOCHEN_OK);

    /* Exact same DAG and slot count; Home mode surfaces modernist resolve badge */
    ASSERT_EQ_INT(proj_pro.slot_count, proj_home.slot_count);
    ASSERT_EQ_INT(proj_pro.step_count, proj_home.step_count);
    ASSERT_EQ_INT(proj_pro.step_count, dag.node_count);
    ASSERT_TRUE(!proj_pro.show_home_resolve_badge);
    ASSERT_TRUE(proj_home.show_home_resolve_badge);
    ASSERT_TRUE(strstr(proj_pro.slots[2].amount_text, "%") != NULL);

    /* Explicit one-click Re-solve for Home Pantry swaps ONLY modernist slot */
    int16_t saved_pasta = p.slots[0].ingredient_id;
    ASSERT_EQ_INT(kochen_resolve_home_pantry(db, &p), KOCHEN_OK);
    ASSERT_EQ_INT(p.slots[0].ingredient_id, saved_pasta);
    ASSERT_TRUE(p.slots[2].ingredient_id != (int16_t)cit_id);
    ASSERT_EQ_INT((db->ingredients[p.slots[2].ingredient_id].diet_mask & KOCHEN_DIET_MODERNIST), 0);

    /* Reroll slot & archetype switch preserving locks */
    p.slots[0].locked = 1;
    ASSERT_EQ_INT(kochen_patch_reroll_slot(db, &p, 3), KOCHEN_OK);
    int ris_id = kochen_find_archetype(db, "starch_hydration");
    ASSERT_EQ_INT(kochen_patch_set_archetypes(db, &p, (uint8_t)ris_id, -1), KOCHEN_OK);
    ASSERT_EQ_INT(p.slots[0].locked, 1);
    ASSERT_EQ_INT(p.slots[0].ingredient_id, saved_pasta);
}

static void test_all_locales_and_markdown(const kochen_db_t *db) {
    /* Verify every locale entry has non-empty EN, DE, and PL text */
    for (uint16_t i = 0; i < db->locale_count; i++) {
        for (int l = 0; l < KOCHEN_LANG_COUNT; l++)
            ASSERT_TRUE(db->locales[i].text[l][0] != '\0');
    }

    kochen_patch_t p;
    kochen_dag_t dag;
    ASSERT_EQ_INT(kochen_patch_init(db, &p, 2, 1, 1, KOCHEN_MODE_HOME), KOCHEN_OK);
    ASSERT_EQ_INT(kochen_patch_solve(db, &p), KOCHEN_OK);
    ASSERT_EQ_INT(kochen_dag_compile(db, &p, &dag), KOCHEN_OK);

    char code[256], md[8192];
    ASSERT_EQ_INT(kochen_patch_encode(&p, code, sizeof(code)), KOCHEN_OK);

    for (int l = 0; l < KOCHEN_LANG_COUNT; l++) {
        kochen_projection_t proj;
        ASSERT_EQ_INT(kochen_project_dag(db, &p, &dag, KOCHEN_MODE_HOME, (kochen_lang_t)l, &proj), KOCHEN_OK);
        size_t n = kochen_export_markdown(db, &p, &dag, &proj, code, md, sizeof(md));
        ASSERT_TRUE(n > 200);
        ASSERT_TRUE(strstr(md, "?patch=") != NULL);
    }
}

int main(void) {
    static kochen_db_t db;
    ASSERT_EQ_INT(kochen_db_load_dir(&db, KOCHEN_SOURCE_DIR "/assets"), KOCHEN_OK);
    test_home_pro_identity_and_resolve(&db);
    test_all_locales_and_markdown(&db);
    printf("test_projections_i18n passed (%d assertions)\n", g_tests_run);
    return 0;
}
