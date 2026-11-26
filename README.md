# Kochen — First-Principles Culinary Synthesizer

**[Launch Kochen on GitHub Pages](https://stbange.github.io/kochen/)**

**Kochen** decomposes cooking and pastry into **food physics, chemical kinetics, colloid stability, and aroma chemistry**, compiling dishes as **deterministic state-transformation graphs (DAGs)** rather than static recipe prose. Built in portable **C11 + SDL3 + OpenGL 3.3 Core / WebGL 2.0 (Emscripten)** with human-editable external TSV/INI datasets.

---

## Highlights

- **Modular Culinary Synthesizer**:
  - **Primary + Companion Oscillators**: Choose from **15 structural archetypes** (*Starch–Fat Warm Emulsion*, *Cold/Warm Shear Emulsion*, *Collagen Braise*, *Maillard + Pan Glaze*, *Starch Hydration*, *Velouté/Suspension*, *Set Protein Gel/Custard*, *Coagulated Curd*, *Gluten Dough*, *Low-Gluten Batter*, *Aerated Foam*, *Sucrose Caramel*, *Acid/Osmotic Cure*, *Dehydrated Crust/Moist Core*, *Fat-Tempered Legume*) and optionally chain a companion sauce/emulsion.
  - **Typed Functional Patch Bay**: Lock specific anchor ingredients, reroll single slots, or open the ingredient picker across **166 scientifically parameterized ingredients** and **12 global regional pantries** + **Modernist Pro Pantry**.
  - **Macro Modulation Knobs & Live Oscilloscope**: Modulate `Tradition <-> Novelty` (`0%` strict regional authenticity to `100%` volatile compound bridging) and 6 **Sensory Equilibrium** axes (*Richness*, *Brightness*, *Savory/Umami*, *Heat*, *Sweet–Bitter*, *Crunch*) with sub-millisecond gram rebalancing and live telemetry (`pH`, `NaCl %`, Yamaguchi umami synergy, emulsion packing/surfactant coverage, and active guards).
- **Live `Home Mode` <-> `Pro Mode` Switch**:
  - Both modes project the **exact same underlying Recipe DAG**—toggling never alters the dish.
  - **Home Mode**: Rounded grams + intuitive household spoons/cups/pieces, stovetop heat levels, physical sensory cues, and a one-click **`Re-solve for Home Pantry`** action whenever modernist additives are present.
  - **Pro Mode**: Formula percentages (`0.1 g` / `%`), exact thermal targets (`°C`), `pH`, `HLB`, volatile bridges, and modernist hydrocolloids/chelators/enzymes (*sodium citrate*, *agar-agar*, *xanthan*, *iota carrageenan*, *methylcellulose*, *soy lecithin*, *transglutaminase*, *shio-koji*).
- **Automatic Thermodynamic Precondition Guards**:
  - `MaillardDry` (desiccates wet surfaces prior to `>= 165 °C` sear), `EnzymeScald` (denatures active bromelain/zingibain/koji proteases at `>= 76 °C` before protein gelation), `PectinDelay` (softens legumes at `pH >= 6.0` before acidic additions), `Casein<68C` (protects unchelated aged cheese emulsions from stringing), `LipidDelay` (develops gluten before high-fat shortening), and `CleanFoam` (isolates albumen/aquafaba aeration from lipids).
- **Cross-Platform, Multilingual & Shareable**:
  - Native **Windows**, **Linux**, **macOS**, and **Web (Emscripten)** with automatic **Mobile Touch UI** switching on phones/tablets (`>= 48px` targets, tabbed views, vertical-drag rotary knobs).
  - Live **`EN` / `DE` / `PL`** localization, procedural C11 synthesizer audio with `Mute` toggle, shareable `?patch=...` URLs, and Markdown Recipe Card export.

---

## Controls

| Action | Desktop (Mouse) | Mobile / Touch |
|---|---|---|
| Switch Primary / Companion / Pantry | Click top transport selectors | Tap top transport selectors |
| Modulate Knobs (`Novelty` & 6 Sensory Axes) | Click and drag vertically on rotary knob | Drag vertically on knob (`Knobs & Scope` tab) |
| Lock / Mutate Single Slot | Click `LOCK` or `MUT` on slot row | Tap `LOCK` or `MUT` (`Patch Bay` tab) |
| Pick Specific Ingredient | Click ingredient slot name | Tap ingredient slot name |
| Toggle `Home` <-> `Pro` / `EN`·`DE`·`PL` | Click `Home Mode` / `EN` button | Tap `Home Mode` / `EN` button |
| Navigate Panels on Narrow Viewports | All 3 racks visible side-by-side | Tap bottom tabs (`Patch Bay` · `Modulation & Scope` · `Recipe DAG`) |
| Share URL or Export Recipe Card | Click `SHARE` (`?patch=...`) or `MD` | Tap `SHARE` or `MD` |

---

## Building & Verification

### Unit, Broad-Sample & Automated Studio Tests (`>= 95%` Coverage)
```bash
cmake -G Ninja -B build-test -DCMAKE_BUILD_TYPE=Debug \
  -DKOCHEN_BUILD_APP=OFF -DKOCHEN_COVERAGE=ON -DKOCHEN_SANITIZERS=ON
cmake --build build-test --parallel
ctest --test-dir build-test --output-on-failure
gcovr --root src --filter 'src/(core|ui/kochen_studio|ui/kochen_audio)' --fail-under-line 95 build-test
```

### Native Desktop (Linux / macOS / Windows)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DKOCHEN_BUILD_TESTS=OFF
cmake --build build --config Release --parallel
./build/kochen
```

### Web (Emscripten)
```bash
emcmake cmake -G Ninja -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --parallel
python3 -m http.server 8000 --directory build-web/dist
```

---

## Extending Kochen

All datasets and translations live as plain human-editable files in `assets/` beside the executable (and beside `kochen.wasm` on the web):
- `assets/data/ingredients.tsv` — 34-column physical, chemical, colloid, enzyme, and volatile ontology
- `assets/data/archetypes.ini` — structural slot envelopes, companion rules, and guard flags
- `assets/data/pantries.tsv` — regional pantry definitions
- `assets/locales/{en,de,pl}.tsv` — English, German, and Polish UI, ingredient, and cue translations

## Development Process

- **OpenSpec**: Living specifications and archived change records live in [`openspec/`](openspec/).
- **Ponytail**: Engineered following [Ponytail](https://github.com/dietrichgebert/ponytail) (`AGENTS.md`) — smallest complete code, zero unnecessary dependencies, strict validation at trust boundaries.

## Licenses

- **Code, Schemas & Build Scripts**: [MIT License](LICENSE)
- **Original Visual Art, Icons & Procedural Sound Design**: [CC BY 4.0](LICENSE-ART)
