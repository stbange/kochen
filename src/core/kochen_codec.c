#include "core/kochen_codec.h"
#include <math.h>
#include <string.h>

static const char B64URL[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static uint16_t crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

static uint8_t clamp_u8(float v01) {
    float c = fminf(1.0f, fmaxf(0.0f, v01));
    return (uint8_t)roundf(c * 255.0f);
}

kochen_status_t kochen_patch_encode(
    const kochen_patch_t *patch,
    char *out_b64,
    size_t out_cap
) {
    if (!patch || !out_b64 || patch->slot_count > KOCHEN_MAX_PATCH_SLOTS) return KOCHEN_ERR_RANGE;

    uint8_t raw[24 + KOCHEN_MAX_PATCH_SLOTS * 6];
    size_t p = 0;
    raw[p++] = 0x01u;
    raw[p++] = patch->primary_arch_id;
    raw[p++] = (uint8_t)patch->companion_arch_id;
    raw[p++] = patch->pantry_id;
    raw[p++] = (uint8_t)patch->solve_mode;
    raw[p++] = patch->slot_count;
    raw[p++] = (uint8_t)(patch->forbidden_diet_mask & 0xFFu);
    raw[p++] = (uint8_t)((patch->forbidden_diet_mask >> 8) & 0xFFu);
    raw[p++] = clamp_u8(patch->novelty);
    for (int a = 0; a < KOCHEN_SENS_COUNT; a++)
        raw[p++] = clamp_u8(patch->target_sensory[a]);

    uint16_t batch_u16 = (uint16_t)fminf(65535.0f, fmaxf(10.0f, roundf(patch->batch_mass_g)));
    raw[p++] = (uint8_t)(batch_u16 & 0xFFu);
    raw[p++] = (uint8_t)(batch_u16 >> 8);
    raw[p++] = (uint8_t)(patch->seed & 0xFFu);
    raw[p++] = (uint8_t)((patch->seed >> 8) & 0xFFu);
    raw[p++] = (uint8_t)((patch->seed >> 16) & 0xFFu);
    raw[p++] = (uint8_t)((patch->seed >> 24) & 0xFFu);

    for (uint8_t i = 0; i < patch->slot_count; i++) {
        const kochen_patch_slot_t *s = &patch->slots[i];
        raw[p++] = (uint8_t)((s->component_idx & 1u) | ((s->locked & 1u) << 1) | ((s->arch_slot_idx & 0x0Fu) << 2));
        raw[p++] = (uint8_t)s->role;
        uint16_t id_u16 = (uint16_t)s->ingredient_id;
        raw[p++] = (uint8_t)(id_u16 & 0xFFu);
        raw[p++] = (uint8_t)(id_u16 >> 8);
        uint16_t dg = (uint16_t)fminf(65535.0f, fmaxf(0.0f, roundf(s->mass_g * 10.0f)));
        raw[p++] = (uint8_t)(dg & 0xFFu);
        raw[p++] = (uint8_t)(dg >> 8);
    }

    uint16_t crc = crc16_ccitt(raw, p);
    raw[p++] = (uint8_t)(crc & 0xFFu);
    raw[p++] = (uint8_t)(crc >> 8);

    size_t needed = ((p * 4) + 2) / 3 + 1;
    if (out_cap < needed) return KOCHEN_ERR_CAPACITY;

    size_t o = 0;
    for (size_t i = 0; i < p; i += 3) {
        uint32_t v = (uint32_t)raw[i] << 16;
        if (i + 1 < p) v |= (uint32_t)raw[i + 1] << 8;
        if (i + 2 < p) v |= (uint32_t)raw[i + 2];
        out_b64[o++] = B64URL[(v >> 18) & 0x3Fu];
        out_b64[o++] = B64URL[(v >> 12) & 0x3Fu];
        if (i + 1 < p) out_b64[o++] = B64URL[(v >> 6) & 0x3Fu];
        if (i + 2 < p) out_b64[o++] = B64URL[v & 0x3Fu];
    }
    out_b64[o] = '\0';
    return KOCHEN_OK;
}

static int b64url_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;
    return -1;
}

