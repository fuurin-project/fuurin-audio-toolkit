// SPDX-License-Identifier: Apache-2.0
#include <fuurin_audio/detector.h>
#include <fvad.h>
#include <errno.h>
#include <string.h>
#include "engine.h"

namespace {
fuurin_audio_config config;
bool initialized, busy, have_model;
Fvad *vad;
fuurin_audio_mode mode;
float history[16];
unsigned cursor, frames, pcm_used, speech_ms, silence_ms;
int16_t vad_pcm[160];
bool speaking;
int64_t last_now = -1, current_now, next_trigger;

void emit(fuurin_audio_event_type type, float probability = 0, float raw = 0) {
    const fuurin_audio_event event{type, probability, raw};
    if (config.callback) config.callback(&event, config.user);
}
void score(float raw, void *) {
    history[cursor] = raw;
    cursor = (cursor + 1) % config.smoothing_frames;
    if (frames < config.warmup_feature_frames) frames += mww_stride();
    float mean = 0;
    if (frames >= config.warmup_feature_frames) {
        for (unsigned i = 0; i < config.smoothing_frames; ++i) mean += history[i];
        mean /= config.smoothing_frames;
    }
    emit(FUURIN_AUDIO_WAKE_SCORE, mean, raw);
    if (frames >= config.warmup_feature_frames && mean >= config.wake_threshold &&
        current_now >= next_trigger) {
        next_trigger = current_now + config.cooldown_ms;
        emit(FUURIN_AUDIO_WAKE_DETECTED, mean, raw);
    }
}
int reset_state() {
    memset(history, 0, sizeof(history));
    cursor = frames = pcm_used = speech_ms = silence_ms = 0;
    speaking = false;
    next_trigger = 0;
    last_now = -1;
    fvad_reset(vad);
    if (fvad_set_sample_rate(vad, 16000) || fvad_set_mode(vad, config.vad_aggressiveness)) return -EINVAL;
    if (mode == FUURIN_AUDIO_WAKE && !mww_reset()) return -EIO;
    return 0;
}
int process_vad(const int16_t *pcm, size_t samples) {
    while (samples) {
        unsigned n = (samples < 160 - pcm_used ? samples : 160 - pcm_used);
        memcpy(vad_pcm + pcm_used, pcm, n * sizeof(int16_t));
        pcm_used += n;
        pcm += n;
        samples -= n;
        if (pcm_used != 160) continue;
        pcm_used = 0;
        int speech = fvad_process(vad, vad_pcm, 160);
        if (speech < 0) return -EIO;
        if (speech) {
            silence_ms = 0;
            if (!speaking) {
                speech_ms += 10;
                if (speech_ms >= config.speech_start_ms) {
                    speaking = true;
                    emit(FUURIN_AUDIO_SPEECH_STARTED);
                }
            }
        } else {
            speech_ms = 0;
            if (speaking) {
                silence_ms += 10;
                if (silence_ms >= config.speech_end_ms) {
                    speaking = false;
                    emit(FUURIN_AUDIO_SPEECH_ENDED);
                }
            }
        }
    }
    return 0;
}
}

void fuurin_audio_config_defaults(fuurin_audio_config *c) {
    if (!c) return;
    *c = {};
    c->wake_threshold = 0.5f;
    c->smoothing_frames = 5;
    c->warmup_feature_frames = 216;
    c->cooldown_ms = 1500;
    c->vad_aggressiveness = 2;
    c->speech_start_ms = 30;
    c->speech_end_ms = 500;
}
int fuurin_audio_init(const fuurin_audio_config *c) {
    if (busy) return -EBUSY;
    if (initialized) return -EALREADY;
    if (!c || !(c->wake_threshold > 0 && c->wake_threshold <= 1) ||
        c->smoothing_frames < 1 || c->smoothing_frames > 16 || c->warmup_feature_frames > 1000000 ||
        c->cooldown_ms > 60000 || c->vad_aggressiveness < 0 || c->vad_aggressiveness > 3 ||
        !c->speech_start_ms || c->speech_start_ms > 60000 || c->speech_start_ms % 10 ||
        !c->speech_end_ms || c->speech_end_ms > 60000 || c->speech_end_ms % 10 ||
        (!c->model && c->model_size)) return -EINVAL;
    config = *c;
    have_model = c->model != nullptr;
    if (have_model && !mww_init(c->model, c->model_size, c->tensor_arena, c->tensor_arena_size)) {
        mww_deinit();
        have_model = false;
        return -EINVAL;
    }
    vad = fvad_new();
    if (!vad) {
        if (have_model) mww_deinit();
        have_model = false;
        return -ENOMEM;
    }
    mode = have_model ? FUURIN_AUDIO_WAKE : FUURIN_AUDIO_VAD;
    int result = reset_state();
    if (result) {
        fvad_free(vad); vad = nullptr;
        if (have_model) mww_deinit();
        have_model = false;
        return result;
    }
    initialized = true;
    return 0;
}
int fuurin_audio_deinit() {
    if (busy) return -EBUSY;
    if (!initialized) return 0;
    if (have_model) mww_deinit();
    fvad_free(vad);
    vad = nullptr;
    initialized = have_model = false;
    return 0;
}
int fuurin_audio_reset() {
    if (busy) return -EBUSY;
    if (!initialized) return -ENODEV;
    return reset_state();
}
int fuurin_audio_set_mode(fuurin_audio_mode requested) {
    if (busy) return -EBUSY;
    if (!initialized) return -ENODEV;
    if (requested != FUURIN_AUDIO_WAKE && requested != FUURIN_AUDIO_VAD) return -EINVAL;
    if (requested == FUURIN_AUDIO_WAKE && !have_model) return -ENOTSUP;
    if (requested == mode) return 0;
    if (requested == FUURIN_AUDIO_WAKE) {
        if (!mww_init(config.model, config.model_size, config.tensor_arena,
                      config.tensor_arena_size)) {
            mww_deinit(); // Partial frontend allocations must also be released.
            return -ENOMEM; // Remain usable in VAD mode; caller may retry.
        }
    } else {
        mww_deinit(); // Return frontend malloc allocations before TLS starts.
    }
    mode = requested;
    return reset_state();
}
int fuurin_audio_process(const int16_t *pcm, size_t samples, int64_t now_ms) {
    if (busy) return -EBUSY;
    if (!initialized) return -ENODEV;
    if ((!pcm && samples) || now_ms < 0 || now_ms < last_now || now_ms > INT64_MAX - 60000) return -EINVAL;
    last_now = current_now = now_ms;
    busy = true;
    int result = mode == FUURIN_AUDIO_WAKE ?
        (mww_process(pcm, samples, score, nullptr) ? 0 : -EIO) : process_vad(pcm, samples);
    busy = false;
    return result;
}
size_t fuurin_audio_arena_used() { return initialized && mode == FUURIN_AUDIO_WAKE ? mww_arena_used() : 0; }
