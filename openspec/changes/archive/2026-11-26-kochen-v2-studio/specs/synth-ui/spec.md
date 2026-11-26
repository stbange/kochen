# Synthesizer UI, Mobile Touch & Procedural Audio Spec Delta

## Purpose
Cross-platform modular culinary synthesizer UI, automatic mobile touch adaptation, procedural synth audio, and URL/Markdown sharing.

## ADDED Requirements

### Requirement: Modular Rack Workbench & Adaptive Mobile Touch Layout
The studio SHALL render a 3-column synthesizer rack (`Patch Bay`, `Modulation & Oscilloscope`, `Recipe DAG`) on desktop viewports and automatically switch to a tabbed Touch UI (`>= 48px` targets, vertical-drag rotary knobs) upon touch input or viewports `< 960px` wide, while supporting manual Desktop/Touch switching and `EN`/`DE`/`PL` localization.

#### Scenario: Touch event on mobile viewport switches studio to Touch UI
- **WHEN** the studio receives a touch down event or resizes to `390x844`
- **THEN** `touch_mode` activates automatically, bottom navigation tabs switch between `Patch Bay`, `Modulation & Scope`, and `Recipe DAG`, and interactive controls expose `>= 48px` hit targets

### Requirement: Procedural Synthesizer Audio Feedback with Mute
The audio engine SHALL synthesize bounded `[-1.0, 1.0]` PCM detents on knob changes, harmonic intervals on balanced synthesis, and dissonant dyads on physical warnings, outputting exact `0.0` silence when muted.

#### Scenario: Muted audio engine emits strict silence
- **WHEN** `muted` is toggled on after triggering a synthesis chord
- **THEN** `kochen_audio_mix()` fills every output sample with `0.0f` and zero `NaN`/`Inf` values
