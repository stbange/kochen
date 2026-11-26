# Ontology & External Loader Spec Delta

## Purpose
Measurable physical, chemical, sensory, and volatile ingredient ontology plus strict external TSV/INI loading and `EN`/`DE`/`PL` localization.

## ADDED Requirements

### Requirement: Vectorized Functional Ingredient Ontology & Boundary Validation
The kernel SHALL load ~150 ingredients across 12 regional pantries and a Modernist Pro pantry from `assets/data/ingredients.tsv`, storing composition (`water`, `fat`, `protein` + class, `starch` + amylose fraction, `sugar` + reducing fraction, `fiber`), colloid/chemical parameters (`pH`, buffer capacity, `NaCl`, free glutamate, 5'-nucleotides, surfactant power & `HLB`, hydrocolloid power & set temp, active enzymes & kill temp), aroma volatility/solubility bitmasks, and Home unit conversions, while rejecting any malformed, non-finite, or out-of-range records.

#### Scenario: Valid dataset loads completely with localized strings
- **WHEN** `kochen_db_load()` reads `assets/data/` and `assets/locales/{en,de,pl}.tsv`
- **THEN** at least 145 ingredients, 12 regional pantries, and 15 structural archetypes load with zero errors and every slug resolves to non-empty UTF-8 display strings in `EN`, `DE`, and `PL`

#### Scenario: Malformed or out-of-bounds TSV/INI input is rejected safely
- **WHEN** a TSV or INI buffer contains `NaN`, negative mass percentages, `pH < 1.5` or `pH > 10.0`, truncated columns, or duplicate slugs
- **THEN** the parser returns a non-zero error status without out-of-bounds memory access or undefined behavior
