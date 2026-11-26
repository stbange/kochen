# Design: Deterministic C11 Culinary Kernel

## Data Representation
- Fixed-capacity value structs (`kochen_db_t`, `kochen_patch_t`, `kochen_solved_t`, `kochen_dag_t`) avoid dynamic heap churn and run identically on x86_64, ARM64, and WASM32.
- `kochen_data.c` parses `ingredients.tsv`, `pantries.tsv`, `archetypes.ini`, and `locales/*.tsv` in `< 150` lines with strict `strtof`/`strtoul` validation, rejecting `NaN`/`Inf`, out-of-range percentages, invalid `pH`, and unknown slugs.

## Physics & Solver Pipeline
1. **Candidate Assignment**: Filters ingredients by `role_mask`, `diet_mask`, and active mode (`DIET_MODERNIST_ADDITIVE` gated to `PRO` during fresh solve), scoring candidates with convex combination $(1 - \alpha)\,P_{\text{pantry}} + \alpha\,B_{\text{volatile}} + \beta\,F_{\text{role}}$ and deterministic `PCG32` tie-breaks.
2. **Projected Coordinate Descent**: Optimizes slot masses $\mathbf{w}$ (`<= 80` passes) minimizing 6-axis Sensory Equilibrium error plus quadratic penalties on archetype hydration, emulsion dispersed-phase packing, surfactant coverage, coagulable protein concentration, ionic floor, and buffer-weighted mixture `pH`.
3. **Thermodynamic DAG & Precondition Guards**: Compiles Primary + optional Companion archetypes into a topologically ordered `kochen_dag_t`, inserting `OP_PAT_DRY_BRINE` (`MaillardDryGuard`), `OP_PRE_SCALD_ENZYME` (`EnzymeGuard`), post-softening acid merge (`PectinSofteningGuard`), `< 68°C` unchelated casein ceiling (`CaseinThermalCeilingGuard`), and early-fat vs. `< 55°C` off-heat `VolatileScheduling`.
4. **Dual Projection & Codec**: Projects one `kochen_dag_t` into `Home` or `Pro` views in `EN`/`DE`/`PL`, flags modernist slots in Home view for explicit one-click `kochen_resolve_home_pantry()`, and encodes patches via `CRC-16/CCITT` + Base64url.
