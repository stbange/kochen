#include "core/kochen_data.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void kochen_db_init(kochen_db_t *db) { memset(db, 0, sizeof(*db)); }

static bool parse_f32(const char *s, float lo, float hi, float *out) {
    char *end = NULL;
    float v = strtof(s, &end);
    if (end == s || (*end != '\0' && !isspace((unsigned char)*end)) || !isfinite(v) || v < lo || v > hi) return false;
    *out = v;
    return true;
}

static bool parse_u32(const char *s, uint32_t hi, uint32_t *out) {
    if (*s == '-') return false;
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 0);
    if (end == s || (*end != '\0' && !isspace((unsigned char)*end)) || v > hi) return false;
    *out = (uint32_t)v;
    return true;
}

static int split_delim(char *line, char delim, char **cols, int max_cols) {
    int n = 0;
    char *p = line;
    while (n < max_cols) {
        cols[n++] = p;
        char *d = strchr(p, delim);
        if (!d) break;
        *d = '\0';
        p = d + 1;
    }
    return n;
}

static char *trim_line(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\r' || s[len - 1] == '\n' || s[len - 1] == ' ')) s[--len] = '\0';
    return s;
}

int kochen_find_ingredient(const kochen_db_t *db, const char *slug) {
    for (uint16_t i = 0; i < db->ingredient_count; i++)
        if (strcmp(db->ingredients[i].slug, slug) == 0) return (int)i;
    return -1;
}

int kochen_find_archetype(const kochen_db_t *db, const char *slug) {
    for (uint8_t i = 0; i < db->archetype_count; i++)
        if (strcmp(db->archetypes[i].slug, slug) == 0) return (int)i;
    return -1;
}

int kochen_find_pantry(const kochen_db_t *db, const char *slug) {
    for (uint8_t i = 0; i < db->pantry_count; i++)
        if (strcmp(db->pantries[i].slug, slug) == 0) return (int)i;
    return -1;
}

const char *kochen_tr(const kochen_db_t *db, kochen_lang_t lang, const char *key) {
    if ((unsigned)lang >= KOCHEN_LANG_COUNT) lang = KOCHEN_LANG_EN;
    for (uint16_t i = 0; i < db->locale_count; i++) {
        if (strcmp(db->locales[i].key, key) == 0) {
            const char *t = db->locales[i].text[lang];
            return t[0] ? t : db->locales[i].text[KOCHEN_LANG_EN];
        }
    }
    return key;
}

kochen_status_t kochen_parse_pantries_tsv(kochen_db_t *db, const char *text) {
    char buf[256], *cols[4];
    const char *cur = text;
    while (*cur) {
        const char *nl = strchr(cur, '\n');
        size_t len = nl ? (size_t)(nl - cur) : strlen(cur);
        if (len >= sizeof(buf)) return KOCHEN_ERR_PARSE;
        memcpy(buf, cur, len); buf[len] = '\0';
        cur = nl ? nl + 1 : cur + len;
        char *line = trim_line(buf);
        if (!line[0] || line[0] == '#') continue;
        if (db->pantry_count >= KOCHEN_MAX_PANTRIES) return KOCHEN_ERR_CAPACITY;
        if (split_delim(line, '\t', cols, 4) != 2 || !cols[0][0] || strlen(cols[0]) >= KOCHEN_SLUG_LEN) return KOCHEN_ERR_PARSE;
        if (kochen_find_pantry(db, cols[0]) >= 0) return KOCHEN_ERR_PARSE;
        kochen_pantry_t *p = &db->pantries[db->pantry_count];
        if (!parse_f32(cols[1], 0.0f, 1.0f, &p->default_novelty_bias)) return KOCHEN_ERR_RANGE;
        p->id = db->pantry_count++;
        strcpy(p->slug, cols[0]);
    }
    return KOCHEN_OK;
}

