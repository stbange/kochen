#include "ui/kochen_studio.h"
#include "core/kochen_codec.h"
#include "core/kochen_dag.h"
#include "core/kochen_data.h"
#include "core/kochen_physics.h"
#include "core/kochen_project.h"
#include "core/kochen_solver.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define COL_BG          0x11141BFFu
#define COL_RACK        0x191E28FFu
#define COL_CARD        0x222938FFu
#define COL_CARD_HI     0x2D374BFFu
#define COL_ACCENT      0xF4B942FFu
#define COL_PRO         0x6CCFF6FFu
#define COL_OK          0x7ED957FFu
#define COL_WARN        0xE06D53FFu
#define COL_BRIDGE      0xC792EAFFu
#define COL_TEXT        0xEEF2F7FFu
#define COL_MUTED       0x9AA5B8FFu

static const uint32_t DIET_CHIP_MASKS[5] = {
    KOCHEN_FILTER_VEGAN, KOCHEN_FILTER_VEGETARIAN,
    KOCHEN_DIET_DAIRY, KOCHEN_DIET_GLUTEN, KOCHEN_DIET_NUT
};
static const char *DIET_CHIP_KEYS[5] = {
    "ui.diet_vegan", "ui.diet_veg", "ui.diet_df", "ui.diet_gf", "ui.diet_nf"
};

static void refresh_studio_dag(kochen_studio_t *st, bool play_chord) {
    kochen_dag_compile(st->db, &st->patch, &st->dag);
    kochen_project_dag(st->db, &st->patch, &st->dag, st->view_mode, st->lang, &st->proj);
    kochen_patch_encode(&st->patch, st->last_patch_code, sizeof(st->last_patch_code));
    if (play_chord && st->audio)
        kochen_audio_trigger_patch(st->audio, &st->dag.total_diag);
}

kochen_status_t kochen_studio_init(
    kochen_studio_t *st,
    const kochen_db_t *db,
    kochen_audio_t *audio,
    float width,
    float height
) {
    if (!st || !db) return KOCHEN_ERR_RANGE;
    memset(st, 0, sizeof(*st));
    st->db = db;
    st->audio = audio;
    st->view_mode = KOCHEN_MODE_HOME;
    st->lang = KOCHEN_LANG_EN;
    st->active_knob = -1;
    st->picker_slot = -1;
    kochen_studio_resize(st, width, height);

    kochen_status_t rc = kochen_patch_init(db, &st->patch, 0, -1, 0, KOCHEN_MODE_HOME);
    if (rc != KOCHEN_OK) return rc;
    rc = kochen_patch_solve(db, &st->patch);
    if (rc != KOCHEN_OK) return rc;
    refresh_studio_dag(st, false);
    return KOCHEN_OK;
}

void kochen_studio_resize(kochen_studio_t *st, float width, float height) {
    if (!st) return;
    st->viewport_w = fmaxf(320.0f, width);
    st->viewport_h = fmaxf(480.0f, height);
    if (!st->touch_override_set)
        st->touch_mode = (st->viewport_w < 960.0f);
}

kochen_status_t kochen_studio_load_patch_code(kochen_studio_t *st, const char *code_or_url) {
    if (!st || !code_or_url) return KOCHEN_ERR_RANGE;
    kochen_patch_t loaded;
    kochen_status_t rc = kochen_patch_decode(st->db, code_or_url, &loaded);
    if (rc != KOCHEN_OK) return rc;
    st->patch = loaded;
    st->view_mode = loaded.solve_mode;
    st->picker_slot = -1;
    refresh_studio_dag(st, true);
    return KOCHEN_OK;
}

static void push_widget(kochen_studio_t *st, kochen_widget_kind_t kind, int16_t param, float x, float y, float w, float h) {
    if (st->widget_count >= KOCHEN_MAX_WIDGETS) return;
    kochen_widget_t *wg = &st->widgets[st->widget_count++];
    wg->kind = kind; wg->param = param;
    wg->x = x; wg->y = y; wg->w = w; wg->h = h;
}

static void dl_rect(kochen_draw_list_t *dl, float x, float y, float w, float h, uint32_t rgba) {
    if (!dl || dl->count >= KOCHEN_MAX_DRAW_CMDS) return;
    kochen_draw_cmd_t *c = &dl->cmds[dl->count++];
    memset(c, 0, sizeof(*c));
    c->kind = KOCHEN_CMD_RECT; c->x = x; c->y = y; c->w = w; c->h = h; c->rgba = rgba;
}

