# Proposal: Kochen v2 — Modular Synthesizer Studio, Mobile Touch, Audio & CI/Pages

## Why
With the deterministic C11 culinary kernel proven across 60 global classics, 30 adversarial mutations, and 1,050 stress runs at 98% line coverage, Kochen needs its interactive cross-platform Modular Synthesizer Studio targeting Windows, Linux, macOS, and Emscripten Web (with automatic mobile touch switching and GitHub Pages deployment).

## What Changes
- Implement `src/ui/kochen_studio.h/.c`: decoupled immediate-mode rack state, layout & hit-testing, Desktop 3-column rack vs. Mobile Touch tabbed UI (`>= 48px` hitboxes), 7 rotary knobs, Typed Patch Bay with lock/reroll/ingredient picker, Bezier aroma-bridge cables, Live Patch Oscilloscope, `HOME` ↔ `PRO` switch + one-click `Re-solve for Home Pantry`, `EN`/`DE`/`PL` switcher, and `?patch=` + Markdown export actions.
- Implement `src/ui/kochen_audio.h/.c`: procedural C11 synth audio (knob detents, consonance/dissonance chords, Mute).
- Implement `src/ui/kochen_gl.h/.c`, `src/ui/main.c`, `assets/shaders/synth.{vert,frag}`, `assets/icons/oscillator_icons.svg`, and `web/shell.html`.
- Implement `tests/test_e2e_synth.c`, `.github/workflows/{ci-and-pages.yml,release.yml}`, and `README.md`.

## Out of Scope
- Career/puzzle wrappers, external UI toolkits, or multi-branch git workflows.
