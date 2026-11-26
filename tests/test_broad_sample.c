#include "tests/test_harness.h"
#include "core/kochen_dag.h"
#include "core/kochen_data.h"
#include "core/kochen_physics.h"
#include "core/kochen_project.h"
#include "core/kochen_solver.h"

typedef struct {
    const char *classic_name;
    const char *arch_slug;
    const char *pantry_slug;
    uint32_t diet_filter;
    float rich, bright, umami, heat, sweet;
} classic_case_t;

/* 60 Global Classics: 4 regional classics per each of the 15 structural archetypes */
static const classic_case_t CLASSICS_60[60] = {
    /* 1. starch_fat_emulsion */
    {"Cacio e Pepe",         "starch_fat_emulsion", "italian",      0, 0.62f,0.12f,0.75f,0.45f,0.05f},
    {"Pasta al Limone",      "starch_fat_emulsion", "italian",      0, 0.65f,0.58f,0.50f,0.10f,0.08f},
    {"Sesame Dan Dan",       "starch_fat_emulsion", "sichuan",      0, 0.68f,0.35f,0.72f,0.78f,0.15f},
    {"Miso Butter Udon",     "starch_fat_emulsion", "washoku",      0, 0.58f,0.18f,0.74f,0.12f,0.16f},
    /* 2. shear_emulsion */
    {"Provençal Aioli",      "shear_emulsion",      "french",       0, 0.85f,0.42f,0.30f,0.22f,0.06f},
    {"Classic Hollandaise",  "shear_emulsion",      "french",       0, 0.88f,0.48f,0.25f,0.08f,0.06f},
    {"Levantine Toum",       "shear_emulsion",      "levantine",    KOCHEN_FILTER_VEGAN, 0.86f,0.55f,0.20f,0.30f,0.04f},
    {"Peruvian Huancaina",   "shear_emulsion",      "andean",       0, 0.72f,0.42f,0.38f,0.62f,0.12f},
    /* 3. collagen_braise */
    {"Boeuf Bourguignon",    "collagen_braise",     "french",       0, 0.65f,0.32f,0.82f,0.12f,0.16f},
    {"Hongshao Pork Belly",  "collagen_braise",     "cantonese",    0, 0.75f,0.22f,0.84f,0.18f,0.42f},
    {"Oaxacan Beef Mole",    "collagen_braise",     "mexican",      0, 0.68f,0.28f,0.78f,0.55f,0.44f},
    {"Beef Rendang",         "collagen_braise",     "se_asian",     0, 0.76f,0.26f,0.80f,0.68f,0.28f},
    /* 4. maillard_pan_glaze */
    {"Steak au Poivre",      "maillard_pan_glaze",  "french",       0, 0.62f,0.22f,0.78f,0.52f,0.10f},
    {"Kung Pao Chicken",     "maillard_pan_glaze",  "sichuan",      0, 0.48f,0.42f,0.76f,0.75f,0.34f},
    {"Chicken Teriyaki",     "maillard_pan_glaze",  "washoku",      0, 0.42f,0.20f,0.78f,0.12f,0.48f},
    {"Lomo Saltado Sear",    "maillard_pan_glaze",  "andean",       0, 0.46f,0.44f,0.76f,0.56f,0.16f},
    /* 5. starch_hydration */
    {"Risotto Milanese",     "starch_hydration",    "italian",      0, 0.56f,0.20f,0.68f,0.06f,0.12f},
    {"Cantonese Congee",     "starch_hydration",    "cantonese",    0, 0.32f,0.15f,0.64f,0.25f,0.08f},
    {"Mughlai Pulao/Biryani","starch_hydration",    "north_indian", 0, 0.52f,0.22f,0.58f,0.38f,0.14f},
    {"Levantine Spiced Pilaf","starch_hydration",   "levantine",    0, 0.46f,0.24f,0.52f,0.22f,0.16f},
    /* 6. veloute_suspension */
    {"French Chicken Veloute","veloute_suspension", "french",       0, 0.60f,0.22f,0.62f,0.08f,0.10f},
    {"Shellfish Bisque",     "veloute_suspension",  "french",       0, 0.68f,0.26f,0.76f,0.14f,0.14f},
    {"Nordic Leek Vichyssoise","veloute_suspension","nordic_euro",  0, 0.58f,0.18f,0.48f,0.10f,0.12f},
    {"Levantine Almond Soup","veloute_suspension",  "levantine",    KOCHEN_FILTER_VEGAN, 0.54f,0.34f,0.38f,0.18f,0.12f},
    /* 7. protein_custard */
    {"Chawanmushi",          "protein_custard",     "washoku",      0, 0.22f,0.12f,0.72f,0.05f,0.10f},
    {"Creme Brulee",         "protein_custard",     "french",       0, 0.78f,0.08f,0.05f,0.02f,0.68f},
    {"Savory Gruyere Royale","protein_custard",     "french",       0, 0.66f,0.12f,0.64f,0.08f,0.08f},
    {"Cantonese Steamed Egg","protein_custard",     "cantonese",    0, 0.34f,0.12f,0.66f,0.08f,0.08f},
    /* 8. coagulated_curd */
    {"Fresh Milk Paneer",    "coagulated_curd",     "north_indian", 0, 0.48f,0.36f,0.20f,0.05f,0.10f},
    {"Warm Whey Ricotta",    "coagulated_curd",     "italian",      0, 0.52f,0.34f,0.22f,0.04f,0.12f},
    {"Yose-Dofu Soy Curd",   "coagulated_curd",     "washoku",      KOCHEN_FILTER_VEGAN, 0.32f,0.22f,0.36f,0.05f,0.08f},
    {"Levantine Labneh Curd","coagulated_curd",     "levantine",    0, 0.50f,0.42f,0.24f,0.06f,0.10f},
    /* 9. gluten_dough */
    {"Neapolitan Pizza Dough","gluten_dough",       "italian",      KOCHEN_FILTER_VEGAN, 0.18f,0.14f,0.25f,0.04f,0.08f},
    {"French Brioche",       "gluten_dough",        "french",       0, 0.62f,0.10f,0.12f,0.02f,0.48f},
    {"Cantonese Steamed Bao","gluten_dough",        "cantonese",    0, 0.28f,0.10f,0.16f,0.02f,0.32f},
    {"Nordic Seeded Loaf",   "gluten_dough",        "nordic_euro",  0, 0.30f,0.18f,0.24f,0.04f,0.16f},
    /* 10. low_gluten_batter */
    {"Crisp Edomae Tempura", "low_gluten_batter",   "washoku",      0, 0.42f,0.12f,0.24f,0.04f,0.08f},
    {"Pate a Choux",         "low_gluten_batter",   "french",       0, 0.56f,0.08f,0.18f,0.02f,0.22f},
    {"Buttermilk Pancakes",  "low_gluten_batter",   "nordic_euro",  0, 0.48f,0.24f,0.14f,0.02f,0.42f},
    {"Crisp Rice Dosa",      "low_gluten_batter",   "south_indian", KOCHEN_FILTER_VEGAN, 0.36f,0.26f,0.28f,0.14f,0.08f},
    /* 11. aerated_foam */
    {"French Crisp Meringue","aerated_foam",        "french",       0, 0.08f,0.22f,0.04f,0.00f,0.78f},
    {"Dark Chocolate Mousse","aerated_foam",        "french",       0, 0.58f,0.14f,0.06f,0.02f,0.68f},
    {"Italian Marsala Foam", "aerated_foam",        "italian",      0, 0.38f,0.24f,0.06f,0.02f,0.66f},
    {"Rosewater Aquafaba",   "aerated_foam",        "levantine",    KOCHEN_FILTER_VEGAN, 0.12f,0.26f,0.04f,0.00f,0.72f},
    /* 12. sucrose_caramel */
    {"Salted Butter Caramel","sucrose_caramel",     "french",       0, 0.62f,0.14f,0.06f,0.02f,0.86f},
    {"Hazelnut Praline",     "sucrose_caramel",     "italian",      0, 0.52f,0.12f,0.06f,0.02f,0.84f},
    {"Sesame Tanghulu Glaze","sucrose_caramel",     "cantonese",    KOCHEN_FILTER_VEGAN, 0.22f,0.24f,0.06f,0.02f,0.86f},
    {"Palm Sugar Gastrique", "sucrose_caramel",     "se_asian",     KOCHEN_FILTER_VEGAN, 0.28f,0.44f,0.12f,0.08f,0.80f},
    /* 13. acid_osmotic_cure */
    {"Peruvian Sea Bass Ceviche","acid_osmotic_cure","andean",      0, 0.18f,0.86f,0.48f,0.58f,0.14f},
    {"Nordic Dill Gravlax",  "acid_osmotic_cure",   "nordic_euro",  0, 0.42f,0.68f,0.46f,0.14f,0.22f},
    {"Sinaloan Aguachile",   "acid_osmotic_cure",   "mexican",      0, 0.18f,0.88f,0.50f,0.72f,0.12f},
    {"Yuzu Fish Sunomono",   "acid_osmotic_cure",   "washoku",      0, 0.18f,0.78f,0.52f,0.12f,0.20f},
    /* 14. crisp_crust_roast */
    {"Duck-Fat Roast Potato","crisp_crust_roast",   "french",       0, 0.56f,0.14f,0.48f,0.12f,0.10f},
    {"Chicken Karaage",      "crisp_crust_roast",   "washoku",      0, 0.52f,0.18f,0.68f,0.32f,0.12f},
    {"Herb Chickpea Falafel","crisp_crust_roast",   "levantine",    KOCHEN_FILTER_VEGAN, 0.50f,0.22f,0.52f,0.28f,0.10f},
    {"Spiced Crispy Pork",   "crisp_crust_roast",   "sichuan",      0, 0.58f,0.18f,0.66f,0.68f,0.12f},
    /* 15. tempered_legume */
    {"Punjabi Dal Tadka",    "tempered_legume",     "north_indian", 0, 0.52f,0.36f,0.56f,0.48f,0.14f},
    {"South Indian Dal",     "tempered_legume",     "south_indian", KOCHEN_FILTER_VEGAN, 0.46f,0.44f,0.54f,0.52f,0.16f},
    {"Egyptian Ful Medames", "tempered_legume",     "levantine",    KOCHEN_FILTER_VEGAN, 0.48f,0.42f,0.52f,0.26f,0.12f},
    {"Oaxacan Black Beans",  "tempered_legume",     "mexican",      KOCHEN_FILTER_VEGAN, 0.46f,0.32f,0.58f,0.46f,0.14f},
};