static void dl_line(kochen_draw_list_t *dl, float x0, float y0, float x1, float y1, float thick, uint32_t rgba) {
    if (!dl || dl->count >= KOCHEN_MAX_DRAW_CMDS) return;
    kochen_draw_cmd_t *c = &dl->cmds[dl->count++];
    memset(c, 0, sizeof(*c));
    c->kind = KOCHEN_CMD_LINE; c->x = x0; c->y = y0; c->w = x1; c->h = y1;
    c->value01 = thick; c->rgba = rgba;
}

static void dl_knob(kochen_draw_list_t *dl, float cx, float cy, float r, float val01, uint32_t rgba, const char *lbl) {
    if (!dl || dl->count >= KOCHEN_MAX_DRAW_CMDS) return;
    kochen_draw_cmd_t *c = &dl->cmds[dl->count++];
    memset(c, 0, sizeof(*c));
    c->kind = KOCHEN_CMD_KNOB; c->x = cx; c->y = cy; c->w = r; c->h = r;
    c->value01 = fminf(1.0f, fmaxf(0.0f, val01)); c->rgba = rgba;
    if (lbl) snprintf(c->text, sizeof(c->text), "%s", lbl);
}

static void dl_text(kochen_draw_list_t *dl, float x, float y, uint8_t sz, uint32_t rgba, const char *txt) {
    if (!dl || !txt || !txt[0] || dl->count >= KOCHEN_MAX_DRAW_CMDS) return;
    kochen_draw_cmd_t *c = &dl->cmds[dl->count++];
    memset(c, 0, sizeof(*c));
    c->kind = KOCHEN_CMD_TEXT; c->x = x; c->y = y; c->font_size = sz; c->rgba = rgba;
    snprintf(c->text, sizeof(c->text), "%s", txt);
}

static void draw_btn(
    kochen_studio_t *st, kochen_draw_list_t *dl,
    kochen_widget_kind_t kind, int16_t param,
    float x, float y, float w, float h,
    uint32_t bg, uint32_t fg, const char *label
) {
    dl_rect(dl, x, y, w, h, bg);
    dl_text(dl, x + 8.0f, y + h * 0.5f + 5.0f, 0, fg, label);
    push_widget(st, kind, param, x, y, w, h);
}

const kochen_widget_t *kochen_studio_find_widget(
    const kochen_studio_t *st,
    kochen_widget_kind_t kind,
    int16_t param
) {
    if (!st) return NULL;
    for (uint16_t i = 0; i < st->widget_count; i++)
        if (st->widgets[i].kind == kind && st->widgets[i].param == param)
            return &st->widgets[i];
    return NULL;
}

static void build_patch_bay(kochen_studio_t *st, kochen_draw_list_t *dl, float rx, float ry, float rw, float rh) {
    dl_rect(dl, rx, ry, rw, rh, COL_RACK);
    dl_text(dl, rx + 12.0f, ry + 24.0f, 1, COL_ACCENT, kochen_tr(st->db, st->lang, "ui.patch_bay"));

    float row_h = st->touch_mode ? 52.0f : 42.0f;
    float y = ry + 36.0f;
    float slot_centers_y[KOCHEN_MAX_PATCH_SLOTS] = {0};

    for (uint8_t i = 0; i < st->patch.slot_count; i++) {
        if (y + row_h > ry + rh - 6.0f) break;
        const kochen_patch_slot_t *s = &st->patch.slots[i];
        bool filled = (s->ingredient_id >= 0);
        uint32_t card_col = (s->component_idx == 1) ? 0x26243AFFu : COL_CARD;
        dl_rect(dl, rx + 8.0f, y, rw - 16.0f, row_h - 4.0f, card_col);
        slot_centers_y[i] = y + (row_h - 4.0f) * 0.5f;

        float btn_w = st->touch_mode ? 48.0f : 34.0f;
        float pick_w = rw - 24.0f - 2.0f * (btn_w + 4.0f);
        char line[140], key[64];
        if (filled) {
            snprintf(key, sizeof(key), "ing.%s", st->db->ingredients[s->ingredient_id].slug);
            snprintf(line, sizeof(line), "%s  (%.0fg)", kochen_tr(st->db, st->lang, key), s->mass_g);
        } else {
            snprintf(line, sizeof(line), "[%s]", kochen_tr(st->db, st->lang, "ui.none"));
        }
        draw_btn(st, dl, KOCHEN_WDG_SLOT_PICK, (int16_t)i, rx + 10.0f, y + 2.0f, pick_w, row_h - 8.0f,
                 COL_CARD_HI, COL_TEXT, line);
        draw_btn(st, dl, KOCHEN_WDG_SLOT_LOCK, (int16_t)i, rx + 14.0f + pick_w, y + 2.0f, btn_w, row_h - 8.0f,
                 s->locked ? COL_ACCENT : COL_CARD_HI, s->locked ? COL_BG : COL_MUTED, s->locked ? "LOCK" : "open");
        draw_btn(st, dl, KOCHEN_WDG_SLOT_REROLL, (int16_t)i, rx + 18.0f + pick_w + btn_w, y + 2.0f, btn_w, row_h - 8.0f,
                 COL_CARD_HI, COL_PRO, "MUT");
        y += row_h;
    }

    /* Glowing volatile aroma-bridge cables connecting slots sharing dominant volatiles */
    for (uint8_t i = 0; i < st->patch.slot_count; i++) {
        if (st->patch.slots[i].ingredient_id < 0 || slot_centers_y[i] <= 0.0f) continue;
        uint32_t vi = st->db->ingredients[st->patch.slots[i].ingredient_id].volatile_mask;
        for (uint8_t j = i + 1; j < st->patch.slot_count; j++) {
            if (st->patch.slots[j].ingredient_id < 0 || slot_centers_y[j] <= 0.0f) continue;
            uint32_t vj = st->db->ingredients[st->patch.slots[j].ingredient_id].volatile_mask;
            if (vi & vj) {
                dl_line(dl, rx + 5.0f, slot_centers_y[i], rx + 5.0f, slot_centers_y[j], 2.5f, COL_BRIDGE);
                break;
            }
        }
    }
}

