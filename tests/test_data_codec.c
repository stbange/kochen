#include "tests/test_harness.h"
#include "core/kochen_codec.h"
#include "core/kochen_data.h"
#include "core/kochen_solver.h"

static void test_full_dataset_and_lookups(kochen_db_t *db) {
    ASSERT_EQ_INT(kochen_db_load_dir(db, KOCHEN_SOURCE_DIR "/assets"), KOCHEN_OK);
    ASSERT_TRUE(db->ingredient_count >= 150);
    ASSERT_EQ_INT(db->pantry_count, 12);
    ASSERT_EQ_INT(db->archetype_count, 15);
    ASSERT_TRUE(db->locale_count >= 290);

    ASSERT_TRUE(kochen_find_ingredient(db, "pecorino_romano") >= 0);
    ASSERT_EQ_INT(kochen_find_ingredient(db, "missing_ing"), -1);
    ASSERT_TRUE(kochen_find_archetype(db, "starch_fat_emulsion") >= 0);
    ASSERT_EQ_INT(kochen_find_archetype(db, "missing_arch"), -1);
    ASSERT_TRUE(kochen_find_pantry(db, "washoku") >= 0);
    ASSERT_EQ_INT(kochen_find_pantry(db, "missing_pantry"), -1);

    /* Fallback on unknown key or out-of-range lang */
    ASSERT_TRUE(strcmp(kochen_tr(db, KOCHEN_LANG_EN, "unknown.key"), "unknown.key") == 0);
    ASSERT_TRUE(strlen(kochen_tr(db, (kochen_lang_t)99, "ui.app_title")) > 5);
}

static void test_parser_boundary_rejections(void) {
    kochen_db_t db;
    kochen_db_init(&db);
    ASSERT_EQ_INT(kochen_db_load_dir(&db, "/nonexistent/path/assets"), KOCHEN_ERR_IO);

    kochen_db_init(&db);
    ASSERT_EQ_INT(kochen_parse_pantries_tsv(&db, "bad_line_no_tab\n"), KOCHEN_ERR_PARSE);
    ASSERT_EQ_INT(kochen_parse_pantries_tsv(&db, "italian\t2.5\n"), KOCHEN_ERR_RANGE);
    ASSERT_EQ_INT(kochen_parse_pantries_tsv(&db, "italian\t0.2\nitalian\t0.2\n"), KOCHEN_ERR_PARSE);

    kochen_db_init(&db);
    ASSERT_EQ_INT(kochen_parse_ingredients_tsv(&db, "too_few\t1\t2\n"), KOCHEN_ERR_PARSE);
    /* Saturated fat > total fat and invalid pH rejected */
    const char *bad_ing = "bad_fat\t10\t5\t20\t0\t10\t0\t0\t0\t0\t0\t0\t6.0\t0.2\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t1\t0\t0\t1\t1\t0\t0\t10\n";
    ASSERT_EQ_INT(kochen_parse_ingredients_tsv(&db, bad_ing), KOCHEN_ERR_RANGE);
    const char *nan_ing = "nan_ing\tNaN\t5\t1\t0\t10\t0\t0\t0\t0\t0\t0\t6.0\t0.2\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t1\t0\t0\t1\t1\t0\t0\t10\n";
    ASSERT_EQ_INT(kochen_parse_ingredients_tsv(&db, nan_ing), KOCHEN_ERR_RANGE);
    const char *neg_u32 = "neg_u\t10\t5\t1\t0\t10\t-1\t0\t0\t0\t0\t0\t6.0\t0.2\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t1\t0\t0\t1\t1\t0\t0\t10\n";
    ASSERT_EQ_INT(kochen_parse_ingredients_tsv(&db, neg_u32), KOCHEN_ERR_RANGE);

    kochen_db_init(&db);
    ASSERT_EQ_INT(kochen_parse_archetypes_ini(&db, "orphan_key=1\n"), KOCHEN_ERR_PARSE);
    ASSERT_EQ_INT(kochen_parse_archetypes_ini(&db, "[]\n"), KOCHEN_ERR_PARSE);
    ASSERT_EQ_INT(kochen_parse_archetypes_ini(&db, "[empty_arch]\n"), KOCHEN_ERR_PARSE);
    ASSERT_EQ_INT(kochen_parse_archetypes_ini(&db, "[bad_arch]\nunknown=1\n"), KOCHEN_ERR_PARSE);
    ASSERT_EQ_INT(kochen_parse_archetypes_ini(&db, "[bad_ph]\nph=8.0,4.0\n"), KOCHEN_ERR_RANGE);

    kochen_db_init(&db);
    ASSERT_EQ_INT(kochen_parse_locale_tsv(&db, (kochen_lang_t)9, "k\tv\n"), KOCHEN_ERR_RANGE);
    ASSERT_EQ_INT(kochen_parse_locale_tsv(&db, KOCHEN_LANG_EN, "missing_tab\n"), KOCHEN_ERR_PARSE);
}

