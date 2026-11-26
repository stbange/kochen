#include "tests/test_harness.h"
#include "core/kochen_data.h"
#include "ui/kochen_audio.h"
#include "ui/kochen_studio.h"

static void click_widget(kochen_studio_t *st, kochen_draw_list_t *dl, kochen_widget_kind_t kind, int16_t param, bool is_touch) {
    kochen_studio_build_frame(st, 0.016f, dl);
    const kochen_widget_t *w = kochen_studio_find_widget(st, kind, param);
    ASSERT_TRUE(w != NULL);
    if (st->touch_mode) ASSERT_TRUE(w->h >= 40.0f);
    float cx = w->x + w->w * 0.5f, cy = w->y + w->h * 0.5f;
    ASSERT_TRUE(kochen_studio_pointer_down(st, cx, cy, is_touch));
    kochen_studio_pointer_up(st);
}

static void drag_knob(kochen_studio_t *st, kochen_draw_list_t *dl, int16_t knob_idx, float dy_px) {
    kochen_studio_build_frame(st, 0.016f, dl);
    const kochen_widget_t *w = kochen_studio_find_widget(st, KOCHEN_WDG_KNOB, knob_idx);
    ASSERT_TRUE(w != NULL);
    float cx = w->x + w->w * 0.5f, cy = w->y + w->h * 0.5f;
    ASSERT_TRUE(kochen_studio_pointer_down(st, cx, cy, st->touch_mode));
    ASSERT_TRUE(kochen_studio_pointer_move(st, cx, cy + dy_px));
    kochen_studio_pointer_up(st);
}