static void assert_physical_invariants(
    const kochen_db_t *db,
    const kochen_patch_t *p,
    const kochen_dag_t *dag,
    const char *label
) {
    if (!dag->total_diag.ph_in_window || !dag->total_diag.hydration_in_window ||
        !dag->total_diag.emulsion_stable || !dag->total_diag.gel_in_window) {
        fprintf(stderr, "Physics failure on [%s]: pH=%.2f(ok=%d) hyd=%.2f(ok=%d) emul=%.2f/disp=%.2f(ok=%d) gel=%.2f(ok=%d) nacl=%.3f\n",
                label, dag->total_diag.mixture_ph, dag->total_diag.ph_in_window,
                dag->total_diag.hydration_ratio, dag->total_diag.hydration_in_window,
                dag->total_diag.surfactant_coverage, dag->total_diag.dispersed_fat_frac, dag->total_diag.emulsion_stable,
                dag->total_diag.gel_protein_pct, dag->total_diag.gel_in_window, dag->total_diag.nacl_pct);
        for (uint8_t s = 0; s < p->slot_count; s++) {
            const char *sl = (p->slots[s].ingredient_id >= 0) ? db->ingredients[p->slots[s].ingredient_id].slug : "NONE";
            fprintf(stderr, "  slot[%u] comp=%u role=%u: %s (%.1fg)\n", s, p->slots[s].component_idx, p->slots[s].role, sl, p->slots[s].mass_g);
        }
    }
    ASSERT_TRUE(dag->total_diag.ph_in_window);
    ASSERT_TRUE(dag->total_diag.hydration_in_window);
    ASSERT_TRUE(dag->total_diag.emulsion_stable);
    ASSERT_TRUE(dag->total_diag.gel_in_window);
    ASSERT_TRUE(dag->node_count >= 1);

    /* Verify Home vs Pro DAG projection structural identity */
    kochen_projection_t ph, pp;
    ASSERT_EQ_INT(kochen_project_dag(db, p, dag, KOCHEN_MODE_HOME, KOCHEN_LANG_EN, &ph), KOCHEN_OK);
    ASSERT_EQ_INT(kochen_project_dag(db, p, dag, KOCHEN_MODE_PRO, KOCHEN_LANG_EN, &pp), KOCHEN_OK);
    ASSERT_EQ_INT(ph.slot_count, pp.slot_count);
    ASSERT_EQ_INT(ph.step_count, pp.step_count);
    ASSERT_EQ_INT(ph.step_count, dag->node_count);
}