static void build_knobs_and_scope(kochen_studio_t *st, kochen_draw_list_t *dl, float rx, float ry, float rw, float rh) {
    dl_rect(dl, rx, ry, rw, rh, COL_RACK);
    dl_text(dl, rx + 12.0f, ry + 24.0f, 1, COL_ACCENT, kochen_tr(st->db, st->lang, "ui.knobs_scope"));

    static const char *KNOB_KEYS[7] = {
        "ui.novelty", "ui.sens_rich", "ui.sens_bright", "ui.sens_umami",
        "ui.sens_heat", "ui.sens_sweet", "ui.sens_crunch"
    };
    float knob_r = st->touch_mode ? 28.0f : 24.0f;
    float hit = fmaxf(48.0f, knob_r * 2.2f);
    for (int k = 0; k < 7; k++) {
        int row = (k == 0) ? 0 : (1 + (k - 1) / 3);
        int col = (k == 0) ? 1 : ((k - 1) % 3);
        float cx = rx + rw * ((float)(col * 2 + 1) / 6.0f);
        float cy = ry + 66.0f + (float)row * (knob_r * 2.95f);
        float val = (k == 0) ? st->patch.novelty : st->patch.target_sensory[k - 1];
        uint32_t kc = (k == 0) ? COL_BRIDGE : COL_ACCENT;
        char lbl[64];
        snprintf(lbl, sizeof(lbl), "%s %d%%", kochen_tr(st->db, st->lang, KNOB_KEYS[k]), (int)roundf(val * 100.0f));
        dl_knob(dl, cx, cy, knob_r, val, kc, lbl);
        push_widget(st, KOCHEN_WDG_KNOB, (int16_t)k, cx - hit * 0.5f, cy - hit * 0.5f, hit, hit);
    }

    /* Live Patch Oscilloscope */
    float sy = ry + 66.0f + 3.0f * (knob_r * 2.95f) + 6.0f;
    float sh = fmaxf(90.0f, (ry + rh) - sy - 10.0f);
    dl_rect(dl, rx + 10.0f, sy, rw - 20.0f, sh, 0x0D1017FFu);

    const kochen_diagnostics_t *d = &st->dag.total_diag;
    bool all_ok = d->emulsion_stable && d->ph_in_window && d->hydration_in_window && d->gel_in_window;
    uint32_t trace_col = all_ok ? COL_OK : COL_WARN;

    /* Waveform trace driven by 6 sensory harmonics + stability */
    int segs = 28;
    float wx0 = rx + 18.0f, ww = rw - 36.0f, wy_mid = sy + sh * 0.38f, amp = sh * 0.22f;
    float prev_x = wx0, prev_y = wy_mid;
    for (int s = 1; s <= segs; s++) {
        float t = (float)s / (float)segs;
        float wv = 0.45f * (0.3f + d->sensory[0]) * sinf(t * 6.283f * 2.0f) +
                   0.30f * d->sensory[2] * sinf(t * 6.283f * 3.0f + 0.6f) +
                   0.25f * d->sensory[1] * cosf(t * 6.283f * 5.0f);
        float nx = wx0 + t * ww;
        float ny = wy_mid - wv * amp;
        dl_line(dl, prev_x, prev_y, nx, ny, 2.2f, trace_col);
        prev_x = nx; prev_y = ny;
    }

    char tele[160];
    snprintf(tele, sizeof(tele), "pH %.2f | NaCl %.2f%% | Umami %.2f | Emul %s | Bridges %u",
             d->mixture_ph, d->nacl_pct, d->umami_synergy,
             d->emulsion_stable ? "OK" : "SPLIT", kochen_popcount32(d->shared_volatile_mask));
    dl_text(dl, rx + 16.0f, sy + sh - 28.0f, 0, COL_PRO, tele);

    char guards[160] = "Guards:";
    if (st->dag.active_guards & KOCHEN_GUARD_MAILLARD_DRY) strcat(guards, " [MaillardDry]");
    if (st->dag.active_guards & KOCHEN_GUARD_ENZYME_PROTEASE) strcat(guards, " [EnzymeScald]");
    if (st->dag.active_guards & KOCHEN_GUARD_PECTIN_ACID) strcat(guards, " [PectinDelay]");
    if (st->dag.active_guards & KOCHEN_GUARD_CASEIN_CEIL) strcat(guards, " [Casein<68C]");
    if (st->dag.active_guards & KOCHEN_GUARD_LIPID_SHORTEN) strcat(guards, " [LipidDelay]");
    if (st->dag.active_guards & KOCHEN_GUARD_FOAM_NO_LIPID) strcat(guards, " [CleanFoam]");
    if (st->dag.active_guards == 0) strcat(guards, " [Nominal]");
    dl_text(dl, rx + 16.0f, sy + sh - 10.0f, 0, COL_ACCENT, guards);
}

