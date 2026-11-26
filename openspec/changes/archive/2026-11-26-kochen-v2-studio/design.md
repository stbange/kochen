# Design: Modular Synthesizer Studio & Automated E2E Harness

## Headless-First UI & Audio Architecture
- `kochen_studio.c` computes exact normalized/pixel widget rectangles, emits a flat `kochen_draw_list_t` of vector primitives (`PANEL`, `KNOB_ARC`, `BEZIER_CABLE`, `LINE`, `TEXT`), and processes pointer/touch/key events without calling SDL or OpenGL directly.
- `kochen_audio.c` synthesizes `44.1 kHz` float32 PCM buffers directly from lock-free event triggers (`detent_click`, `chord_ping`, `hazard_dyad`, `muted`).
- `kochen_gl.c` and `main.c` stay ultra-thin: upload `stb_truetype` UTF-8 atlas (`ASCII + Latin-1 + Latin Extended-A` for `EN`/`DE`/`PL`), flush `kochen_draw_list_t` through one GL 3.3 / WebGL 2.0 shader pair, pump SDL3 mouse/touch/keyboard/audio, and bridge `?patch=` / clipboard on Emscripten.
