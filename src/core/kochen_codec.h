#ifndef KOCHEN_CODEC_H
#define KOCHEN_CODEC_H

#include "core/kochen_types.h"

kochen_status_t kochen_patch_encode(
    const kochen_patch_t *patch,
    char *out_b64,
    size_t out_cap
);

kochen_status_t kochen_patch_decode(
    const kochen_db_t *db,
    const char *input_str,
    kochen_patch_t *out_patch
);

#endif /* KOCHEN_CODEC_H */