static void test_codec_roundtrip_and_tamper(const kochen_db_t *db) {
    kochen_patch_t p = {0}, decoded = {0};
    ASSERT_EQ_INT(kochen_patch_init(db, &p, 0, 1, 0, KOCHEN_MODE_PRO), KOCHEN_OK);
    p.novelty = 0.65f;
    p.forbidden_diet_mask = KOCHEN_DIET_DAIRY;
    ASSERT_EQ_INT(kochen_patch_solve(db, &p), KOCHEN_OK);
    p.slots[0].locked = 1;

    char code[256];
    ASSERT_EQ_INT(kochen_patch_encode(&p, code, 8), KOCHEN_ERR_CAPACITY);
    ASSERT_EQ_INT(kochen_patch_encode(&p, code, sizeof(code)), KOCHEN_OK);

    char url[320];
    snprintf(url, sizeof(url), "https://stbange.github.io/kochen/?patch=%s&lang=de", code);
    ASSERT_EQ_INT(kochen_patch_decode(db, url, &decoded), KOCHEN_OK);
    ASSERT_EQ_INT(decoded.primary_arch_id, p.primary_arch_id);
    ASSERT_EQ_INT(decoded.companion_arch_id, p.companion_arch_id);
    ASSERT_EQ_INT(decoded.pantry_id, p.pantry_id);
    ASSERT_EQ_INT(decoded.solve_mode, p.solve_mode);
    ASSERT_EQ_INT(decoded.slot_count, p.slot_count);
    ASSERT_NEAR(decoded.novelty, p.novelty, 0.01);
    for (uint8_t i = 0; i < p.slot_count; i++) {
        ASSERT_EQ_INT(decoded.slots[i].ingredient_id, p.slots[i].ingredient_id);
        ASSERT_EQ_INT(decoded.slots[i].locked, p.slots[i].locked);
        ASSERT_NEAR(decoded.slots[i].mass_g, p.slots[i].mass_g, 0.15);
    }

    /* Corrupt checksum */
    size_t clen = strlen(code);
    code[clen - 2] = (code[clen - 2] == 'A') ? 'B' : 'A';
    ASSERT_EQ_INT(kochen_patch_decode(db, code, &decoded), KOCHEN_ERR_CHECKSUM);
    ASSERT_EQ_INT(kochen_patch_decode(db, "short", &decoded), KOCHEN_ERR_PARSE);
    ASSERT_EQ_INT(kochen_patch_decode(db, "invalid$chars$in$base64$string$here!", &decoded), KOCHEN_ERR_PARSE);
}

int main(void) {
    static kochen_db_t db;
    test_full_dataset_and_lookups(&db);
    test_parser_boundary_rejections();
    test_codec_roundtrip_and_tamper(&db);
    printf("test_data_codec passed (%d assertions)\n", g_tests_run);
    return 0;
}
