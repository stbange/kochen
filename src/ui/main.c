#include "core/kochen_codec.h"
#include "core/kochen_data.h"
#include "core/kochen_project.h"
#include "ui/kochen_audio.h"
#include "ui/kochen_gl.h"
#include "ui/kochen_studio.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    SDL_Window *win;
    SDL_GLContext gl_ctx;
    SDL_AudioStream *audio_stream;
    kochen_db_t db;
    kochen_audio_t audio;
    kochen_studio_t studio;
    kochen_gl_renderer_t *renderer;
    kochen_draw_list_t draw_list;
    uint64_t last_ticks;
    bool running;
} kochen_runtime_t;

static kochen_runtime_t g_app;

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE int kochen_web_load_patch(const char *code) {
    return (int)kochen_studio_load_patch_code(&g_app.studio, code);
}
EMSCRIPTEN_KEEPALIVE const char *kochen_web_get_patch(void) {
    kochen_patch_encode(&g_app.studio.patch, g_app.studio.last_patch_code, sizeof(g_app.studio.last_patch_code));
    return g_app.studio.last_patch_code;
}
EMSCRIPTEN_KEEPALIVE const char *kochen_web_get_markdown(void) {
    kochen_patch_encode(&g_app.studio.patch, g_app.studio.last_patch_code, sizeof(g_app.studio.last_patch_code));
    kochen_export_markdown(&g_app.db, &g_app.studio.patch, &g_app.studio.dag, &g_app.studio.proj,
                           g_app.studio.last_patch_code, g_app.studio.last_markdown, sizeof(g_app.studio.last_markdown));
    return g_app.studio.last_markdown;
}
EMSCRIPTEN_KEEPALIVE void kochen_web_set_touch_hint(int is_touch) {
    if (is_touch && !g_app.studio.touch_override_set)
        g_app.studio.touch_mode = true;
}
#endif

static void SDLCALL audio_stream_cb(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
    (void)total_amount;
    kochen_audio_t *au = (kochen_audio_t *)userdata;
    if (additional_amount <= 0) return;
    float buf[512];
    int frames_left = additional_amount / (int)sizeof(float);
    while (frames_left > 0) {
        int chunk = frames_left > 512 ? 512 : frames_left;
        kochen_audio_mix(au, buf, (size_t)chunk);
        SDL_PutAudioStreamData(stream, buf, chunk * (int)sizeof(float));
        frames_left -= chunk;
    }
}

static void handle_share_and_export(kochen_runtime_t *app) {
    if (app->studio.share_requested) {
        app->studio.share_requested = false;
        char share_str[320];
        snprintf(share_str, sizeof(share_str), "?patch=%s", app->studio.last_patch_code);
        SDL_SetClipboardText(share_str);
#ifdef __EMSCRIPTEN__
        EM_ASM({
            var code = UTF8ToString($0);
            var url = window.location.origin + window.location.pathname + "?patch=" + code;
            if (window.history && window.history.replaceState) window.history.replaceState(null, "", url);
            if (navigator.clipboard && navigator.clipboard.writeText) navigator.clipboard.writeText(url);
        }, app->studio.last_patch_code);
#endif
    }
    if (app->studio.export_md_requested) {
        app->studio.export_md_requested = false;
        SDL_SetClipboardText(app->studio.last_markdown);
#ifdef __EMSCRIPTEN__
        EM_ASM({
            var md = UTF8ToString($0);
            if (navigator.clipboard && navigator.clipboard.writeText) navigator.clipboard.writeText(md);
            if (window.kochenDownloadMarkdown) window.kochenDownloadMarkdown(md);
        }, app->studio.last_markdown);
#else
        FILE *f = fopen("kochen_recipe.md", "wb");
        if (f) {
            fwrite(app->studio.last_markdown, 1, strlen(app->studio.last_markdown), f);
            fclose(f);
        }
#endif
    }
}

