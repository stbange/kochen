#include "ui/kochen_audio.h"
#include "core/kochen_physics.h"
#include <math.h>
#include <string.h>

#define KOCHEN_TAU 6.28318530718f

void kochen_audio_init(kochen_audio_t *au, uint32_t sample_rate) {
    if (!au) return;
    memset(au, 0, sizeof(*au));
    au->sample_rate = sample_rate >= 8000u ? sample_rate : 44100u;
    au->muted = false;
}

void kochen_audio_set_muted(kochen_audio_t *au, bool muted) {
    if (!au) return;
    au->muted = muted;
    if (muted) memset(au->voices, 0, sizeof(au->voices));
}

static void start_voice(
    kochen_audio_t *au,
    int slot,
    float freq_hz,
    float dur_sec,
    float amp,
    uint8_t waveform
) {
    if (au->muted || slot < 0 || slot >= KOCHEN_AUDIO_VOICES) return;
    kochen_voice_t *v = &au->voices[slot];
    v->active = true;
    v->freq_hz = fminf(4000.0f, fmaxf(55.0f, freq_hz));
    v->phase = 0.0f;
    v->env = 1.0f;
    v->decay_per_sample = powf(0.001f, 1.0f / fmaxf(32.0f, dur_sec * (float)au->sample_rate));
    v->amp = amp;
    v->waveform = waveform;
}

void kochen_audio_trigger_detent(kochen_audio_t *au, float norm_val01) {
    if (!au || au->muted) return;
    float clamped = fminf(1.0f, fmaxf(0.0f, norm_val01));
    start_voice(au, 0, 520.0f + 440.0f * clamped, 0.014f, 0.14f, 1);
}

void kochen_audio_trigger_patch(kochen_audio_t *au, const kochen_diagnostics_t *diag) {
    if (!au || !diag || au->muted) return;
    bool ok = diag->emulsion_stable && diag->ph_in_window && diag->hydration_in_window && diag->gel_in_window;
    uint32_t bridges = kochen_popcount32(diag->shared_volatile_mask);
    float root = 220.0f + 12.0f * (float)(bridges % 7u);

    if (ok) {
        /* Warm major-9th / fifth consonance reflecting physical & volatile harmony */
        start_voice(au, 1, root,          0.26f, 0.16f, 0);
        start_voice(au, 2, root * 1.25f,  0.24f, 0.12f, 0);
        start_voice(au, 3, root * 1.50f,  0.28f, 0.11f, 0);
    } else {
        /* Gentle dissonant tritone + minor-second dyad signaling physical tension */
        start_voice(au, 1, 196.0f,        0.24f, 0.16f, 2);
        start_voice(au, 2, 196.0f * 1.4142f, 0.24f, 0.14f, 2);
        start_voice(au, 3, 196.0f * 1.0667f, 0.20f, 0.10f, 2);
    }
}

void kochen_audio_mix(kochen_audio_t *au, float *out_mono, size_t frames) {
    if (!out_mono || frames == 0) return;
    memset(out_mono, 0, frames * sizeof(float));
    if (!au || au->muted) return;

    float inv_sr = 1.0f / (float)au->sample_rate;
    for (size_t i = 0; i < frames; i++) {
        float s = 0.0f;
        for (int k = 0; k < KOCHEN_AUDIO_VOICES; k++) {
            kochen_voice_t *v = &au->voices[k];
            if (!v->active) continue;
            float w = 0.0f;
            if (v->waveform == 0)
                w = 0.8f * sinf(v->phase) + 0.2f * sinf(2.0f * v->phase);
            else if (v->waveform == 1)
                w = cosf(v->phase);
            else
                w = (2.0f / 3.14159265f) * asinf(sinf(v->phase));

            s += w * v->env * v->amp;
            v->phase += KOCHEN_TAU * v->freq_hz * inv_sr;
            if (v->phase >= KOCHEN_TAU) v->phase -= KOCHEN_TAU;
            v->env *= v->decay_per_sample;
            if (v->env < 0.0012f) v->active = false;
        }
        out_mono[i] = fminf(0.95f, fmaxf(-0.95f, s));
    }
}
