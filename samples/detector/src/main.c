/* SPDX-License-Identifier: Apache-2.0 */
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <fuurin_audio/detector.h>
#ifdef SAMPLE_HAS_MODEL
static const unsigned char model[] __aligned(16) = {
#include "model.inc"
};
#endif
static bool wake, speech_ended;
static void event(const struct fuurin_audio_event *e, void *user)
{
    (void)user;
    if (e->type == FUURIN_AUDIO_WAKE_DETECTED) {
        printk("Wake detected\n");
        wake = true; /* Defer mode changes until process() returns. */
    } else if (e->type == FUURIN_AUDIO_SPEECH_STARTED) {
        printk("Speech started\n");
    } else if (e->type == FUURIN_AUDIO_SPEECH_ENDED) {
        printk("Speech ended\n");
        speech_ended = true;
    }
}
int main(void)
{
    struct fuurin_audio_config config;
    fuurin_audio_config_defaults(&config);
#ifdef SAMPLE_HAS_MODEL
    config.model = model;
    config.model_size = sizeof(model);
    /* Example Hey Pico v2 policy: set these from YOUR model metadata. */
    config.wake_threshold = 0.37265625f;
    config.warmup_feature_frames = 216;
#endif
    config.callback = event;
    int ret = fuurin_audio_init(&config);
    if (ret) { printk("Detector init failed: %d\n", ret); return ret; }
    printk("Fuurin detector ready; this sample feeds silence, not a microphone\n");
    for (;;) {
        /* Replace with 16 kHz mono microphone PCM, optionally after AEC. */
        int16_t pcm[160] = {0};
        ret = fuurin_audio_process(pcm, 160, k_uptime_get());
        if (ret) { printk("Process failed: %d\n", ret); return ret; }
        if (wake) {
            wake = false;
            ret = fuurin_audio_set_mode(FUURIN_AUDIO_VAD);
#ifdef SAMPLE_HAS_MODEL
        } else if (speech_ended) {
            speech_ended = false;
            ret = fuurin_audio_set_mode(FUURIN_AUDIO_WAKE);
#endif
        }
        if (ret) return ret;
        k_msleep(10);
    }
}
