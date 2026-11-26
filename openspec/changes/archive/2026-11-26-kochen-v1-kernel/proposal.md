# Proposal: Kochen v1 — Deterministic Culinary Kernel & Broad-Sample Validation

## Why
Before building interactive UI or rendering layers, Kochen must prove that a single deterministic physics-and-chemistry solver in portable C11 can reconstruct diverse global classics without hardcoding and synthesize physically sound novel dishes across 15 structural archetypes.

## What Changes
- Implement `src/core/` in dependency-free C11 (`kochen_types.h`, `kochen_data`, `kochen_physics`, `kochen_solver`, `kochen_dag`, `kochen_project`, `kochen_codec`).
- Curate `assets/data/{pantries.tsv,ingredients.tsv,archetypes.ini}` (~150 ingredients across 12 regional pantries + Modernist lab; 15 structural archetypes + Primary/Companion chaining) and `assets/locales/{en,de,pl}.tsv`.
- Enforce Home ↔ Pro DAG structural identity, explicit `kochen_resolve_home_pantry()` swap for modernist additives, CRC16+Base64url `?patch=` codec, and Markdown Recipe Card export.
- Build comprehensive unit and broad-sample test suites (`60` classics, `30` mutations, `1,000` stress runs) achieving `>= 95%` line coverage under `ASan + UBSan`.

## Out of Scope
- SDL3/OpenGL graphical studio, procedural audio stream, and Emscripten HTML shell (deferred to `kochen-v2-studio` once Gate 1 passes).
- LLM text generation, career/puzzle-mission wrappers, or scripting VMs.
