#include "core/kochen_project.h"
#include "core/kochen_data.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *op_slug(kochen_op_kind_t op) {
    static const char *names[] = {
        "pat_dry_brine", "pre_scald_enzyme", "hydrate_neutral", "lipid_shorten_knead",
        "whip_clean_foam", "bloom_fat_aroma", "maillard_sear", "starch_cook",
        "collagen_braise", "shear_emulsify", "gentle_coagulate", "sucrose_stage",
        "acid_osmotic_cure", "crust_dehydrate_fry", "post_soften_acid",
        "off_heat_volatiles", "plate_assemble"
    };
    return ((unsigned)op < sizeof(names) / sizeof(names[0])) ? names[op] : "starch_cook";
}

static const char *role_key(kochen_role_t r) {
    static const char *keys[] = {
        "role.starch_matrix", "role.protein_collagen", "role.protein_tender",
        "role.legume_matrix", "role.aqueous_base", "role.fat_medium",
        "role.surfactant_binder", "role.hydrocolloid_gel", "role.umami_ferment",
        "role.aroma_early_fat", "role.aroma_late_volatile", "role.acid_brightener",
        "role.sweet_caramel", "role.texture_crisp"
    };
    return ((unsigned)r < KOCHEN_ROLE_COUNT) ? keys[r] : "role.starch_matrix";
}

static const char *unit_key(kochen_home_unit_t u) {
    static const char *keys[] = {
        "unit.g", "unit.tsp", "unit.tbsp", "unit.cup", "unit.pcs", "unit.clove", "unit.pinch"
    };
    return ((unsigned)u <= KOCHEN_UNIT_PINCH) ? keys[u] : "unit.g";
}

static const char *heat_level_key(float temp_c) {
    if (temp_c <= 32.0f) return "heat.room";
    if (temp_c <= 68.0f) return "heat.gentle";
    if (temp_c <= 96.0f) return "heat.simmer";
    if (temp_c <= 140.0f) return "heat.medium";
    return "heat.high";
}

static void format_duration_home(float sec, char *out, size_t cap) {
    if (sec < 90.0f) snprintf(out, cap, "%d s", (int)roundf(sec));
    else if (sec < 5400.0f) snprintf(out, cap, "%d min", (int)fmaxf(1.0f, roundf(sec / 60.0f)));
    else snprintf(out, cap, "%.1f h", sec / 3600.0f);
}

static void format_slot_amount(
    const kochen_db_t *db,
    const kochen_ingredient_t *ing,
    float mass_g,
    float comp_total_g,
    kochen_mode_t mode,
    kochen_lang_t lang,
    char *out,
    size_t cap
) {
    if (mode == KOCHEN_MODE_PRO) {
        float pct = (comp_total_g > 0.1f) ? (100.0f * mass_g / comp_total_g) : 0.0f;
        snprintf(out, cap, "%.1f g (%.1f%%)", mass_g, pct);
        return;
    }
    int g_round = (int)fmaxf(1.0f, roundf(mass_g));
    if (ing->home_unit == KOCHEN_UNIT_GRAM || ing->grams_per_unit <= 0.1f) {
        snprintf(out, cap, "%d g", g_round);
    } else {
        float units = mass_g / ing->grams_per_unit;
        const char *u_lbl = kochen_tr(db, lang, unit_key(ing->home_unit));
        if (units < 0.35f)
            snprintf(out, cap, "%d g (~ 1/4 %s)", g_round, u_lbl);
        else if (units < 0.65f)
            snprintf(out, cap, "%d g (~ 1/2 %s)", g_round, u_lbl);
        else
            snprintf(out, cap, "%d g (~ %.1f %s)", g_round, roundf(units * 2.0f) * 0.5f, u_lbl);
    }
}