static void run_60_classic_reconstructions(const kochen_db_t *db) {
    for (int i = 0; i < 60; i++) {
        const classic_case_t *c = &CLASSICS_60[i];
        int aid = kochen_find_archetype(db, c->arch_slug);
        int pid = kochen_find_pantry(db, c->pantry_slug);
        ASSERT_TRUE(aid >= 0 && pid >= 0);

        kochen_patch_t p;
        ASSERT_EQ_INT(kochen_patch_init(db, &p, (uint8_t)aid, -1, (uint8_t)pid, KOCHEN_MODE_HOME), KOCHEN_OK);
        p.novelty = 0.0f;
        p.forbidden_diet_mask = c->diet_filter;
        p.target_sensory[KOCHEN_SENS_RICHNESS] = c->rich;
        p.target_sensory[KOCHEN_SENS_BRIGHTNESS] = c->bright;
        p.target_sensory[KOCHEN_SENS_UMAMI] = c->umami;
        p.target_sensory[KOCHEN_SENS_HEAT] = c->heat;
        p.target_sensory[KOCHEN_SENS_SWEET_BITTER] = c->sweet;

        ASSERT_EQ_INT(kochen_patch_solve(db, &p), KOCHEN_OK);

        /* Verify regional authenticity: majority of filled slots belong to target pantry */
        int filled = 0, regional = 0;
        for (uint8_t s = 0; s < p.slot_count; s++) {
            if (p.slots[s].ingredient_id < 0) continue;
            filled++;
            const kochen_ingredient_t *ing = &db->ingredients[p.slots[s].ingredient_id];
            ASSERT_EQ_INT((ing->diet_mask & c->diet_filter), 0);
            if (ing->pantry_mask & (1u << pid)) regional++;
        }
        ASSERT_TRUE(filled >= 3);
        ASSERT_TRUE(regional * 2 >= filled);

        kochen_dag_t dag;
        ASSERT_EQ_INT(kochen_dag_compile(db, &p, &dag), KOCHEN_OK);
        assert_physical_invariants(db, &p, &dag, c->classic_name);
    }
}

