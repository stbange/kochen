#include "ui/kochen_gl.h"
#include <SDL3/SDL.h>
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __EMSCRIPTEN__
typedef GLuint (APIENTRY *pfn_glCreateShader)(GLenum);
typedef void   (APIENTRY *pfn_glShaderSource)(GLuint, GLsizei, const GLchar *const*, const GLint*);
typedef void   (APIENTRY *pfn_glCompileShader)(GLuint);
typedef void   (APIENTRY *pfn_glGetShaderiv)(GLuint, GLenum, GLint*);
typedef void   (APIENTRY *pfn_glDeleteShader)(GLuint);
typedef GLuint (APIENTRY *pfn_glCreateProgram)(void);
typedef void   (APIENTRY *pfn_glAttachShader)(GLuint, GLuint);
typedef void   (APIENTRY *pfn_glLinkProgram)(GLuint);
typedef void   (APIENTRY *pfn_glUseProgram)(GLuint);
typedef void   (APIENTRY *pfn_glDeleteProgram)(GLuint);
typedef GLint  (APIENTRY *pfn_glGetUniformLocation)(GLuint, const GLchar*);
typedef void   (APIENTRY *pfn_glUniform1i)(GLint, GLint);
typedef void   (APIENTRY *pfn_glUniform2f)(GLint, GLfloat, GLfloat);
typedef void   (APIENTRY *pfn_glGenVertexArrays)(GLsizei, GLuint*);
typedef void   (APIENTRY *pfn_glBindVertexArray)(GLuint);
typedef void   (APIENTRY *pfn_glDeleteVertexArrays)(GLsizei, const GLuint*);
typedef void   (APIENTRY *pfn_glGenBuffers)(GLsizei, GLuint*);
typedef void   (APIENTRY *pfn_glBindBuffer)(GLenum, GLuint);
typedef void   (APIENTRY *pfn_glBufferData)(GLenum, GLsizeiptr, const void*, GLenum);
typedef void   (APIENTRY *pfn_glDeleteBuffers)(GLsizei, const GLuint*);
typedef void   (APIENTRY *pfn_glEnableVertexAttribArray)(GLuint);
typedef void   (APIENTRY *pfn_glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef void   (APIENTRY *pfn_glActiveTexture_)(GLenum);

static struct {
    pfn_glCreateShader CreateShader; pfn_glShaderSource ShaderSource; pfn_glCompileShader CompileShader;
    pfn_glGetShaderiv GetShaderiv; pfn_glDeleteShader DeleteShader; pfn_glCreateProgram CreateProgram;
    pfn_glAttachShader AttachShader; pfn_glLinkProgram LinkProgram; pfn_glUseProgram UseProgram;
    pfn_glDeleteProgram DeleteProgram; pfn_glGetUniformLocation GetUniformLocation;
    pfn_glUniform1i Uniform1i; pfn_glUniform2f Uniform2f;
    pfn_glGenVertexArrays GenVertexArrays; pfn_glBindVertexArray BindVertexArray; pfn_glDeleteVertexArrays DeleteVertexArrays;
    pfn_glGenBuffers GenBuffers; pfn_glBindBuffer BindBuffer; pfn_glBufferData BufferData; pfn_glDeleteBuffers DeleteBuffers;
    pfn_glEnableVertexAttribArray EnableVertexAttribArray; pfn_glVertexAttribPointer VertexAttribPointer;
    pfn_glActiveTexture_ ActiveTexture;
} g_gl;

#define LOAD_GL(fn) g_gl.fn = (pfn_gl##fn)SDL_GL_GetProcAddress("gl" #fn)
static bool load_desktop_gl(void) {
    LOAD_GL(CreateShader); LOAD_GL(ShaderSource); LOAD_GL(CompileShader);
    LOAD_GL(GetShaderiv); LOAD_GL(DeleteShader); LOAD_GL(CreateProgram);
    LOAD_GL(AttachShader); LOAD_GL(LinkProgram); LOAD_GL(UseProgram);
    LOAD_GL(DeleteProgram); LOAD_GL(GetUniformLocation);
    LOAD_GL(Uniform1i); LOAD_GL(Uniform2f);
    LOAD_GL(GenVertexArrays); LOAD_GL(BindVertexArray); LOAD_GL(DeleteVertexArrays);
    LOAD_GL(GenBuffers); LOAD_GL(BindBuffer); LOAD_GL(BufferData); LOAD_GL(DeleteBuffers);
    LOAD_GL(EnableVertexAttribArray); LOAD_GL(VertexAttribPointer);
    g_gl.ActiveTexture = (pfn_glActiveTexture_)SDL_GL_GetProcAddress("glActiveTexture");
    return g_gl.CreateProgram && g_gl.GenVertexArrays && g_gl.GenBuffers;
}
#define GL_CALL(fn) g_gl.fn
#else
#define GL_CALL(fn) gl##fn
#endif

#define ATLAS_W 1024
#define ATLAS_H 1024
#define FIRST_CP 32
#define NUM_CPS  352 /* Covers ASCII + Latin-1 + Latin Extended-A (U+0020 .. U+017F) */
#define MAX_VERTS 32768

typedef struct {
    float x, y, u, v;
    float r, g, b, a;
    float mode;
} kochen_vert_t;

struct kochen_gl_renderer {
    GLuint prog, vao, vbo, font_tex;
    GLint loc_vp, loc_tex;
    stbtt_bakedchar glyphs[3][NUM_CPS];
    kochen_vert_t verts[MAX_VERTS];
    uint32_t vcount;
};

static char *read_asset(const char *dir, const char *rel, size_t *out_sz) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, rel);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    char *b = (char *)malloc((size_t)sz + 1);
    if (!b) { fclose(f); return NULL; }
    size_t rd = fread(b, 1, (size_t)sz, f);
    fclose(f);
    b[rd] = '\0';
    if (out_sz) *out_sz = rd;
    return b;
}