kochen_status_t kochen_project_dag(
    const kochen_db_t *db,
    const kochen_patch_t *patch,
    const kochen_dag_t *dag,
    kochen_mode_t view_mode,
    kochen_lang_t lang,
    kochen_projection_t *out
) {
    if (!db || !patch || !dag || !out || patch->primary_arch_id >= db->archetype_count)
        return KOCHEN_ERR_RANGE;
    if ((unsigned)lang >= KOCHEN_LANG_COUNT) lang = KOCHEN_LANG_EN;

    memset(out, 0, sizeof(*out));
    out->view_mode = view_mode;
    out->lang = lang;

    char key[64];
    const kochen_archetype_t *pa = &db->archetypes[patch->primary_arch_id];
    snprintf(key, sizeof(key), "arch.%s", pa->slug);
    const char *pa_name = kochen_tr(db, lang, key);

    const char *lead_ing = NULL;
    for (uint8_t i = 0; i < patch->slot_count; i++) {
        if (patch->slots[i].component_idx == 0 && patch->slots[i].ingredient_id >= 0) {
            snprintf(key, sizeof(key), "ing.%s", db->ingredients[patch->slots[i].ingredient_id].slug);
            lead_ing = kochen_tr(db, lang, key);
            break;
        }
    }

    if (patch->companion_arch_id >= 0 && patch->companion_arch_id < (int8_t)db->archetype_count) {
        char ckey[64];
        snprintf(ckey, sizeof(ckey), "arch.%s", db->archetypes[patch->companion_arch_id].slug);
        snprintf(out->dish_title, sizeof(out->dish_title), "%s — %s + %s",
                 lead_ing ? lead_ing : "Kochen", pa_name, kochen_tr(db, lang, ckey));
    } else {
        snprintf(out->dish_title, sizeof(out->dish_title), "%s — %s",
                 lead_ing ? lead_ing : "Kochen", pa_name);
    }

    snprintf(key, sizeof(key), "pantry.%s", db->pantries[patch->pantry_id].slug);
    snprintf(out->subtitle, sizeof(out->subtitle), "%s · %s (%d%%)",
             kochen_tr(db, lang, key),
             kochen_tr(db, lang, view_mode == KOCHEN_MODE_PRO ? "ui.mode_pro" : "ui.mode_home"),
             (int)roundf(patch->novelty * 100.0f));

    if (view_mode == KOCHEN_MODE_HOME && dag->total_diag.has_modernist_additive) {
        out->show_home_resolve_badge = true;
        snprintf(out->home_resolve_prompt, sizeof(out->home_resolve_prompt), "%s",
                 kochen_tr(db, lang, "ui.home_resolve_prompt"));
    }

    for (uint8_t i = 0; i < patch->slot_count; i++) {
        const kochen_patch_slot_t *s = &patch->slots[i];
        if (s->ingredient_id < 0 || s->mass_g <= 0.05f) continue;
        kochen_projected_slot_t *ps = &out->slots[out->slot_count++];
        const kochen_ingredient_t *ing = &db->ingredients[s->ingredient_id];
        snprintf(key, sizeof(key), "ing.%s", ing->slug);
        snprintf(ps->name, sizeof(ps->name), "%s", kochen_tr(db, lang, key));
        snprintf(ps->role_label, sizeof(ps->role_label), "%s", kochen_tr(db, lang, role_key(s->role)));
        float comp_tot = (s->component_idx == 0) ? dag->primary_diag.total_mass_g : dag->companion_diag.total_mass_g;
        format_slot_amount(db, ing, s->mass_g, comp_tot, view_mode, lang, ps->amount_text, sizeof(ps->amount_text));
        ps->component_idx = s->component_idx;
        ps->locked = s->locked;
        ps->is_modernist = (ing->diet_mask & KOCHEN_DIET_MODERNIST) ? 1 : 0;
        ps->bridge_mask = ing->volatile_mask & dag->total_diag.shared_volatile_mask;
    }

    for (uint8_t n = 0; n < dag->node_count; n++) {
        const kochen_dag_node_t *nd = &dag->nodes[n];
        kochen_projected_step_t *st = &out->steps[out->step_count++];
        st->node_id = nd->node_id;
        st->component_idx = nd->component_idx;

        snprintf(key, sizeof(key), "op.%s", op_slug(nd->op));
        snprintf(st->title, sizeof(st->title), "%s", kochen_tr(db, lang, key));

        if (view_mode == KOCHEN_MODE_PRO) {
            snprintf(st->metrics_badge, sizeof(st->metrics_badge), "%.1f °C · %.0f s · pH %.2f · %.0f g",
                     nd->temp_c, nd->duration_sec, nd->out_ph, nd->out_mass_g);
        } else {
            char dur[32];
            format_duration_home(nd->duration_sec, dur, sizeof(dur));
            snprintf(st->metrics_badge, sizeof(st->metrics_badge), "%s · %s",
                     kochen_tr(db, lang, heat_level_key(nd->temp_c)), dur);
        }

        char ing_list[180] = {0};
        size_t pos = 0;
        for (uint8_t r = 0; r < nd->slot_ref_count; r++) {
            uint8_t si = nd->slot_refs[r];
            if (si >= patch->slot_count || patch->slots[si].ingredient_id < 0) continue;
            const kochen_ingredient_t *ing = &db->ingredients[patch->slots[si].ingredient_id];
            snprintf(key, sizeof(key), "ing.%s", ing->slug);
            const char *nm = kochen_tr(db, lang, key);
            int w = snprintf(ing_list + pos, sizeof(ing_list) - pos, "%s%s (%.0fg)",
                             pos > 0 ? ", " : "", nm, patch->slots[si].mass_g);
            if (w > 0 && pos + (size_t)w < sizeof(ing_list)) pos += (size_t)w;
        }

        snprintf(key, sizeof(key), "cue.%s.%s", view_mode == KOCHEN_MODE_PRO ? "pro" : "home", op_slug(nd->op));
        const char *cue = kochen_tr(db, lang, key);
        if (ing_list[0])
            snprintf(st->instruction, sizeof(st->instruction), "[%s] %s", ing_list, cue);
        else
            snprintf(st->instruction, sizeof(st->instruction), "%s", cue);
    }

    return KOCHEN_OK;
}

