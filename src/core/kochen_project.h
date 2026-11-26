#ifndef KOCHEN_PROJECT_H
#define KOCHEN_PROJECT_H

#include "core/kochen_types.h"

kochen_status_t kochen_project_dag(
    const kochen_db_t *db,
    const kochen_patch_t *patch,
    const kochen_dag_t *dag,
    kochen_mode_t view_mode,
    kochen_lang_t lang,
    kochen_projection_t *out_proj
);

size_t kochen_export_markdown(
    const kochen_db_t *db,
    const kochen_patch_t *patch,
    const kochen_dag_t *dag,
    const kochen_projection_t *proj,
    const char *patch_code,
    char *out_buf,
    size_t buf_cap
);

#endif /* KOCHEN_PROJECT_H */
