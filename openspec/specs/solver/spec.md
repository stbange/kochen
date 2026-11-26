# Deterministic Solver & Dual Projection Spec Delta

## Purpose
Constrained slot assignment, continuous gram-ratio optimization, Primary + Companion archetype composition, Home/Pro projection identity, and `?patch=` codec.

## ADDED Requirements

### Requirement: Zero-Hardcoding Classic Reconstruction & Constrained Novel Synthesis
Given an Archetype (plus optional Companion Archetype), Regional Pantry, Dietary Mask, `Tradition ↔ Novelty` dial, Locked Slots, and 6-axis Sensory Equilibrium target, the solver SHALL deterministically assign functional slots and optimize gram masses so all structural ratio envelopes (`hydration`, `dispersed fat`, `surfactant coverage`, `protein gel window`, `safe pH`) hold simultaneously.

#### Scenario: Broad 60-classic reconstruction emerges without recipe tables
- **WHEN** the solver runs at `0%` Novelty across 60 global benchmark combinations spanning all 15 archetypes and 12 regional pantries
- **THEN** every solved dish matches expected functional ingredient families and satisfies all physical archetype envelopes

#### Scenario: Randomized 1,000-patch novelty sweep preserves physical invariants
- **WHEN** 1,000 patches are solved across `0%..100%` Novelty, dietary filters, slot locks, and Companion archetypes
- **THEN** 100% of feasible patches pass emulsion, hydration, protein coagulation, and pH checks deterministically

### Requirement: Home ↔ Pro DAG Identity & Explicit Home Pantry Re-Solve
Toggling between `HOME` and `PRO` modes SHALL project the exact same `kochen_dag_t` without altering ingredients or gram masses; if modernist additives are present when viewing `HOME` mode, the projection SHALL expose a flag and `kochen_resolve_home_pantry()` SHALL replace only modernist additive slots with Home Pantry functional equivalents.

#### Scenario: Toggling Pro to Home preserves dish and offers explicit Home re-solve
- **WHEN** a Pro patch containing sodium citrate or agar is projected in `HOME` mode and then passed to `kochen_resolve_home_pantry()`
- **THEN** toggling alone keeps identical ingredient IDs, weights, and DAG nodes while setting `has_modernist_additive = true`, and invoking `kochen_resolve_home_pantry()` replaces only modernist slots with valid Home Pantry stabilizers
