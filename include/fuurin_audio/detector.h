/* SPDX-License-Identifier: Apache-2.0 */
#ifndef FUURIN_AUDIO_DETECTOR_H_
#define FUURIN_AUDIO_DETECTOR_H_
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum fuurin_audio_mode { FUURIN_AUDIO_WAKE, FUURIN_AUDIO_VAD };
enum fuurin_audio_event_type {
    FUURIN_AUDIO_WAKE_SCORE,
    FUURIN_AUDIO_WAKE_DETECTED,
    FUURIN_AUDIO_SPEECH_STARTED,
    FUURIN_AUDIO_SPEECH_ENDED
};
struct fuurin_audio_event {
    enum fuurin_audio_event_type type;
    float probability; /* wake mean; VAD events have 0 (binary classifier) */
    float raw_probability;
};
typedef void (*fuurin_audio_callback)(const struct fuurin_audio_event *, void *user);
struct fuurin_audio_config {
    const unsigned char *model; /* 16-byte aligned TFLite FlatBuffer; caller retains it */
    size_t model_size;
    void *tensor_arena; /* optional 16-byte aligned caller-owned RAM */
    size_t tensor_arena_size;
    float wake_threshold;
    unsigned smoothing_frames; /* 1..16 inference outputs */
    unsigned warmup_feature_frames; /* model-specific streaming context */
    unsigned cooldown_ms;
    int vad_aggressiveness; /* 0..3 */
    unsigned speech_start_ms; /* positive multiple of 10 */
    unsigned speech_end_ms; /* positive multiple of 10 */
    fuurin_audio_callback callback;
    void *user;
};
/* Default timing policy; model/warmup/threshold must be set for your model. */
void fuurin_audio_config_defaults(struct fuurin_audio_config *config);
/* One detector per firmware. All API calls must be serialized by the caller.
 * Callbacks execute synchronously inside process(), must be short, and must
 * not call init/deinit/reset/set_mode/process. Queue application actions instead.
 * PCM: 16 kHz, mono, signed int16; arbitrary chunk lengths are buffered.
 * AEC, microphone drivers, threads and network connections are application-owned.
 * Functions return 0 on success or negative errno. */
int fuurin_audio_init(const struct fuurin_audio_config *config);
int fuurin_audio_deinit(void);
int fuurin_audio_reset(void); /* discard context after missing/discontinuous PCM */
/* VAD releases wake frontend heap allocations; WAKE reconstructs them and
 * starts warmup again. If reconstruction fails, stays in VAD with -ENOMEM.
 * Caller-owned model and tensor arena must remain valid until deinit.
 * arena_used() reports zero in VAD mode; fixed arena RAM is not freed. */
int fuurin_audio_set_mode(enum fuurin_audio_mode mode);
int fuurin_audio_process(const int16_t *pcm, size_t samples, int64_t now_ms);
size_t fuurin_audio_arena_used(void);
#ifdef __cplusplus
}
#endif
#endif
