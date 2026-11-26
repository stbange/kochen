# Automated E2E Testing, Coverage & CI/Pages Spec Delta

## Purpose
End-to-end automated studio testing, `>= 95%` line coverage enforcement, multi-OS CI builds, and `main`-branch GitHub Pages deployment.

## ADDED Requirements

### Requirement: Automated Studio Journey Test & >=95% Line Coverage
`ctest` SHALL execute unit, broad-sample, and automated end-to-end studio journey tests under `ASan + UBSan`, and `gcovr` SHALL enforce `>= 95%` line coverage across `src/core/`, `src/ui/kochen_studio.c`, and `src/ui/kochen_audio.c`.

#### Scenario: Automated studio journey exercises Desktop, Touch, Pro->Home re-solve, i18n, audio, and ?patch= round-trip
- **WHEN** `test_e2e_synth` runs in headless test mode
- **THEN** all 5 interactive user journeys complete with zero assertion or sanitizer errors and `gcovr` reports `>= 95%` line coverage