kochen_status_t kochen_patch_decode(
    const kochen_db_t *db,
    const char *input_str,
    kochen_patch_t *out_patch
) {
    if (!db || !input_str || !out_patch) return KOCHEN_ERR_RANGE;

    const char *s = strstr(input_str, "patch=");
    s = s ? (s + 6) : input_str;
    size_t slen = 0;
    while (s[slen] && s[slen] != '&' && s[slen] != '#' && s[slen] != ' ' && s[slen] != '=') slen++;
    if (slen < 28 || slen > 240 || (slen % 4) == 1) return KOCHEN_ERR_PARSE;

    uint8_t raw[180];
    size_t p = 0;
    for (size_t i = 0; i < slen; i += 4) {
        int v0 = b64url_val(s[i]), v1 = b64url_val(s[i + 1]);
        int v2 = (i + 2 < slen) ? b64url_val(s[i + 2]) : 0;
        int v3 = (i + 3 < slen) ? b64url_val(s[i + 3]) : 0;
        if (v0 < 0 || v1 < 0 || v2 < 0 || v3 < 0 || p + 3 > sizeof(raw)) return KOCHEN_ERR_PARSE;
        uint32_t acc = ((uint32_t)v0 << 18) | ((uint32_t)v1 << 12) | ((uint32_t)v2 << 6) | (uint32_t)v3;
        raw[p++] = (uint8_t)((acc >> 16) & 0xFFu);
        if (i + 2 < slen) raw[p++] = (uint8_t)((acc >> 8) & 0xFFu);
        if (i + 3 < slen) raw[p++] = (uint8_t)(acc & 0xFFu);
    }

    if (p < 23 || raw[0] != 0x01u) return KOCHEN_ERR_PARSE;
    uint8_t n_slots = raw[5];
    size_t expected = 21 + (size_t)n_slots * 6 + 2;
    if (n_slots > KOCHEN_MAX_PATCH_SLOTS || p != expected) return KOCHEN_ERR_PARSE;

    uint16_t got_crc = (uint16_t)raw[p - 2] | ((uint16_t)raw[p - 1] << 8);
    if (crc16_ccitt(raw, p - 2) != got_crc) return KOCHEN_ERR_CHECKSUM;

    kochen_patch_t tmp = {0};
    tmp.primary_arch_id = raw[1];
    tmp.companion_arch_id = (int8_t)raw[2];
    tmp.pantry_id = raw[3];
    if (raw[4] > 1 || tmp.primary_arch_id >= db->archetype_count ||
        tmp.companion_arch_id < -1 || tmp.companion_arch_id >= (int8_t)db->archetype_count ||
        tmp.pantry_id >= db->pantry_count)
        return KOCHEN_ERR_RANGE;

    tmp.solve_mode = (kochen_mode_t)raw[4];
    tmp.slot_count = n_slots;
    tmp.forbidden_diet_mask = ((uint32_t)raw[6] | ((uint32_t)raw[7] << 8)) & 0x3FFu;
    tmp.novelty = (float)raw[8] / 255.0f;
    for (int a = 0; a < KOCHEN_SENS_COUNT; a++)
        tmp.target_sensory[a] = (float)raw[9 + a] / 255.0f;
    tmp.batch_mass_g = (float)((uint16_t)raw[15] | ((uint16_t)raw[16] << 8));
    tmp.seed = (uint32_t)raw[17] | ((uint32_t)raw[18] << 8) | ((uint32_t)raw[19] << 16) | ((uint32_t)raw[20] << 24);
    if (tmp.batch_mass_g < 10.0f) return KOCHEN_ERR_RANGE;

    size_t off = 21;
    for (uint8_t i = 0; i < n_slots; i++) {
        kochen_patch_slot_t *sl = &tmp.slots[i];
        uint8_t fl = raw[off];
        sl->component_idx = fl & 1u;
        sl->locked = (fl >> 1) & 1u;
        sl->arch_slot_idx = (fl >> 2) & 0x0Fu;
        sl->role = (kochen_role_t)raw[off + 1];
        sl->ingredient_id = (int16_t)((uint16_t)raw[off + 2] | ((uint16_t)raw[off + 3] << 8));
        sl->mass_g = (float)((uint16_t)raw[off + 4] | ((uint16_t)raw[off + 5] << 8)) * 0.1f;
        off += 6;

        const kochen_archetype_t *arch = (sl->component_idx == 0)
            ? &db->archetypes[tmp.primary_arch_id]
            : (tmp.companion_arch_id >= 0 ? &db->archetypes[tmp.companion_arch_id] : NULL);
        if (!arch || sl->arch_slot_idx >= arch->slot_count || (unsigned)sl->role >= KOCHEN_ROLE_COUNT ||
            sl->ingredient_id < -1 || sl->ingredient_id >= (int16_t)db->ingredient_count)
            return KOCHEN_ERR_RANGE;
    }

    *out_patch = tmp;
    return KOCHEN_OK;
}
