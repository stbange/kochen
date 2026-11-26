# Tasks: Kochen v2 — Modular Synthesizer Studio & CI/Pages

- [x] 1. Stage `stb_truetype.h`, CC BY 4.0 `oscillator_icons.svg`, and `assets/shaders/synth.{vert,frag}`.
- [x] 2. Implement `src/ui/kochen_audio.h/.c` and `src/ui/kochen_studio.h/.c` (headless-testable rack/touch UI & draw list).
- [x] 3. Implement `src/ui/kochen_gl.h/.c`, `src/ui/main.c`, and `web/shell.html`.
- [x] 4. Implement `tests/test_e2e_synth.c` and verify `ctest` + `gcovr = 98%` + native build + Emscripten Web build.
- [x] 5. Add `.github/workflows/{ci-and-pages.yml,release.yml}`, `README.md`, archive OpenSpec change, push `main`, and verify GitHub Pages.