static void build_recipe_dag(kochen_studio_t *st, kochen_draw_list_t *dl, float rx, float ry, float rw, float rh) {
    dl_rect(dl, rx, ry, rw, rh, COL_RACK);
    dl_text(dl, rx + 12.0f, ry + 22.0f, 1, COL_TEXT, st->proj.dish_title);
    dl_text(dl, rx + 12.0f, ry + 40.0f, 0, COL_MUTED, st->proj.subtitle);

    float y = ry + 50.0f - st->recipe_scroll_y;
    if (st->proj.show_home_resolve_badge) {
        float bh = st->touch_mode ? 48.0f : 34.0f;
        draw_btn(st, dl, KOCHEN_WDG_HOME_RESOLVE, 0, rx + 10.0f, ry + 48.0f, rw - 20.0f, bh,
                 COL_WARN, COL_BG, kochen_tr(st->db, st->lang, "ui.resolve_home"));
        y += bh + 8.0f;
    }

    for (uint8_t i = 0; i < st->proj.step_count; i++) {
        const kochen_projected_step_t *sp = &st->proj.steps[i];
        float ch = 58.0f;
        if (y + ch >= ry + 48.0f && y <= ry + rh - 8.0f) {
            dl_rect(dl, rx + 10.0f, y, rw - 20.0f, ch - 4.0f, COL_CARD);
            char hdr[200];
            snprintf(hdr, sizeof(hdr), "%u. %s  [%s]", (unsigned)(i + 1), sp->title, sp->metrics_badge);
            dl_text(dl, rx + 16.0f, y + 18.0f, 0, st->view_mode == KOCHEN_MODE_PRO ? COL_PRO : COL_ACCENT, hdr);
            dl_text(dl, rx + 16.0f, y + 38.0f, 0, COL_TEXT, sp->instruction);
        }
        y += ch;
    }
}