kochen_status_t kochen_parse_ingredients_tsv(kochen_db_t *db, const char *text) {
    char buf[512], *c[36];
    const char *cur = text;
    while (*cur) {
        const char *nl = strchr(cur, '\n');
        size_t len = nl ? (size_t)(nl - cur) : strlen(cur);
        if (len >= sizeof(buf)) return KOCHEN_ERR_PARSE;
        memcpy(buf, cur, len); buf[len] = '\0';
        cur = nl ? nl + 1 : cur + len;
        char *line = trim_line(buf);
        if (!line[0] || line[0] == '#') continue;
        if (db->ingredient_count >= KOCHEN_MAX_INGREDIENTS) return KOCHEN_ERR_CAPACITY;
        if (split_delim(line, '\t', c, 36) != 34 || !c[0][0] || strlen(c[0]) >= KOCHEN_SLUG_LEN) return KOCHEN_ERR_PARSE;
        if (kochen_find_ingredient(db, c[0]) >= 0) return KOCHEN_ERR_PARSE;
        kochen_ingredient_t g = {0};
        uint32_t pc = 0, enz = 0, sol = 0, lab = 0, unit = 0;
        bool ok = parse_f32(c[1], 0, 100, &g.water_pct) && parse_f32(c[2], 0, 100, &g.fat_pct) &&
                  parse_f32(c[3], 0, 100, &g.fat_sat_pct) && parse_f32(c[4], 0, 320, &g.smoke_point_c) &&
                  parse_f32(c[5], 0, 100, &g.protein_pct) && parse_u32(c[6], 7, &pc) &&
                  parse_f32(c[7], 0, 100, &g.starch_pct) && parse_f32(c[8], 0, 1, &g.amylose_frac) &&
                  parse_f32(c[9], 0, 100, &g.sugar_pct) && parse_f32(c[10], 0, 1, &g.reducing_sugar_frac) &&
                  parse_f32(c[11], 0, 100, &g.fiber_pectin_pct) && parse_f32(c[12], 1.8f, 9.5f, &g.ph) &&
                  parse_f32(c[13], 0.01f, 1, &g.buffer_capacity) && parse_f32(c[14], 0, 100, &g.nacl_pct) &&
                  parse_f32(c[15], 0, 6000, &g.glutamate_mg) && parse_f32(c[16], 0, 4000, &g.nucleotide_mg) &&
                  parse_f32(c[17], 0, 60, &g.surfactant_power) && parse_f32(c[18], 0, 20, &g.surfactant_hlb) &&
                  parse_f32(c[19], 0, 80, &g.hydrocolloid_power) && parse_f32(c[20], 0, 110, &g.gel_set_c) &&
                  parse_u32(c[21], 0xFF, &enz) && parse_f32(c[22], 0, 120, &g.enzyme_kill_c) &&
                  parse_f32(c[23], 0, 1, &g.pungency_idx) && parse_f32(c[24], 0, 1, &g.bitterness_idx) &&
                  parse_f32(c[25], 0, 1, &g.crunch_potential) && parse_u32(c[26], 0x07, &sol) &&
                  parse_u32(c[27], 1, &lab) && parse_u32(c[28], 0xFFFFu, &g.volatile_mask) &&
                  parse_u32(c[29], 0x3FFFu, &g.role_mask) && parse_u32(c[30], 0xFFFFu, &g.pantry_mask) &&
                  parse_u32(c[31], 0x3FFu, &g.diet_mask) && parse_u32(c[32], 6, &unit) &&
                  parse_f32(c[33], 0.1f, 1000, &g.grams_per_unit);
        if (!ok || g.fat_sat_pct > g.fat_pct + 0.1f || g.role_mask == 0) return KOCHEN_ERR_RANGE;
        g.id = db->ingredient_count; strcpy(g.slug, c[0]);
        g.prot_class = (kochen_protein_class_t)pc; g.enzyme_mask = (uint8_t)enz;
        g.solubility_mask = (uint8_t)sol; g.heat_labile = (uint8_t)lab; g.home_unit = (kochen_home_unit_t)unit;
        db->ingredients[db->ingredient_count++] = g;
    }
    return KOCHEN_OK;
}

