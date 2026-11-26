# Thermodynamic DAG & Physical Guards Spec Delta

## Purpose
Compile solved ingredient patches into topologically valid thermodynamic DAGs enforcing food-physics preconditions automatically.

## ADDED Requirements

### Requirement: Automatic Physical Precondition Guards
The DAG compiler SHALL inspect ingredient vectors and archetype flags to automatically insert or reorder preparation and thermal nodes for surface-dry Maillard browning, protease enzyme thermal deactivation, neutral-pH legume pectin softening prior to acid addition, unchelated casein `< 68°C` emulsification ceilings, and early-lipid vs. `< 55°C` off-heat volatile scheduling.

#### Scenario: Active protease in a custard or gelatin matrix triggers pre-scald guard
- **WHEN** an ingredient with `ENZ_PROTEASE` (e.g., raw pineapple, ginger, or koji) is locked into a Set Protein Gel or Collagen Braise
- **THEN** the compiled DAG inserts an `OP_PRE_SCALD_ENZYME` node at `>= enzyme_kill_c` strictly upstream of the protein coagulation or gelatin merge node

#### Scenario: Acidic additions in legume matrices are delayed past pectin softening
- **WHEN** an ingredient with `pH < 5.0` (e.g., tomato, tamarind, lemon) is solved into a Fat-Tempered Legume Matrix
- **THEN** the legume hydration/simmer node executes at `pH >= 6.0` and the acidic ingredient enters only at a downstream merge/finish node