static void build_picker_modal(kochen_studio_t *st, kochen_draw_list_t *dl) {
    if (st->picker_slot < 0 || st->picker_slot >= (int8_t)st->patch.slot_count) return;
    float mw = fminf(820.0f, st->viewport_w - 16.0f);
    float mh = fminf(680.0f, st->viewport_h - 24.0f);
    float mx = (st->viewport_w - mw) * 0.5f, my = (st->viewport_h - mh) * 0.5f;
    dl_rect(dl, mx, my, mw, mh, 0x161B26F8u);

    float close_h = st->touch_mode ? 48.0f : 32.0f;
    draw_btn(st, dl, KOCHEN_WDG_PICKER_CLOSE, 0, mx + mw - 84.0f, my + 8.0f, 76.0f, close_h, COL_WARN, COL_BG, "X Close");

    int ncols = st->touch_mode ? 2 : 3;
    float item_h = st->touch_mode ? 48.0f : 30.0f;
    kochen_role_t role = st->patch.slots[st->picker_slot].role;
    float ix = mx + 12.0f, iy = my + close_h + 14.0f;
    float col_w = (mw - 24.0f - (float)(ncols - 1) * 6.0f) / (float)ncols;
    int shown = 0;
    for (uint16_t g = 0; g < st->db->ingredient_count; g++) {
        const kochen_ingredient_t *ing = &st->db->ingredients[g];
        if ((ing->role_mask & (1u << role)) == 0) continue;
        if ((ing->diet_mask & st->patch.forbidden_diet_mask) != 0) continue;
        if (st->view_mode == KOCHEN_MODE_HOME && (ing->diet_mask & KOCHEN_DIET_MODERNIST)) continue;

        float bx = ix + (float)(shown % ncols) * (col_w + 6.0f);
        float by = iy + (float)(shown / ncols) * (item_h + 4.0f);
        if (by + item_h > my + mh - 6.0f) break;
        char key[64];
        snprintf(key, sizeof(key), "ing.%s", ing->slug);
        draw_btn(st, dl, KOCHEN_WDG_PICKER_ITEM, (int16_t)g, bx, by, col_w, item_h,
                 COL_CARD_HI, COL_TEXT, kochen_tr(st->db, st->lang, key));
        shown++;
    }
}