kochen_status_t kochen_parse_archetypes_ini(kochen_db_t *db, const char *text) {
    char buf[384], *v[8];
    kochen_archetype_t *a = NULL;
    const char *cur = text;
    while (*cur) {
        const char *nl = strchr(cur, '\n');
        size_t len = nl ? (size_t)(nl - cur) : strlen(cur);
        if (len >= sizeof(buf)) return KOCHEN_ERR_PARSE;
        memcpy(buf, cur, len); buf[len] = '\0';
        cur = nl ? nl + 1 : cur + len;
        char *line = trim_line(buf);
        if (!line[0] || line[0] == '#') continue;
        if (line[0] == '[') {
            char *rb = strchr(line, ']');
            if (!rb || rb == line + 1 || (size_t)(rb - line - 1) >= KOCHEN_SLUG_LEN) return KOCHEN_ERR_PARSE;
            *rb = '\0';
            if (kochen_find_archetype(db, line + 1) >= 0) return KOCHEN_ERR_PARSE;
            if (db->archetype_count >= KOCHEN_MAX_ARCHETYPES) return KOCHEN_ERR_CAPACITY;
            a = &db->archetypes[db->archetype_count];
            memset(a, 0, sizeof(*a));
            a->id = db->archetype_count++; strcpy(a->slug, line + 1);
            continue;
        }
        char *eq = strchr(line, '=');
        if (!a || !eq) return KOCHEN_ERR_PARSE;
        *eq = '\0'; char *k = trim_line(line), *val = trim_line(eq + 1);
        uint32_t u0 = 0, u1 = 0, u2 = 0;
        if (strcmp(k, "companion") == 0) {
            if (split_delim(val, ',', v, 3) != 2 || !parse_u32(v[0], 1, &u0) || !parse_u32(v[1], 0xFFFFFu, &a->companion_allow_mask)) return KOCHEN_ERR_RANGE;
            a->is_companion_capable = (uint8_t)u0;
        } else if (strcmp(k, "hydration") == 0) {
            if (split_delim(val, ',', v, 3) != 2 || !parse_f32(v[0], 0, 30, &a->min_hydration_ratio) || !parse_f32(v[1], a->min_hydration_ratio, 30, &a->max_hydration_ratio)) return KOCHEN_ERR_RANGE;
        } else if (strcmp(k, "dispersed") == 0) {
            if (split_delim(val, ',', v, 3) != 2 || !parse_f32(v[0], 0, 1, &a->min_dispersed_frac) || !parse_f32(v[1], a->min_dispersed_frac, 1, &a->max_dispersed_frac)) return KOCHEN_ERR_RANGE;
        } else if (strcmp(k, "surfactant") == 0) {
            if (!parse_f32(val, 0, 5, &a->min_surfactant_ratio)) return KOCHEN_ERR_RANGE;
        } else if (strcmp(k, "gel_prot") == 0) {
            if (split_delim(val, ',', v, 3) != 2 || !parse_f32(v[0], 0, 30, &a->min_gel_protein_pct) || !parse_f32(v[1], a->min_gel_protein_pct, 30, &a->max_gel_protein_pct)) return KOCHEN_ERR_RANGE;
        } else if (strcmp(k, "ph") == 0) {
            if (split_delim(val, ',', v, 3) != 2 || !parse_f32(v[0], 1.8f, 9.5f, &a->min_ph) || !parse_f32(v[1], a->min_ph, 9.5f, &a->max_ph)) return KOCHEN_ERR_RANGE;
        } else if (strcmp(k, "temp") == 0) {
            if (split_delim(val, ',', v, 3) != 2 || !parse_f32(v[0], 0, 260, &a->target_temp_c) || !parse_f32(v[1], a->target_temp_c, 280, &a->ceiling_temp_c)) return KOCHEN_ERR_RANGE;
        } else if (strcmp(k, "duration") == 0) {
            if (split_delim(val, ',', v, 3) != 2 || !parse_f32(v[0], 5, 86400, &a->cook_duration_sec) || !parse_f32(v[1], 0, 0.85f, &a->evap_loss_frac)) return KOCHEN_ERR_RANGE;
        } else if (strcmp(k, "guards") == 0) {
            if (!parse_u32(val, 0x3Fu, &a->guard_flags)) return KOCHEN_ERR_RANGE;
        } else if (strcmp(k, "sensory") == 0) {
            if (split_delim(val, ',', v, 7) != 6) return KOCHEN_ERR_PARSE;
            for (int i = 0; i < 6; i++) if (!parse_f32(v[i], 0, 1, &a->default_sensory[i])) return KOCHEN_ERR_RANGE;
        } else if (strcmp(k, "slot") == 0) {
            if (a->slot_count >= KOCHEN_MAX_ARCH_SLOTS || split_delim(val, ',', v, 7) != 6) return KOCHEN_ERR_PARSE;
            kochen_slot_spec_t *s = &a->slots[a->slot_count];
            if (!parse_u32(v[0], KOCHEN_ROLE_COUNT - 1, &u0) || !parse_u32(v[1], 1, &u1) ||
                !parse_f32(v[2], 0, 1, &s->min_frac) || !parse_f32(v[3], s->min_frac, 1, &s->max_frac) ||
                !parse_f32(v[4], s->min_frac, s->max_frac, &s->target_frac) || !parse_u32(v[5], 4, &u2)) return KOCHEN_ERR_RANGE;
            s->role = (kochen_role_t)u0; s->optional = (uint8_t)u1; s->stage = (kochen_stage_t)u2;
            a->slot_count++;
        } else return KOCHEN_ERR_PARSE;
    }
    for (uint8_t i = 0; i < db->archetype_count; i++) if (db->archetypes[i].slot_count == 0) return KOCHEN_ERR_PARSE;
    return KOCHEN_OK;
}