typedef struct {
    const char *name;
    const char *arch_slug;
    const char *pantry_slug;
    kochen_mode_t mode;
    uint32_t diet_mask;
    const char *lock_ing;
    uint8_t lock_slot;
    uint32_t expect_guard;
    bool test_home_resolve;
} mutation_case_t;

static const mutation_case_t MUTATIONS_30[30] = {
    {"Vegan Hollandaise",        "shear_emulsion",      "french",       KOCHEN_MODE_HOME, KOCHEN_FILTER_VEGAN, NULL,0, 0, false},
    {"Vegan Cacio e Pepe",       "starch_fat_emulsion", "italian",      KOCHEN_MODE_HOME, KOCHEN_FILTER_VEGAN, NULL,0, 0, false},
    {"Gluten-Free Cacio e Pepe", "starch_fat_emulsion", "italian",      KOCHEN_MODE_HOME, KOCHEN_DIET_GLUTEN,  NULL,0, KOCHEN_GUARD_CASEIN_CEIL, false},
    {"Dairy-Free Risotto",       "starch_hydration",    "italian",      KOCHEN_MODE_HOME, KOCHEN_DIET_DAIRY,   NULL,0, 0, false},
    {"Vegan Bourguignon",        "collagen_braise",     "french",       KOCHEN_MODE_HOME, KOCHEN_FILTER_VEGAN, NULL,0, KOCHEN_GUARD_MAILLARD_DRY, false},
    {"Alcohol-Free Bourguignon", "collagen_braise",     "french",       KOCHEN_MODE_HOME, KOCHEN_DIET_ALCOHOL, NULL,0, KOCHEN_GUARD_MAILLARD_DRY, false},
    {"Nut-Free Kung Pao",        "maillard_pan_glaze",  "sichuan",      KOCHEN_MODE_HOME, KOCHEN_DIET_NUT,     NULL,0, KOCHEN_GUARD_MAILLARD_DRY, false},
    {"Soy-Free Teriyaki Glaze",  "maillard_pan_glaze",  "washoku",      KOCHEN_MODE_HOME, KOCHEN_DIET_SOY,     NULL,0, KOCHEN_GUARD_MAILLARD_DRY, false},
    {"Gluten-Free Tempura",      "low_gluten_batter",   "washoku",      KOCHEN_MODE_HOME, KOCHEN_DIET_GLUTEN,  NULL,0, 0, false},
    {"Vegan Choux/Batter",       "low_gluten_batter",   "french",       KOCHEN_MODE_HOME, KOCHEN_FILTER_VEGAN, NULL,0, 0, false},
    {"Gluten-Free Pizza Dough",  "gluten_dough",        "italian",      KOCHEN_MODE_HOME, KOCHEN_DIET_GLUTEN,  NULL,0, 0, false},
    {"Vegan Brioche Rich Dough", "gluten_dough",        "french",       KOCHEN_MODE_HOME, KOCHEN_FILTER_VEGAN, "coconut_oil",2, KOCHEN_GUARD_LIPID_SHORTEN, false},
    {"Vegan Aquafaba Meringue",  "aerated_foam",        "french",       KOCHEN_MODE_HOME, KOCHEN_FILTER_VEGAN, NULL,0, KOCHEN_GUARD_FOAM_NO_LIPID, false},
    {"Dairy-Free Salted Caramel","sucrose_caramel",     "french",       KOCHEN_MODE_HOME, KOCHEN_DIET_DAIRY,   NULL,0, 0, false},
    {"Vegan Mushroom Ceviche",   "acid_osmotic_cure",   "andean",       KOCHEN_MODE_HOME, KOCHEN_FILTER_VEGAN, NULL,0, 0, false},
    {"Gluten-Free Karaage",      "crisp_crust_roast",   "washoku",      KOCHEN_MODE_HOME, KOCHEN_DIET_GLUTEN,  NULL,0, KOCHEN_GUARD_MAILLARD_DRY, false},
    {"Vegan Crispy Tofu Roast",  "crisp_crust_roast",   "sichuan",      KOCHEN_MODE_HOME, KOCHEN_FILTER_VEGAN, NULL,0, KOCHEN_GUARD_MAILLARD_DRY, false},
    {"Dairy-Free Veloute",       "veloute_suspension",  "french",       KOCHEN_MODE_HOME, KOCHEN_DIET_DAIRY,   NULL,0, 0, false},
    {"Vegan Silken Soy Custard", "protein_custard",     "washoku",      KOCHEN_MODE_HOME, KOCHEN_FILTER_VEGAN, "silken_tofu_base",1, 0, false},
    {"Vegan Soy Nigari Curd",    "coagulated_curd",     "north_indian", KOCHEN_MODE_HOME, KOCHEN_FILTER_VEGAN, "soy_milk_rich",0, 0, false},
    /* Physical Precondition Guard Adversarial Injections */
    {"Raw Pineapple Custard",    "protein_custard",     "mexican",      KOCHEN_MODE_HOME, 0, "fresh_pineapple_raw",4, KOCHEN_GUARD_ENZYME_PROTEASE, false},
    {"Raw Ginger Custard",       "protein_custard",     "cantonese",    KOCHEN_MODE_HOME, 0, "fresh_ginger_raw",2,    KOCHEN_GUARD_ENZYME_PROTEASE, false},
    {"Active Koji Custard",      "protein_custard",     "washoku",      KOCHEN_MODE_PRO,  0, "koji_enzyme_paste",2,   KOCHEN_GUARD_ENZYME_PROTEASE, true},
    {"Ginger Collagen Braise",   "collagen_braise",     "sichuan",      KOCHEN_MODE_HOME, 0, "fresh_ginger_raw",3,    KOCHEN_GUARD_ENZYME_PROTEASE, false},
    {"Tamarind Lentil Dal",      "tempered_legume",     "south_indian", KOCHEN_MODE_HOME, 0, "tamarind_pulp_conc",4,  KOCHEN_GUARD_PECTIN_ACID, false},
    {"Tomato Chickpea Stew",     "tempered_legume",     "north_indian", KOCHEN_MODE_HOME, 0, "fresh_tomato_diced",4,  KOCHEN_GUARD_PECTIN_ACID, false},
    {"Lime Black Bean Matrix",   "tempered_legume",     "mexican",      KOCHEN_MODE_HOME, 0, "fresh_lime_juice",4,    KOCHEN_GUARD_PECTIN_ACID, false},
    /* Pro Modernist -> Home Pantry Re-Solve Mutations */
    {"Citrate Emulsion->Home",   "starch_fat_emulsion", "italian",      KOCHEN_MODE_PRO,  0, "sodium_citrate",2,      0, true},
    {"Agar Vegan Gel->Home",     "protein_custard",     "washoku",      KOCHEN_MODE_PRO,  KOCHEN_FILTER_VEGAN, "agar_agar_powder",1, 0, true},
    {"Lecithin Emulsion->Home",  "shear_emulsion",      "french",       KOCHEN_MODE_PRO,  KOCHEN_DIET_EGG,     "soy_lecithin_pwd",1, 0, true},
};

