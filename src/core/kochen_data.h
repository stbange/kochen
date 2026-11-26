#ifndef KOCHEN_DATA_H
#define KOCHEN_DATA_H

#include "core/kochen_types.h"

void kochen_db_init(kochen_db_t *db);

kochen_status_t kochen_parse_pantries_tsv(kochen_db_t *db, const char *text);
kochen_status_t kochen_parse_ingredients_tsv(kochen_db_t *db, const char *text);
kochen_status_t kochen_parse_archetypes_ini(kochen_db_t *db, const char *text);
kochen_status_t kochen_parse_locale_tsv(kochen_db_t *db, kochen_lang_t lang, const char *text);

kochen_status_t kochen_db_load_dir(kochen_db_t *db, const char *assets_dir);

int kochen_find_ingredient(const kochen_db_t *db, const char *slug);
int kochen_find_archetype(const kochen_db_t *db, const char *slug);
int kochen_find_pantry(const kochen_db_t *db, const char *slug);
const char *kochen_tr(const kochen_db_t *db, kochen_lang_t lang, const char *key);

#endif /* KOCHEN_DATA_H */