void kochen_studio_build_frame(kochen_studio_t *st, float dt_sec, kochen_draw_list_t *dl) {
    if (!st || !dl) return;
    dl->count = 0;
    st->widget_count = 0;
    if (st->toast_timer > 0.0f) st->toast_timer = fmaxf(0.0f, st->toast_timer - dt_sec);

    float W = st->viewport_w, H = st->viewport_h;
    dl_rect(dl, 0, 0, W, H, COL_BG);

    float bh = st->touch_mode ? 48.0f : 32.0f;
    float top_h = st->touch_mode ? (bh * 3.0f + 24.0f) : (bh * 2.0f + 22.0f);
    dl_rect(dl, 0, 0, W, top_h, COL_RACK);

    char key[64], buf[96];
    snprintf(key, sizeof(key), "arch.%s", st->db->archetypes[st->patch.primary_arch_id].slug);
    snprintf(buf, sizeof(buf), "Osc1: %s >", kochen_tr(st->db, st->lang, key));
    float x = 8.0f, y1 = 6.0f;
    float w_osc = st->touch_mode ? (W - 22.0f) * 0.5f : 220.0f;
    draw_btn(st, dl, KOCHEN_WDG_PRIMARY_NEXT, 0, x, y1, w_osc, bh, COL_CARD_HI, COL_ACCENT, buf);
    x += w_osc + 6.0f;

    if (st->patch.companion_arch_id >= 0) {
        snprintf(key, sizeof(key), "arch.%s", st->db->archetypes[st->patch.companion_arch_id].slug);
        snprintf(buf, sizeof(buf), "Osc2: %s >", kochen_tr(st->db, st->lang, key));
    } else {
        snprintf(buf, sizeof(buf), "Osc2: %s >", kochen_tr(st->db, st->lang, "ui.none"));
    }
    draw_btn(st, dl, KOCHEN_WDG_COMP_NEXT, 0, x, y1, w_osc, bh, COL_CARD_HI, COL_BRIDGE, buf);

    float y2 = y1 + bh + 6.0f;
    float x2 = 8.0f;
    float pw = st->touch_mode ? (W - 28.0f) * 0.38f : 148.0f;
    float mw_btn = st->touch_mode ? (W - 28.0f) * 0.30f : 96.0f;
    float mut_w = st->touch_mode ? (W - 28.0f) * 0.32f : 108.0f;
    snprintf(key, sizeof(key), "pantry.%s", st->db->pantries[st->patch.pantry_id].slug);
    draw_btn(st, dl, KOCHEN_WDG_PANTRY_NEXT, 0, x2, y2, pw, bh, COL_CARD_HI, COL_TEXT, kochen_tr(st->db, st->lang, key));
    x2 += pw + 6.0f;

    draw_btn(st, dl, KOCHEN_WDG_MODE_TOGGLE, 0, x2, y2, mw_btn, bh,
             st->view_mode == KOCHEN_MODE_PRO ? COL_PRO : COL_ACCENT, COL_BG,
             kochen_tr(st->db, st->lang, st->view_mode == KOCHEN_MODE_PRO ? "ui.mode_pro" : "ui.mode_home"));
    x2 += mw_btn + 6.0f;

    draw_btn(st, dl, KOCHEN_WDG_MUTATE, 0, x2, y2, mut_w, bh, COL_OK, COL_BG, kochen_tr(st->db, st->lang, "ui.mutate"));
    x2 += mut_w + 6.0f;

    static const char *LANG_LBL[3] = {"EN", "DE", "PL"};
    float util_y = st->touch_mode ? (y2 + bh + 6.0f) : y1;
    float util_x = st->touch_mode ? 8.0f : (x + w_osc + 8.0f);
    float uw = st->touch_mode ? ((W - 40.0f) / 5.0f) : 66.0f;
    draw_btn(st, dl, KOCHEN_WDG_LANG_CYCLE, 0, util_x, util_y, uw, bh, COL_CARD_HI, COL_TEXT, LANG_LBL[st->lang]);
    util_x += uw + 6.0f;
    draw_btn(st, dl, KOCHEN_WDG_MUTE_TOGGLE, 0, util_x, util_y, uw, bh, COL_CARD_HI,
             (st->audio && st->audio->muted) ? COL_WARN : COL_OK,
             (st->audio && st->audio->muted) ? "MUTE" : "AUDIO");
    util_x += uw + 6.0f;
    draw_btn(st, dl, KOCHEN_WDG_TOUCH_TOGGLE, 0, util_x, util_y, uw, bh, COL_CARD_HI, COL_MUTED,
             st->touch_mode ? "TOUCH" : "DESK");
    util_x += uw + 6.0f;
    draw_btn(st, dl, KOCHEN_WDG_SHARE_PATCH, 0, util_x, util_y, uw, bh, COL_CARD_HI, COL_PRO, "SHARE");
    util_x += uw + 6.0f;
    draw_btn(st, dl, KOCHEN_WDG_EXPORT_MD, 0, util_x, util_y, uw, bh, COL_CARD_HI, COL_ACCENT, "MD");

    if (!st->touch_mode) {
        for (int d = 0; d < 5; d++) {
            bool on = (st->patch.forbidden_diet_mask & DIET_CHIP_MASKS[d]) == DIET_CHIP_MASKS[d];
            draw_btn(st, dl, KOCHEN_WDG_DIET_CHIP, (int16_t)d, x2, y2, 86.0f, bh,
                     on ? COL_ACCENT : COL_CARD, on ? COL_BG : COL_MUTED, kochen_tr(st->db, st->lang, DIET_CHIP_KEYS[d]));
            x2 += 90.0f;
        }
    }

    float body_y = top_h + 6.0f;
    if (st->touch_mode) {
        float tab_h = 52.0f;
        float body_h = H - body_y - tab_h - 8.0f;
        if (st->mobile_tab == 0) build_patch_bay(st, dl, 6.0f, body_y, W - 12.0f, body_h);
        else if (st->mobile_tab == 1) build_knobs_and_scope(st, dl, 6.0f, body_y, W - 12.0f, body_h);
        else build_recipe_dag(st, dl, 6.0f, body_y, W - 12.0f, body_h);

        static const char *TAB_KEYS[3] = {"ui.patch_bay", "ui.knobs_scope", "ui.recipe_dag"};
        float tw = (W - 24.0f) / 3.0f;
        for (int t = 0; t < 3; t++) {
            draw_btn(st, dl, KOCHEN_WDG_MOBILE_TAB, (int16_t)t, 6.0f + (float)t * (tw + 6.0f), H - tab_h - 4.0f, tw, tab_h,
                     st->mobile_tab == t ? COL_ACCENT : COL_CARD_HI,
                     st->mobile_tab == t ? COL_BG : COL_TEXT, kochen_tr(st->db, st->lang, TAB_KEYS[t]));
        }
    } else {
        float body_h = H - body_y - 8.0f;
        float w_left = W * 0.30f, w_mid = W * 0.34f, w_right = W - w_left - w_mid - 24.0f;
        build_patch_bay(st, dl, 6.0f, body_y, w_left, body_h);
        build_knobs_and_scope(st, dl, 12.0f + w_left, body_y, w_mid, body_h);
        build_recipe_dag(st, dl, 18.0f + w_left + w_mid, body_y, w_right, body_h);
    }

    if (st->picker_slot >= 0) build_picker_modal(st, dl);
    if (st->toast_timer > 0.0f && st->toast_text[0])
        dl_text(dl, W * 0.5f - 90.0f, H - 14.0f, 1, COL_OK, st->toast_text);
}