kochen_status_t kochen_parse_locale_tsv(kochen_db_t *db, kochen_lang_t lang, const char *text) {
    if ((unsigned)lang >= KOCHEN_LANG_COUNT) return KOCHEN_ERR_RANGE;
    char buf[512], *c[3];
    const char *cur = text;
    while (*cur) {
        const char *nl = strchr(cur, '\n');
        size_t len = nl ? (size_t)(nl - cur) : strlen(cur);
        if (len >= sizeof(buf)) return KOCHEN_ERR_PARSE;
        memcpy(buf, cur, len); buf[len] = '\0';
        cur = nl ? nl + 1 : cur + len;
        char *line = trim_line(buf);
        if (!line[0] || line[0] == '#') continue;
        if (split_delim(line, '\t', c, 3) != 2 || !c[0][0] || strlen(c[0]) >= sizeof(db->locales[0].key) || strlen(c[1]) >= KOCHEN_STEP_TEXT_LEN) return KOCHEN_ERR_PARSE;
        int idx = -1;
        for (uint16_t i = 0; i < db->locale_count; i++) if (strcmp(db->locales[i].key, c[0]) == 0) { idx = (int)i; break; }
        if (idx < 0) {
            if (db->locale_count >= KOCHEN_MAX_LOCALE_KEYS) return KOCHEN_ERR_CAPACITY;
            idx = db->locale_count++;
            strcpy(db->locales[idx].key, c[0]);
        }
        strcpy(db->locales[idx].text[lang], c[1]);
    }
    return KOCHEN_OK;
}

static char *slurp_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0 || sz > 4 * 1024 * 1024 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    return buf;
}

kochen_status_t kochen_db_load_dir(kochen_db_t *db, const char *assets_dir) {
    kochen_db_init(db);
    const char *files[6] = {
        "data/pantries.tsv", "data/ingredients.tsv", "data/archetypes.ini",
        "locales/en.tsv", "locales/de.tsv", "locales/pl.tsv"
    };
    char path[512];
    for (int i = 0; i < 6; i++) {
        snprintf(path, sizeof(path), "%s/%s", assets_dir, files[i]);
        char *txt = slurp_file(path);
        if (!txt) return KOCHEN_ERR_IO;
        kochen_status_t st = KOCHEN_OK;
        if (i == 0) st = kochen_parse_pantries_tsv(db, txt);
        else if (i == 1) st = kochen_parse_ingredients_tsv(db, txt);
        else if (i == 2) st = kochen_parse_archetypes_ini(db, txt);
        else st = kochen_parse_locale_tsv(db, (kochen_lang_t)(i - 3), txt);
        free(txt);
        if (st != KOCHEN_OK) return st;
    }
    return KOCHEN_OK;
}