static void run_30_adversarial_mutations(const kochen_db_t *db) {
    for (int i = 0; i < 30; i++) {
        const mutation_case_t *m = &MUTATIONS_30[i];
        int aid = kochen_find_archetype(db, m->arch_slug);
        int pid = kochen_find_pantry(db, m->pantry_slug);
        kochen_patch_t p;
        ASSERT_EQ_INT(kochen_patch_init(db, &p, (uint8_t)aid, -1, (uint8_t)pid, m->mode), KOCHEN_OK);
        p.forbidden_diet_mask = m->diet_mask;
        p.novelty = 0.25f;

        if (m->lock_ing) {
            int gid = kochen_find_ingredient(db, m->lock_ing);
            ASSERT_TRUE(gid >= 0 && m->lock_slot < p.slot_count);
            p.slots[m->lock_slot].ingredient_id = (int16_t)gid;
            p.slots[m->lock_slot].locked = 1;
            if (m->expect_guard == KOCHEN_GUARD_LIPID_SHORTEN) p.slots[m->lock_slot].mass_g = 65.0f;
            if (strcmp(m->lock_ing, "agar_agar_powder") == 0) p.slots[m->lock_slot].mass_g = 4.0f;
            if (strcmp(m->lock_ing, "sodium_citrate") == 0) p.slots[m->lock_slot].mass_g = 9.0f;
            if (strcmp(m->lock_ing, "soy_lecithin_pwd") == 0) p.slots[m->lock_slot].mass_g = 12.0f;
        }

        ASSERT_EQ_INT(kochen_patch_solve(db, &p), KOCHEN_OK);
        kochen_dag_t dag;
        ASSERT_EQ_INT(kochen_dag_compile(db, &p, &dag), KOCHEN_OK);
        assert_physical_invariants(db, &p, &dag, m->name);
        if (m->expect_guard && (dag.active_guards & m->expect_guard) != m->expect_guard) {
            fprintf(stderr, "Guard mismatch on [%s]: got 0x%X expected 0x%X\n", m->name, dag.active_guards, m->expect_guard);
            ASSERT_TRUE(false);
        }

        if (m->test_home_resolve) {
            ASSERT_TRUE(dag.total_diag.has_modernist_additive);
            ASSERT_EQ_INT(kochen_resolve_home_pantry(db, &p), KOCHEN_OK);
            ASSERT_EQ_INT(kochen_dag_compile(db, &p, &dag), KOCHEN_OK);
            ASSERT_TRUE(!dag.total_diag.has_modernist_additive);
            assert_physical_invariants(db, &p, &dag, m->name);
        }
    }
}