int main(void) {
    static kochen_db_t db;
    ASSERT_EQ_INT(kochen_db_load_dir(&db, KOCHEN_SOURCE_DIR "/assets"), KOCHEN_OK);

    kochen_audio_t audio;
    kochen_audio_init(&audio, 44100);

    static kochen_studio_t st;
    static kochen_draw_list_t dl;
    ASSERT_EQ_INT(kochen_studio_init(&st, &db, &audio, 1280.0f, 800.0f), KOCHEN_OK);
    ASSERT_TRUE(!st.touch_mode);

    /* Journey 1: Desktop Rack, Knob Dragging, Lock/Reroll, Picker Modal & Scroll */
    kochen_studio_build_frame(&st, 0.016f, &dl);
    ASSERT_TRUE(dl.count > 20);
    ASSERT_TRUE(!kochen_studio_pointer_down(&st, 2.0f, 400.0f, false));

    drag_knob(&st, &dl, 0, -95.0f); /* Drag Novelty up */
    ASSERT_TRUE(st.patch.novelty > 0.45f);
    drag_knob(&st, &dl, 1, -60.0f); /* Drag Richness up */
    drag_knob(&st, &dl, 3, -50.0f); /* Drag Umami up */

    click_widget(&st, &dl, KOCHEN_WDG_SLOT_LOCK, 0, false);
    ASSERT_EQ_INT(st.patch.slots[0].locked, 1);
    click_widget(&st, &dl, KOCHEN_WDG_SLOT_REROLL, 2, false);
    click_widget(&st, &dl, KOCHEN_WDG_MUTATE, 0, false);

    /* Open ingredient picker, test close, then reopen and select item */
    click_widget(&st, &dl, KOCHEN_WDG_SLOT_PICK, 1, false);
    ASSERT_EQ_INT(st.picker_slot, 1);
    kochen_studio_build_frame(&st, 0.016f, &dl);
    /* Background clicks ignored while modal open */
    ASSERT_TRUE(!kochen_studio_pointer_down(&st, 10.0f, 10.0f, false));
    click_widget(&st, &dl, KOCHEN_WDG_PICKER_CLOSE, 0, false);
    ASSERT_EQ_INT(st.picker_slot, -1);

    click_widget(&st, &dl, KOCHEN_WDG_SLOT_PICK, 1, false);
    kochen_studio_build_frame(&st, 0.016f, &dl);
    int16_t picked_ing = -1;
    for (uint16_t i = 0; i < st.widget_count; i++) {
        if (st.widgets[i].kind == KOCHEN_WDG_PICKER_ITEM) {
            picked_ing = st.widgets[i].param;
            break;
        }
    }
    ASSERT_TRUE(picked_ing >= 0);
    click_widget(&st, &dl, KOCHEN_WDG_PICKER_ITEM, picked_ing, false);
    ASSERT_EQ_INT(st.picker_slot, -1);
    ASSERT_EQ_INT(st.patch.slots[1].ingredient_id, picked_ing);

    kochen_studio_scroll(&st, -2.0f);
    kochen_studio_scroll(&st, 2.0f);

    /* Journey 2: Primary + Companion Chaining & Dietary Chips */
    click_widget(&st, &dl, KOCHEN_WDG_PRIMARY_NEXT, 0, false);
    click_widget(&st, &dl, KOCHEN_WDG_PANTRY_NEXT, 0, false);
    click_widget(&st, &dl, KOCHEN_WDG_COMP_NEXT, 0, false);
    ASSERT_TRUE(st.patch.companion_arch_id >= 0);
    ASSERT_TRUE(st.dag.nodes[st.dag.node_count - 1].op == KOCHEN_OP_PLATE_ASSEMBLE);

    click_widget(&st, &dl, KOCHEN_WDG_DIET_CHIP, 0, false); /* Vegan ON */
    ASSERT_EQ_INT((st.patch.forbidden_diet_mask & KOCHEN_FILTER_VEGAN), KOCHEN_FILTER_VEGAN);
    click_widget(&st, &dl, KOCHEN_WDG_DIET_CHIP, 0, false); /* Vegan OFF */

    /* Cycle companion back through all options to None */
    for (int c = 0; c < 10 && st.patch.companion_arch_id >= 0; c++)
        click_widget(&st, &dl, KOCHEN_WDG_COMP_NEXT, 0, false);
    ASSERT_EQ_INT(st.patch.companion_arch_id, -1);

    /* Journey 3: Pro Modernist -> Home Toggle Identity & Explicit Home Re-Solve */
    ASSERT_EQ_INT(kochen_studio_init(&st, &db, &audio, 1280.0f, 800.0f), KOCHEN_OK);
    click_widget(&st, &dl, KOCHEN_WDG_MODE_TOGGLE, 0, false);
    ASSERT_EQ_INT(st.view_mode, KOCHEN_MODE_PRO);

    int cit_id = kochen_find_ingredient(&db, "sodium_citrate");
    click_widget(&st, &dl, KOCHEN_WDG_SLOT_PICK, 2, false);
    click_widget(&st, &dl, KOCHEN_WDG_PICKER_ITEM, (int16_t)cit_id, false);
    ASSERT_TRUE(st.dag.total_diag.has_modernist_additive);
    uint8_t pro_nodes = st.dag.node_count;

    /* Toggle PRO -> HOME: exact same DAG & ingredient preserved; badge appears */
    click_widget(&st, &dl, KOCHEN_WDG_MODE_TOGGLE, 0, false);
    ASSERT_EQ_INT(st.view_mode, KOCHEN_MODE_HOME);
    ASSERT_EQ_INT(st.dag.node_count, pro_nodes);
    ASSERT_EQ_INT(st.patch.slots[2].ingredient_id, cit_id);
    ASSERT_TRUE(st.proj.show_home_resolve_badge);

    /* Explicit one-click Re-solve for Home Pantry */
    click_widget(&st, &dl, KOCHEN_WDG_HOME_RESOLVE, 0, false);
    ASSERT_TRUE(!st.dag.total_diag.has_modernist_additive);
    ASSERT_TRUE(!st.proj.show_home_resolve_badge);

    /* Journey 4: Share URL & Markdown Export + Restore */
    click_widget(&st, &dl, KOCHEN_WDG_SHARE_PATCH, 0, false);
    ASSERT_TRUE(st.share_requested);
    char saved_code[256];
    strcpy(saved_code, st.last_patch_code);
    uint8_t saved_arch = st.patch.primary_arch_id;

    click_widget(&st, &dl, KOCHEN_WDG_EXPORT_MD, 0, false);
    ASSERT_TRUE(st.export_md_requested);
    ASSERT_TRUE(strlen(st.last_markdown) > 150);

    click_widget(&st, &dl, KOCHEN_WDG_PRIMARY_NEXT, 0, false);
    ASSERT_TRUE(st.patch.primary_arch_id != saved_arch);
    ASSERT_EQ_INT(kochen_studio_load_patch_code(&st, saved_code), KOCHEN_OK);
    ASSERT_EQ_INT(st.patch.primary_arch_id, saved_arch);

    /* Journey 5: Mobile Touch Auto-Switch, Tabs, EN/DE/PL & Procedural Audio/Mute */
    kochen_studio_resize(&st, 390.0f, 844.0f);
    ASSERT_TRUE(st.touch_mode);
    kochen_studio_build_frame(&st, 0.5f, &dl);
    const kochen_widget_t *tab1 = kochen_studio_find_widget(&st, KOCHEN_WDG_MOBILE_TAB, 1);
    ASSERT_TRUE(tab1 != NULL && tab1->h >= 48.0f);

    click_widget(&st, &dl, KOCHEN_WDG_MOBILE_TAB, 1, true);
    ASSERT_EQ_INT(st.mobile_tab, 1);
    drag_knob(&st, &dl, 2, -45.0f);
    click_widget(&st, &dl, KOCHEN_WDG_MOBILE_TAB, 2, true);
    ASSERT_EQ_INT(st.mobile_tab, 2);
    click_widget(&st, &dl, KOCHEN_WDG_MOBILE_TAB, 0, true);

    /* Cycle EN -> DE -> PL -> EN */
    click_widget(&st, &dl, KOCHEN_WDG_LANG_CYCLE, 0, true);
    ASSERT_EQ_INT(st.lang, KOCHEN_LANG_DE);
    click_widget(&st, &dl, KOCHEN_WDG_LANG_CYCLE, 0, true);
    ASSERT_EQ_INT(st.lang, KOCHEN_LANG_PL);
    click_widget(&st, &dl, KOCHEN_WDG_LANG_CYCLE, 0, true);
    ASSERT_EQ_INT(st.lang, KOCHEN_LANG_EN);

    /* Manual Touch <-> Desktop override toggle */
    click_widget(&st, &dl, KOCHEN_WDG_TOUCH_TOGGLE, 0, false);
    ASSERT_TRUE(!st.touch_mode);
    /* Also test auto-switch on touch event when override is cleared */
    st.touch_override_set = false;
    kochen_studio_build_frame(&st, 0.016f, &dl);
    const kochen_widget_t *mut_btn = kochen_studio_find_widget(&st, KOCHEN_WDG_MUTATE, 0);
    ASSERT_TRUE(kochen_studio_pointer_down(&st, mut_btn->x + 5.0f, mut_btn->y + 5.0f, true));
    kochen_studio_pointer_up(&st);
    ASSERT_TRUE(st.touch_mode);

    /* Verify active PCM synthesis (both harmonic & dissonant) and strict 0.0f mute */
    float pcm[1024];
    kochen_audio_mix(&audio, pcm, 1024);
    float peak = 0.0f;
    for (int i = 0; i < 1024; i++) {
        ASSERT_TRUE(isfinite(pcm[i]) && fabsf(pcm[i]) <= 0.96f);
        if (fabsf(pcm[i]) > peak) peak = fabsf(pcm[i]);
    }
    ASSERT_TRUE(peak > 0.01f);

    kochen_diagnostics_t bad_diag = st.dag.total_diag;
    bad_diag.emulsion_stable = false;
    kochen_audio_trigger_patch(&audio, &bad_diag);
    kochen_audio_mix(&audio, pcm, 512);
    ASSERT_TRUE(fabsf(pcm[10]) > 0.0f);

    click_widget(&st, &dl, KOCHEN_WDG_MUTE_TOGGLE, 0, true);
    ASSERT_TRUE(audio.muted);
    kochen_audio_trigger_patch(&audio, &st.dag.total_diag);
    kochen_audio_mix(&audio, pcm, 256);
    for (int i = 0; i < 256; i++) ASSERT_NEAR(pcm[i], 0.0f, 1e-7f);

    printf("test_e2e_synth passed (%d assertions)\n", g_tests_run);
    return 0;
}
