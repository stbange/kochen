#ifndef KOCHEN_AUDIO_H
#define KOCHEN_AUDIO_H

#include "core/kochen_types.h"

#define KOCHEN_AUDIO_VOICES 4

typedef struct {
    bool active;
    float freq_hz;
    float phase;
    float env;
    float decay_per_sample;
    float amp;
    uint8_t waveform; /* 0 = warm sine+2nd, 1 = crisp detent cosine, 2 = warning triangle */
} kochen_voice_t;

typedef struct {
    uint32_t sample_rate;
    bool muted;
    kochen_voice_t voices[KOCHEN_AUDIO_VOICES];
} kochen_audio_t;

void kochen_audio_init(kochen_audio_t *au, uint32_t sample_rate);
void kochen_audio_set_muted(kochen_audio_t *au, bool muted);
void kochen_audio_trigger_detent(kochen_audio_t *au, float norm_val01);
void kochen_audio_trigger_patch(kochen_audio_t *au, const kochen_diagnostics_t *diag);
void kochen_audio_mix(kochen_audio_t *au, float *out_mono, size_t frames);

#endif /* KOCHEN_AUDIO_H */