static GLuint compile_sh(GLenum type, const char *body) {
#ifdef __EMSCRIPTEN__
    const char *hdr = "#version 300 es\nprecision mediump float;\n";
#else
    const char *hdr = "#version 330 core\n";
#endif
    const char *srcs[2] = {hdr, body};
    GLuint s = GL_CALL(CreateShader)(type);
    GL_CALL(ShaderSource)(s, 2, srcs, NULL);
    GL_CALL(CompileShader)(s);
    GLint ok = 0;
    GL_CALL(GetShaderiv)(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { GL_CALL(DeleteShader)(s); return 0; }
    return s;
}

kochen_gl_renderer_t *kochen_gl_create(const char *assets_dir) {
#ifndef __EMSCRIPTEN__
    if (!load_desktop_gl()) return NULL;
#endif
    char *vs_src = read_asset(assets_dir, "shaders/synth.vert", NULL);
    char *fs_src = read_asset(assets_dir, "shaders/synth.frag", NULL);
    size_t ttf_sz = 0;
    unsigned char *ttf = (unsigned char *)read_asset(assets_dir, "fonts/DejaVuSans.ttf", &ttf_sz);
    if (!vs_src || !fs_src || !ttf) { free(vs_src); free(fs_src); free(ttf); return NULL; }

    kochen_gl_renderer_t *r = (kochen_gl_renderer_t *)calloc(1, sizeof(*r));
    GLuint vs = compile_sh(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile_sh(GL_FRAGMENT_SHADER, fs_src);
    free(vs_src); free(fs_src);
    if (!r || !vs || !fs) { free(ttf); free(r); return NULL; }

    r->prog = GL_CALL(CreateProgram)();
    GL_CALL(AttachShader)(r->prog, vs);
    GL_CALL(AttachShader)(r->prog, fs);
    GL_CALL(LinkProgram)(r->prog);
    GL_CALL(DeleteShader)(vs);
    GL_CALL(DeleteShader)(fs);
    r->loc_vp = GL_CALL(GetUniformLocation)(r->prog, "u_viewport");
    r->loc_tex = GL_CALL(GetUniformLocation)(r->prog, "u_font_tex");

    GL_CALL(GenVertexArrays)(1, &r->vao);
    GL_CALL(BindVertexArray)(r->vao);
    GL_CALL(GenBuffers)(1, &r->vbo);
    GL_CALL(BindBuffer)(GL_ARRAY_BUFFER, r->vbo);
    GLsizei stride = (GLsizei)sizeof(kochen_vert_t);
    GL_CALL(EnableVertexAttribArray)(0);
    GL_CALL(VertexAttribPointer)(0, 2, GL_FLOAT, GL_FALSE, stride, (const void *)0);
    GL_CALL(EnableVertexAttribArray)(1);
    GL_CALL(VertexAttribPointer)(1, 2, GL_FLOAT, GL_FALSE, stride, (const void *)(2 * sizeof(float)));
    GL_CALL(EnableVertexAttribArray)(2);
    GL_CALL(VertexAttribPointer)(2, 4, GL_FLOAT, GL_FALSE, stride, (const void *)(4 * sizeof(float)));
    GL_CALL(EnableVertexAttribArray)(3);
    GL_CALL(VertexAttribPointer)(3, 1, GL_FLOAT, GL_FALSE, stride, (const void *)(8 * sizeof(float)));

    unsigned char *atlas = (unsigned char *)calloc(ATLAS_W * ATLAS_H, 1);
    static const float SIZES[3] = {13.5f, 16.0f, 20.0f};
    for (int i = 0; i < 3; i++) {
        int band_h = ATLAS_H / 3;
        stbtt_BakeFontBitmap(ttf, 0, SIZES[i], atlas + (i * band_h * ATLAS_W),
                             ATLAS_W, band_h, FIRST_CP, NUM_CPS, r->glyphs[i]);
        for (int c = 0; c < NUM_CPS; c++) {
            r->glyphs[i][c].y0 = (uint16_t)(r->glyphs[i][c].y0 + i * band_h);
            r->glyphs[i][c].y1 = (uint16_t)(r->glyphs[i][c].y1 + i * band_h);
        }
    }
    free(ttf);

    glGenTextures(1, &r->font_tex);
    glBindTexture(GL_TEXTURE_2D, r->font_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ATLAS_W, ATLAS_H, 0, GL_RED, GL_UNSIGNED_BYTE, atlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    free(atlas);
    return r;
}

void kochen_gl_destroy(kochen_gl_renderer_t *r) {
    if (!r) return;
    if (r->font_tex) glDeleteTextures(1, &r->font_tex);
    if (r->vbo) GL_CALL(DeleteBuffers)(1, &r->vbo);
    if (r->vao) GL_CALL(DeleteVertexArrays)(1, &r->vao);
    if (r->prog) GL_CALL(DeleteProgram)(r->prog);
    free(r);
}

static void unpack_rgba(uint32_t c, float *r, float *g, float *b, float *a) {
    *r = (float)((c >> 24) & 0xFFu) / 255.0f;
    *g = (float)((c >> 16) & 0xFFu) / 255.0f;
    *b = (float)((c >> 8) & 0xFFu) / 255.0f;
    *a = (float)(c & 0xFFu) / 255.0f;
}

static void push_quad(
    kochen_gl_renderer_t *rn,
    float x0, float y0, float x1, float y1,
    float u0, float v0, float u1, float v1,
    uint32_t rgba, float mode
) {
    if (rn->vcount + 6 > MAX_VERTS) return;
    float r, g, b, a;
    unpack_rgba(rgba, &r, &g, &b, &a);
    kochen_vert_t *v = &rn->verts[rn->vcount];
    v[0] = (kochen_vert_t){x0, y0, u0, v0, r, g, b, a, mode};
    v[1] = (kochen_vert_t){x1, y0, u1, v0, r, g, b, a, mode};
    v[2] = (kochen_vert_t){x1, y1, u1, v1, r, g, b, a, mode};
    v[3] = (kochen_vert_t){x0, y0, u0, v0, r, g, b, a, mode};
    v[4] = (kochen_vert_t){x1, y1, u1, v1, r, g, b, a, mode};
    v[5] = (kochen_vert_t){x0, y1, u0, v1, r, g, b, a, mode};
    rn->vcount += 6;
}

static void push_line(kochen_gl_renderer_t *rn, float x0, float y0, float x1, float y1, float w, uint32_t rgba) {
    if (rn->vcount + 6 > MAX_VERTS) return;
    float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
    if (len < 0.01f) return;
    float nx = (-dy / len) * w * 0.5f, ny = (dx / len) * w * 0.5f;
    float r, g, b, a;
    unpack_rgba(rgba, &r, &g, &b, &a);
    kochen_vert_t *v = &rn->verts[rn->vcount];
    v[0] = (kochen_vert_t){x0 + nx, y0 + ny, 0, 0, r, g, b, a, 0.0f};
    v[1] = (kochen_vert_t){x1 + nx, y1 + ny, 0, 0, r, g, b, a, 0.0f};
    v[2] = (kochen_vert_t){x1 - nx, y1 - ny, 0, 0, r, g, b, a, 0.0f};
    v[3] = (kochen_vert_t){x0 + nx, y0 + ny, 0, 0, r, g, b, a, 0.0f};
    v[4] = (kochen_vert_t){x1 - nx, y1 - ny, 0, 0, r, g, b, a, 0.0f};
    v[5] = (kochen_vert_t){x0 - nx, y0 - ny, 0, 0, r, g, b, a, 0.0f};
    rn->vcount += 6;
}

static uint32_t next_utf8(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    if (!s[0]) return 0;
    if (s[0] < 0x80) { *p += 1; return s[0]; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        *p += 2; return ((uint32_t)(s[0] & 0x1F) << 6) | (uint32_t)(s[1] & 0x3F);
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *p += 3; return ((uint32_t)(s[0] & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (uint32_t)(s[2] & 0x3F);
    }
    *p += 1;
    return '?';
}

static void push_text(kochen_gl_renderer_t *rn, float x, float y, uint8_t sz, uint32_t rgba, const char *txt) {
    if (sz > 2) sz = 0;
    const char *p = txt;
    while (*p) {
        uint32_t cp = next_utf8(&p);
        if (cp < FIRST_CP || cp >= FIRST_CP + NUM_CPS) cp = '?';
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(rn->glyphs[sz], ATLAS_W, ATLAS_H, (int)(cp - FIRST_CP), &x, &y, &q, 1);
        push_quad(rn, q.x0, q.y0, q.x1, q.y1, q.s0, q.t0, q.s1, q.t1, rgba, 1.0f);
    }
}

void kochen_gl_render(kochen_gl_renderer_t *rn, float vp_w, float vp_h, int fb_w, int fb_h, const kochen_draw_list_t *dl) {
    if (!rn || !dl) return;
    rn->vcount = 0;

    for (uint16_t i = 0; i < dl->count; i++) {
        const kochen_draw_cmd_t *c = &dl->cmds[i];
        if (c->kind == KOCHEN_CMD_RECT) {
            push_quad(rn, c->x, c->y, c->x + c->w, c->y + c->h, -1.0f, -1.0f, 1.0f, 1.0f, c->rgba, 2.0f);
        } else if (c->kind == KOCHEN_CMD_LINE) {
            push_line(rn, c->x, c->y, c->w, c->h, c->value01, c->rgba);
        } else if (c->kind == KOCHEN_CMD_KNOB) {
            float r = c->w;
            push_quad(rn, c->x - r, c->y - r, c->x + r, c->y + r, -1.0f, -1.0f, 1.0f, 1.0f, 0x262F42FFu, 2.0f);
            float a0 = 2.356f, a_end = a0 + c->value01 * 4.712f;
            int segs = 16;
            for (int s = 0; s < segs; s++) {
                float t0 = a0 + (a_end - a0) * ((float)s / (float)segs);
                float t1 = a0 + (a_end - a0) * ((float)(s + 1) / (float)segs);
                push_line(rn, c->x + cosf(t0) * (r + 3.0f), c->y + sinf(t0) * (r + 3.0f),
                          c->x + cosf(t1) * (r + 3.0f), c->y + sinf(t1) * (r + 3.0f), 3.2f, c->rgba);
            }
            push_line(rn, c->x, c->y, c->x + cosf(a_end) * (r * 0.78f), c->y + sinf(a_end) * (r * 0.78f), 2.5f, 0xFFFFFFFFu);
            if (c->text[0]) push_text(rn, c->x - r - 12.0f, c->y + r + 15.0f, 0, 0xDCE3EEFFu, c->text);
        } else if (c->kind == KOCHEN_CMD_TEXT) {
            push_text(rn, c->x, c->y, c->font_size, c->rgba, c->text);
        }
    }

    glViewport(0, 0, (GLsizei)fb_w, (GLsizei)fb_h);
    glClearColor(0.067f, 0.078f, 0.106f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GL_CALL(UseProgram)(rn->prog);
    GL_CALL(Uniform2f)(rn->loc_vp, vp_w, vp_h);
    GL_CALL(ActiveTexture)(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, rn->font_tex);
    GL_CALL(Uniform1i)(rn->loc_tex, 0);

    GL_CALL(BindVertexArray)(rn->vao);
    GL_CALL(BindBuffer)(GL_ARRAY_BUFFER, rn->vbo);
    GL_CALL(BufferData)(GL_ARRAY_BUFFER, (GLsizeiptr)(rn->vcount * sizeof(kochen_vert_t)), rn->verts, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)rn->vcount);
}
