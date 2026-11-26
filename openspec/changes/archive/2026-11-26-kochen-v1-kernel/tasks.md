# Tasks: Kochen v1 — Deterministic Culinary Kernel

- [x] 1. Implement `src/core/kochen_types.h` and `src/core/kochen_data.h/.c` (strict TSV/INI parser).
- [x] 2. Author `assets/data/{pantries.tsv,archetypes.ini,ingredients.tsv}` (15 archetypes, 166 ingredients across 12 pantries + Modernist lab) and `assets/locales/{en,de,pl}.tsv` (306 keys x 3 languages).
- [x] 3. Implement `src/core/kochen_physics.h/.c` and `src/core/kochen_solver.h/.c` (including Primary+Companion solving and `kochen_resolve_home_pantry`).
- [x] 4. Implement `src/core/kochen_dag.h/.c`, `src/core/kochen_project.h/.c`, and `src/core/kochen_codec.h/.c`.
- [x] 5. Build and pass `test_data_codec`, `test_physics_dag`, `test_projections_i18n`, and `test_broad_sample` (60 classics + 30 mutations + 1,050 stress runs) under `ASan + UBSan` with `gcovr = 98%` line coverage.