static void frame_step(void *arg) {
    kochen_runtime_t *app = (kochen_runtime_t *)arg;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_EVENT_QUIT:
                app->running = false;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                int w = 1280, h = 800;
                SDL_GetWindowSize(app->win, &w, &h);
                kochen_studio_resize(&app->studio, (float)w, (float)h);
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (app->audio_stream) SDL_ResumeAudioStreamDevice(app->audio_stream);
                kochen_studio_pointer_down(&app->studio, ev.button.x, ev.button.y, false);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                kochen_studio_pointer_move(&app->studio, ev.motion.x, ev.motion.y);
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                kochen_studio_pointer_up(&app->studio);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                kochen_studio_scroll(&app->studio, ev.wheel.y);
                break;
            case SDL_EVENT_FINGER_DOWN:
                if (app->audio_stream) SDL_ResumeAudioStreamDevice(app->audio_stream);
                kochen_studio_pointer_down(&app->studio,
                                           ev.tfinger.x * app->studio.viewport_w,
                                           ev.tfinger.y * app->studio.viewport_h, true);
                break;
            case SDL_EVENT_FINGER_MOTION:
                kochen_studio_pointer_move(&app->studio,
                                           ev.tfinger.x * app->studio.viewport_w,
                                           ev.tfinger.y * app->studio.viewport_h);
                break;
            case SDL_EVENT_FINGER_UP:
                kochen_studio_pointer_up(&app->studio);
                break;
            default:
                break;
        }
    }

    uint64_t now = SDL_GetTicks();
    float dt = (app->last_ticks > 0) ? (float)(now - app->last_ticks) * 0.001f : 0.016f;
    app->last_ticks = now;

    kochen_studio_build_frame(&app->studio, dt, &app->draw_list);
    handle_share_and_export(app);

    int pw = (int)app->studio.viewport_w, ph = (int)app->studio.viewport_h;
    SDL_GetWindowSizeInPixels(app->win, &pw, &ph);
    kochen_gl_render(app->renderer, app->studio.viewport_w, app->studio.viewport_h, pw, ph, &app->draw_list);
    SDL_GL_SwapWindow(app->win);
}

static void resolve_assets_dir(char *out, size_t cap) {
    FILE *probe = fopen("assets/data/ingredients.tsv", "rb");
    if (probe) { fclose(probe); snprintf(out, cap, "assets"); return; }
    const char *base = SDL_GetBasePath();
    if (base) snprintf(out, cap, "%sassets", base);
    else snprintf(out, cap, "assets");
}

int main(int argc, char **argv) {
    memset(&g_app, 0, sizeof(g_app));
    char assets_dir[512];
    resolve_assets_dir(assets_dir, sizeof(assets_dir));

    if (kochen_db_load_dir(&g_app.db, assets_dir) != KOCHEN_OK) {
        fprintf(stderr, "Failed to load Kochen dataset from %s\n", assets_dir);
        return 1;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

#ifdef __EMSCRIPTEN__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    g_app.win = SDL_CreateWindow("Kochen — Culinary Synthesizer", 1280, 800,
                                 SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!g_app.win) return 1;
    g_app.gl_ctx = SDL_GL_CreateContext(g_app.win);
    if (!g_app.gl_ctx) return 1;

    kochen_audio_init(&g_app.audio, 44100);
    SDL_AudioSpec spec = {SDL_AUDIO_F32, 1, 44100};
    g_app.audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_stream_cb, &g_app.audio);

    int w = 1280, h = 800;
    SDL_GetWindowSize(g_app.win, &w, &h);
    kochen_studio_init(&g_app.studio, &g_app.db, &g_app.audio, (float)w, (float)h);
    if (argc > 1 && argv[1][0])
        kochen_studio_load_patch_code(&g_app.studio, argv[1]);

    g_app.renderer = kochen_gl_create(assets_dir);
    if (!g_app.renderer) {
        fprintf(stderr, "Failed to initialize OpenGL renderer\n");
        return 1;
    }

    g_app.running = true;
#ifdef __EMSCRIPTEN__
    EM_ASM({
        if (window.kochenOnReady) window.kochenOnReady();
    });
    emscripten_set_main_loop_arg(frame_step, &g_app, 0, 1);
#else
    while (g_app.running) {
        frame_step(&g_app);
        SDL_Delay(8);
    }
    if (g_app.audio_stream) SDL_DestroyAudioStream(g_app.audio_stream);
    kochen_gl_destroy(g_app.renderer);
    SDL_GL_DestroyContext(g_app.gl_ctx);
    SDL_DestroyWindow(g_app.win);
    SDL_Quit();
#endif
    return 0;
}