static void append_fmt(char *buf, size_t cap, size_t *len, const char *fmt, ...) {
    if (*len + 1 >= cap) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + *len, cap - *len, fmt, ap);
    va_end(ap);
    if (n > 0) *len = (*len + (size_t)n < cap) ? (*len + (size_t)n) : (cap - 1);
}

size_t kochen_export_markdown(
    const kochen_db_t *db,
    const kochen_patch_t *patch,
    const kochen_dag_t *dag,
    const kochen_projection_t *proj,
    const char *patch_code,
    char *out_buf,
    size_t buf_cap
) {
    if (!db || !patch || !dag || !proj || !out_buf || buf_cap == 0) return 0;
    out_buf[0] = '\0';
    size_t len = 0;

    append_fmt(out_buf, buf_cap, &len, "# %s\n\n_%s_\n\n", proj->dish_title, proj->subtitle);
    if (patch_code && patch_code[0])
        append_fmt(out_buf, buf_cap, &len, "**Patch:** `?patch=%s`\n\n", patch_code);
    if (proj->show_home_resolve_badge)
        append_fmt(out_buf, buf_cap, &len, "> %s\n\n", proj->home_resolve_prompt);

    append_fmt(out_buf, buf_cap, &len, "## %s\n\n", kochen_tr(db, proj->lang, "ui.ingredients"));
    for (uint8_t i = 0; i < proj->slot_count; i++) {
        const kochen_projected_slot_t *s = &proj->slots[i];
        append_fmt(out_buf, buf_cap, &len, "- **%s**: %s _(%s%s)_\n",
                   s->name, s->amount_text, s->role_label, s->component_idx == 1 ? " · Companion" : "");
    }

    append_fmt(out_buf, buf_cap, &len, "\n## %s\n\n", kochen_tr(db, proj->lang, "ui.diagnostics"));
    append_fmt(out_buf, buf_cap, &len,
               "- pH: `%.2f` | NaCl: `%.2f%%` | Umami: `%.2f` | Yield: `%.0f g`\n\n",
               dag->total_diag.mixture_ph, dag->total_diag.nacl_pct,
               dag->total_diag.umami_synergy, dag->total_diag.final_mass_g);

    append_fmt(out_buf, buf_cap, &len, "## %s\n\n", kochen_tr(db, proj->lang, "ui.steps"));
    for (uint8_t i = 0; i < proj->step_count; i++) {
        const kochen_projected_step_t *st = &proj->steps[i];
        append_fmt(out_buf, buf_cap, &len, "%u. **%s** (`%s`) — %s\n",
                   (unsigned)(i + 1), st->title, st->metrics_badge, st->instruction);
    }
    return len;
}