static void cycle_companion(kochen_studio_t *st) {
    int cur = st->patch.companion_arch_id;
    for (int step = 1; step <= (int)st->db->archetype_count + 1; step++) {
        int next = cur + 1;
        if (next >= (int)st->db->archetype_count) { cur = -1; break; }
        cur = next;
        if (cur != (int)st->patch.primary_arch_id && st->db->archetypes[cur].is_companion_capable) break;
    }
    kochen_patch_set_archetypes(st->db, &st->patch, st->patch.primary_arch_id, (int8_t)cur);
    kochen_patch_solve(st->db, &st->patch);
}

bool kochen_studio_pointer_down(kochen_studio_t *st, float x, float y, bool is_touch) {
    if (!st) return false;
    if (is_touch && !st->touch_override_set && !st->touch_mode) {
        st->touch_mode = true;
        kochen_draw_list_t tmp;
        kochen_studio_build_frame(st, 0.0f, &tmp);
    }

    /* Iterate in reverse so modal overlays receive hits first */
    for (int i = (int)st->widget_count - 1; i >= 0; i--) {
        const kochen_widget_t *w = &st->widgets[i];
        if (x < w->x || x > w->x + w->w || y < w->y || y > w->y + w->h) continue;
        if (st->picker_slot >= 0 && w->kind != KOCHEN_WDG_PICKER_ITEM && w->kind != KOCHEN_WDG_PICKER_CLOSE)
            continue;

        switch (w->kind) {
            case KOCHEN_WDG_PRIMARY_NEXT: {
                uint8_t next = (uint8_t)((st->patch.primary_arch_id + 1u) % st->db->archetype_count);
                int8_t comp = (st->patch.companion_arch_id == (int8_t)next) ? -1 : st->patch.companion_arch_id;
                kochen_patch_set_archetypes(st->db, &st->patch, next, comp);
                st->patch.solve_mode = st->view_mode;
                kochen_patch_solve(st->db, &st->patch);
                refresh_studio_dag(st, true);
                return true;
            }
            case KOCHEN_WDG_COMP_NEXT:
                st->patch.solve_mode = st->view_mode;
                cycle_companion(st);
                refresh_studio_dag(st, true);
                return true;
            case KOCHEN_WDG_PANTRY_NEXT:
                st->patch.pantry_id = (uint8_t)((st->patch.pantry_id + 1u) % st->db->pantry_count);
                st->patch.solve_mode = st->view_mode;
                kochen_patch_solve(st->db, &st->patch);
                refresh_studio_dag(st, true);
                return true;
            case KOCHEN_WDG_MODE_TOGGLE:
                /* Toggling view mode NEVER mutates the underlying Recipe DAG */
                st->view_mode = (st->view_mode == KOCHEN_MODE_HOME) ? KOCHEN_MODE_PRO : KOCHEN_MODE_HOME;
                kochen_project_dag(st->db, &st->patch, &st->dag, st->view_mode, st->lang, &st->proj);
                if (st->audio) kochen_audio_trigger_detent(st->audio, (float)st->view_mode);
                return true;
            case KOCHEN_WDG_HOME_RESOLVE:
                kochen_resolve_home_pantry(st->db, &st->patch);
                refresh_studio_dag(st, true);
                return true;
            case KOCHEN_WDG_MUTATE:
                st->patch.seed = st->patch.seed * 1664525u + 1013904223u;
                st->patch.solve_mode = st->view_mode;
                kochen_patch_solve(st->db, &st->patch);
                refresh_studio_dag(st, true);
                return true;
            case KOCHEN_WDG_DIET_CHIP: {
                uint32_t m = DIET_CHIP_MASKS[w->param];
                if ((st->patch.forbidden_diet_mask & m) == m) st->patch.forbidden_diet_mask &= ~m;
                else st->patch.forbidden_diet_mask |= m;
                /* Unlock any slot that now violates the active dietary constraint */
                for (uint8_t s = 0; s < st->patch.slot_count; s++) {
                    int16_t id = st->patch.slots[s].ingredient_id;
                    if (id >= 0 && (st->db->ingredients[id].diet_mask & st->patch.forbidden_diet_mask)) {
                        st->patch.slots[s].locked = 0;
                        st->patch.slots[s].ingredient_id = -1;
                    }
                }
                st->patch.solve_mode = st->view_mode;
                kochen_patch_solve(st->db, &st->patch);
                refresh_studio_dag(st, true);
                return true;
            }
            case KOCHEN_WDG_LANG_CYCLE:
                st->lang = (kochen_lang_t)((st->lang + 1) % KOCHEN_LANG_COUNT);
                kochen_project_dag(st->db, &st->patch, &st->dag, st->view_mode, st->lang, &st->proj);
                if (st->audio) kochen_audio_trigger_detent(st->audio, 0.5f);
                return true;
            case KOCHEN_WDG_TOUCH_TOGGLE:
                st->touch_override_set = true;
                st->touch_mode = !st->touch_mode;
                if (st->audio) kochen_audio_trigger_detent(st->audio, 0.7f);
                return true;
            case KOCHEN_WDG_MUTE_TOGGLE:
                if (st->audio) kochen_audio_set_muted(st->audio, !st->audio->muted);
                return true;
            case KOCHEN_WDG_SHARE_PATCH:
                kochen_patch_encode(&st->patch, st->last_patch_code, sizeof(st->last_patch_code));
                st->share_requested = true;
                snprintf(st->toast_text, sizeof(st->toast_text), "%s", kochen_tr(st->db, st->lang, "ui.copied"));
                st->toast_timer = 2.0f;
                return true;
            case KOCHEN_WDG_EXPORT_MD:
                kochen_patch_encode(&st->patch, st->last_patch_code, sizeof(st->last_patch_code));
                kochen_export_markdown(st->db, &st->patch, &st->dag, &st->proj,
                                       st->last_patch_code, st->last_markdown, sizeof(st->last_markdown));
                st->export_md_requested = true;
                snprintf(st->toast_text, sizeof(st->toast_text), "%s", kochen_tr(st->db, st->lang, "ui.copied"));
                st->toast_timer = 2.0f;
                return true;
            case KOCHEN_WDG_MOBILE_TAB:
                st->mobile_tab = (uint8_t)w->param;
                if (st->audio) kochen_audio_trigger_detent(st->audio, 0.3f * (float)w->param);
                return true;
            case KOCHEN_WDG_SLOT_PICK:
                st->picker_slot = (int8_t)w->param;
                return true;
            case KOCHEN_WDG_SLOT_LOCK:
                st->patch.slots[w->param].locked ^= 1u;
                refresh_studio_dag(st, false);
                if (st->audio) kochen_audio_trigger_detent(st->audio, (float)st->patch.slots[w->param].locked);
                return true;
            case KOCHEN_WDG_SLOT_REROLL:
                st->patch.solve_mode = st->view_mode;
                kochen_patch_reroll_slot(st->db, &st->patch, (uint8_t)w->param);
                refresh_studio_dag(st, true);
                return true;
            case KOCHEN_WDG_KNOB:
                st->active_knob = (int8_t)w->param;
                st->drag_start_y = y;
                st->drag_start_val = (w->param == 0) ? st->patch.novelty : st->patch.target_sensory[w->param - 1];
                return true;
            case KOCHEN_WDG_PICKER_ITEM:
                if (st->picker_slot >= 0 && st->picker_slot < (int8_t)st->patch.slot_count) {
                    st->patch.slots[st->picker_slot].ingredient_id = w->param;
                    st->patch.slots[st->picker_slot].locked = 1;
                    st->picker_slot = -1;
                    kochen_patch_rebalance_masses(st->db, &st->patch);
                    refresh_studio_dag(st, true);
                }
                return true;
            case KOCHEN_WDG_PICKER_CLOSE:
                st->picker_slot = -1;
                return true;
            default:
                break;
        }
    }
    return false;
}

