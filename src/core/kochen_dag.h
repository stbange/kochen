#ifndef KOCHEN_DAG_H
#define KOCHEN_DAG_H

#include "core/kochen_types.h"

kochen_status_t kochen_dag_compile(
    const kochen_db_t *db,
    const kochen_patch_t *patch,
    kochen_dag_t *out_dag
);

#endif /* KOCHEN_DAG_H */
