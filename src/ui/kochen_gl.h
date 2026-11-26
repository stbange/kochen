#ifndef KOCHEN_GL_H
#define KOCHEN_GL_H

#include "ui/kochen_studio.h"

typedef struct kochen_gl_renderer kochen_gl_renderer_t;

kochen_gl_renderer_t *kochen_gl_create(const char *assets_dir);
void kochen_gl_destroy(kochen_gl_renderer_t *r);
void kochen_gl_render(kochen_gl_renderer_t *r, float vp_w, float vp_h, int fb_w, int fb_h, const kochen_draw_list_t *dl);

#endif /* KOCHEN_GL_H */