bool kochen_studio_pointer_move(kochen_studio_t *st, float x, float y) {
    (void)x;
    if (!st || st->active_knob < 0) return false;
    float delta = (st->drag_start_y - y) / 140.0f;
    float nv = fminf(1.0f, fmaxf(0.0f, st->drag_start_val + delta));
    float prev = (st->active_knob == 0) ? st->patch.novelty : st->patch.target_sensory[st->active_knob - 1];
    if (fabsf(nv - prev) < 0.005f) return false;

    if (st->active_knob == 0) {
        st->patch.novelty = nv;
        st->patch.solve_mode = st->view_mode;
        kochen_patch_solve(st->db, &st->patch);
    } else {
        st->patch.target_sensory[st->active_knob - 1] = nv;
        kochen_patch_rebalance_masses(st->db, &st->patch);
    }
    if ((int)(nv * 20.0f) != (int)(prev * 20.0f) && st->audio)
        kochen_audio_trigger_detent(st->audio, nv);
    refresh_studio_dag(st, false);
    return true;
}

void kochen_studio_pointer_up(kochen_studio_t *st) {
    if (st) st->active_knob = -1;
}

void kochen_studio_scroll(kochen_studio_t *st, float dy) {
    if (!st) return;
    float max_sc = fmaxf(0.0f, (float)st->proj.step_count * 58.0f - 220.0f);
    st->recipe_scroll_y = fminf(max_sc, fmaxf(0.0f, st->recipe_scroll_y - dy * 28.0f));
}