static void run_1000_novelty_stress_patches(const kochen_db_t *db) {
    static const uint32_t DIETS[6] = {
        0, KOCHEN_FILTER_VEGETARIAN, KOCHEN_FILTER_VEGAN,
        KOCHEN_DIET_DAIRY, KOCHEN_DIET_GLUTEN, KOCHEN_DIET_NUT | KOCHEN_DIET_SOY
    };
    static const int8_t COMPANIONS[5] = {-1, -1, 1, 5, 12};

    uint32_t rng = 0x20261126u;
    int solved_ok = 0;
    for (int run = 0; run < 1050; run++) {
        rng = rng * 1664525u + 1013904223u;
        uint8_t aid = (uint8_t)(run % db->archetype_count);
        uint8_t pid = (uint8_t)((run * 7 + (rng >> 24)) % db->pantry_count);
        int8_t comp = COMPANIONS[(run / 15) % 5];
        if (comp == (int8_t)aid) comp = -1;
        kochen_mode_t mode = (run & 1) ? KOCHEN_MODE_PRO : KOCHEN_MODE_HOME;

        kochen_patch_t p;
        ASSERT_EQ_INT(kochen_patch_init(db, &p, aid, comp, pid, mode), KOCHEN_OK);
        p.novelty = (float)(run % 11) * 0.1f;
        p.forbidden_diet_mask = DIETS[(run / 3) % 6];
        p.seed = rng;

        for (int a = 0; a < KOCHEN_SENS_COUNT; a++) {
            rng = rng * 1664525u + 1013904223u;
            p.target_sensory[a] = 0.1f + 0.8f * ((float)(rng & 0xFFu) / 255.0f);
        }

        kochen_status_t st = kochen_patch_solve(db, &p);
        if (st == KOCHEN_ERR_INFEASIBLE) continue;
        ASSERT_EQ_INT(st, KOCHEN_OK);
        solved_ok++;

        kochen_dag_t dag;
        ASSERT_EQ_INT(kochen_dag_compile(db, &p, &dag), KOCHEN_OK);
        assert_physical_invariants(db, &p, &dag, "stress_patch");
    }
    ASSERT_TRUE(solved_ok >= 1000);
    printf("Stress suite: %d / 1050 feasible patches verified 100%% physically valid\n", solved_ok);
}

int main(void) {
    static kochen_db_t db;
    ASSERT_EQ_INT(kochen_db_load_dir(&db, KOCHEN_SOURCE_DIR "/assets"), KOCHEN_OK);
    run_60_classic_reconstructions(&db);
    run_30_adversarial_mutations(&db);
    run_1000_novelty_stress_patches(&db);
    printf("test_broad_sample passed (%d assertions)\n", g_tests_run);
    return 0;
}
