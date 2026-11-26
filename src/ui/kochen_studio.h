#ifndef KOCHEN_STUDIO_H
#define KOCHEN_STUDIO_H

#include "core/kochen_types.h"
#include "ui/kochen_audio.h"

#define KOCHEN_MAX_WIDGETS    128
#define KOCHEN_MAX_DRAW_CMDS  512

typedef enum {
    KOCHEN_WDG_NONE = 0,
    KOCHEN_WDG_PRIMARY_PREV,
    KOCHEN_WDG_PRIMARY_NEXT,
    KOCHEN_WDG_COMP_PREV,
    KOCHEN_WDG_COMP_NEXT,
    KOCHEN_WDG_PANTRY_PREV,
    KOCHEN_WDG_PANTRY_NEXT,
    KOCHEN_WDG_MODE_TOGGLE,
    KOCHEN_WDG_HOME_RESOLVE,
    KOCHEN_WDG_MUTATE,
    KOCHEN_WDG_DIET_CHIP,      /* param = 0..4 */
    KOCHEN_WDG_LANG_CYCLE,
    KOCHEN_WDG_TOUCH_TOGGLE,
    KOCHEN_WDG_MUTE_TOGGLE,
    KOCHEN_WDG_SHARE_PATCH,
    KOCHEN_WDG_EXPORT_MD,
    KOCHEN_WDG_MOBILE_TAB,     /* param = 0..2 */
    KOCHEN_WDG_SLOT_PICK,      /* param = slot_idx */
    KOCHEN_WDG_SLOT_LOCK,      /* param = slot_idx */
    KOCHEN_WDG_SLOT_REROLL,    /* param = slot_idx */
    KOCHEN_WDG_KNOB,           /* param = 0 (novelty), 1..6 (sensory) */
    KOCHEN_WDG_PICKER_ITEM,    /* param = ingredient_id */
    KOCHEN_WDG_PICKER_CLOSE
} kochen_widget_kind_t;

typedef struct {
    kochen_widget_kind_t kind;
    int16_t param;
    float x, y, w, h;
} kochen_widget_t;

typedef enum {
    KOCHEN_CMD_RECT = 0,
    KOCHEN_CMD_LINE = 1,
    KOCHEN_CMD_KNOB = 2,
    KOCHEN_CMD_TEXT = 3
} kochen_cmd_kind_t;

typedef struct {
    kochen_cmd_kind_t kind;
    float x, y, w, h;
    float value01; /* Used by KOCHEN_CMD_KNOB; thickness for KOCHEN_CMD_LINE */
    uint32_t rgba;
    uint8_t font_size; /* 0 = small 13px, 1 = body 15px, 2 = title 19px */
    char text[160];
} kochen_draw_cmd_t;

typedef struct {
    uint16_t count;
    kochen_draw_cmd_t cmds[KOCHEN_MAX_DRAW_CMDS];
} kochen_draw_list_t;

typedef struct {
    const kochen_db_t *db;
    kochen_audio_t *audio;

    kochen_patch_t patch;
    kochen_dag_t dag;
    kochen_projection_t proj;

    kochen_mode_t view_mode;
    kochen_lang_t lang;

    float viewport_w;
    float viewport_h;
    bool touch_mode;
    bool touch_override_set;
    uint8_t mobile_tab; /* 0 = Patch Bay, 1 = Knobs & Scope, 2 = Recipe DAG */

    int8_t active_knob; /* -1 if not dragging knob, else 0..6 */
    float drag_start_y;
    float drag_start_val;

    int8_t picker_slot; /* -1 if closed, else slot_idx */
    float recipe_scroll_y;
    float toast_timer;
    char toast_text[96];

    bool share_requested;
    bool export_md_requested;
    char last_patch_code[256];
    char last_markdown[8192];

    uint16_t widget_count;
    kochen_widget_t widgets[KOCHEN_MAX_WIDGETS];
} kochen_studio_t;

kochen_status_t kochen_studio_init(
    kochen_studio_t *st,
    const kochen_db_t *db,
    kochen_audio_t *audio,
    float width,
    float height
);

void kochen_studio_resize(kochen_studio_t *st, float width, float height);
kochen_status_t kochen_studio_load_patch_code(kochen_studio_t *st, const char *code_or_url);

void kochen_studio_build_frame(kochen_studio_t *st, float dt_sec, kochen_draw_list_t *out_dl);

bool kochen_studio_pointer_down(kochen_studio_t *st, float x, float y, bool is_touch);
bool kochen_studio_pointer_move(kochen_studio_t *st, float x, float y);
void kochen_studio_pointer_up(kochen_studio_t *st);
void kochen_studio_scroll(kochen_studio_t *st, float dy);

const kochen_widget_t *kochen_studio_find_widget(
    const kochen_studio_t *st,
    kochen_widget_kind_t kind,
    int16_t param
);

#endif /* KOCHEN_STUDIO_H */
