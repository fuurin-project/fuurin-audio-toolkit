// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <sys_malloc.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <fuurin_audio/detector.h>
#include <fvad.h>
alignas(16) static const unsigned char model[] = {
#include "model.inc"
};
static const unsigned char expected[400] = {84,49,27,12,5,2,1};
static bool failure, early_policy, in_vad;
static unsigned index, wakes, starts, ends;
static int expected_vad_event;
static int64_t now_ms, last_wake = -100000;
static void event(const fuurin_audio_event *e, void *) {
    if (fuurin_audio_set_mode(FUURIN_AUDIO_VAD) != -EBUSY) failure = true;
    if (e->type == FUURIN_AUDIO_WAKE_SCORE) {
        if (in_vad || index >= 400) { failure = true; return; }
        unsigned raw = unsigned(e->raw_probability * 256 + .5f);
        unsigned sum = 0;
        if (early_policy || index >= 71)
            for (unsigned j = index > 4 ? index - 4 : 0; j <= index; ++j) sum += expected[j];
        if (raw != expected[index] || unsigned(e->probability * 1280 + .5f) != sum) failure = true;
        ++index;
    } else if (e->type == FUURIN_AUDIO_WAKE_DETECTED) {
        if (in_vad || !early_policy || now_ms - last_wake < 1500) failure = true;
        last_wake = now_ms;
        ++wakes;
    } else {
        if (!in_vad || int(e->type) != expected_vad_event) failure = true;
        expected_vad_event = -1;
        if (e->type == FUURIN_AUDIO_SPEECH_STARTED) ++starts;
        if (e->type == FUURIN_AUDIO_SPEECH_ENDED) ++ends;
    }
}
static bool wake_run() {
    index = 0;
    uint32_t random = 12345678;
    unsigned total = 0, chunk = 0;
    const unsigned chunks[] = {73,247,320,160};
    int16_t pcm[320];
    while (total < 192320) {
        unsigned n = std::min(chunks[chunk++ % 4], 192320 - total);
        for (unsigned i = 0; i < n; ++i) {
            random = random * 1664525u + 1013904223u;
            pcm[i] = int16_t(int((random >> 16) & 16383) - 8192);
        }
        now_ms = total / 16;
        if (fuurin_audio_process(pcm, n, now_ms)) return false;
        total += n;
    }
    return index == 400 && !failure;
}
static bool vad_run() {
    Fvad *reference = fvad_new();
    if (!reference) return false;
    fvad_set_sample_rate(reference, 16000);
    fvad_set_mode(reference, 2);
    bool speaking = false;
    unsigned speech_ms = 0, silence_ms = 0;
    in_vad = true;
    bool ok = true;
    for (unsigned frame = 0; frame < 150; ++frame) {
        int16_t pcm[160];
        for (unsigned i = 0; i < 160; ++i)
            pcm[i] = frame >= 20 && frame < 50 ? ((i % 40 < 20) ? 10000 : -10000) : 0;
        int speech = fvad_process(reference, pcm, 160);
        expected_vad_event = -1;
        if (speech > 0) {
            silence_ms = 0;
            if (!speaking && (speech_ms += 10) >= 30) {
                speaking = true; expected_vad_event = FUURIN_AUDIO_SPEECH_STARTED;
            }
        } else {
            speech_ms = 0;
            if (speaking && (silence_ms += 10) >= 50) {
                speaking = false; expected_vad_event = FUURIN_AUDIO_SPEECH_ENDED;
            }
        }
        now_ms = frame * 10;
        if (fuurin_audio_process(pcm, 73, now_ms) ||
            fuurin_audio_process(pcm + 73, 87, now_ms) || expected_vad_event != -1) ok = false;
    }
    in_vad = false;
    fvad_free(reference);
    return ok && starts && ends && !failure;
}
#define CHECK(x) do { if (!(x)) { printk("FAIL line %u: %s\n", __LINE__, #x); return 1; } } while (0)
int main() {
    fuurin_audio_config c;
    fuurin_audio_config_defaults(&c);
    c.model = model; c.model_size = sizeof(model);
    c.wake_threshold = 0.37265625f;
    c.speech_end_ms = 50;
    c.callback = event;
    CHECK(fuurin_audio_process(nullptr, 0, 0) == -ENODEV);
    struct sys_memory_stats baseline, wake_heap, vad_heap, restored;
    CHECK(malloc_runtime_stats_get(&baseline) == 0);
    CHECK(fuurin_audio_init(&c) == 0);
    CHECK(malloc_runtime_stats_get(&wake_heap) == 0);
    printk("arena=%u\n", unsigned(fuurin_audio_arena_used()));
    CHECK(fuurin_audio_init(&c) == -EALREADY);
    CHECK(wake_run());
    CHECK(fuurin_audio_reset() == 0);
    CHECK(wake_run());
    CHECK(fuurin_audio_set_mode(FUURIN_AUDIO_VAD) == 0);
    CHECK(fuurin_audio_arena_used() == 0);
    CHECK(malloc_runtime_stats_get(&vad_heap) == 0);
    CHECK(vad_heap.allocated_bytes < wake_heap.allocated_bytes);
    printk("frontend freed=%u bytes\n", unsigned(wake_heap.allocated_bytes - vad_heap.allocated_bytes));
    CHECK(fuurin_audio_reset() == 0);
    CHECK(vad_run());
    // Force reinitialization to fail: VAD must remain usable and retryable.
    void *pressure[128];
    unsigned count = 0;
    while (count < 128 && (pressure[count] = malloc(512))) ++count;
    CHECK(count < 128);
    CHECK(fuurin_audio_set_mode(FUURIN_AUDIO_WAKE) == -ENOMEM);
    CHECK(fuurin_audio_arena_used() == 0);
    CHECK(fuurin_audio_reset() == 0);
    int16_t quiet[160] = {};
    CHECK(fuurin_audio_process(quiet, 160, 0) == 0);
    while (count) free(pressure[--count]);
    CHECK(malloc_runtime_stats_get(&restored) == 0);
    CHECK(restored.allocated_bytes == vad_heap.allocated_bytes);
    CHECK(fuurin_audio_set_mode(FUURIN_AUDIO_WAKE) == 0);
    CHECK(malloc_runtime_stats_get(&restored) == 0);
    CHECK(restored.allocated_bytes == wake_heap.allocated_bytes);
    CHECK(wake_run());
    for (unsigned i = 0; i < 10; ++i) {
        CHECK(fuurin_audio_set_mode(FUURIN_AUDIO_VAD) == 0);
        CHECK(fuurin_audio_set_mode(FUURIN_AUDIO_WAKE) == 0);
        CHECK(malloc_runtime_stats_get(&restored) == 0);
        CHECK(restored.allocated_bytes == wake_heap.allocated_bytes);
    }
    CHECK(fuurin_audio_process(nullptr, 1, now_ms) == -EINVAL);
    CHECK(fuurin_audio_process(nullptr, 0, -1) == -EINVAL);
    CHECK(fuurin_audio_deinit() == 0);
    CHECK(malloc_runtime_stats_get(&restored) == 0);
    CHECK(restored.allocated_bytes == baseline.allocated_bytes);
    c.model = model + 1;
    CHECK(fuurin_audio_init(&c) == -EINVAL);
    c.model = model;
    c.warmup_feature_frames = 0;
    c.wake_threshold = .001f;
    early_policy = true;
    CHECK(fuurin_audio_init(&c) == 0);
    CHECK(wake_run());
    CHECK(wakes == 1);
    CHECK(fuurin_audio_deinit() == 0);
    c.model = nullptr; c.model_size = 0;
    CHECK(fuurin_audio_init(&c) == 0);
    CHECK(fuurin_audio_set_mode(FUURIN_AUDIO_WAKE) == -ENOTSUP);
    CHECK(fuurin_audio_arena_used() == 0);
    CHECK(fuurin_audio_deinit() == 0);
    printk("PASS: 4x400 golden wake scores, reset/mode isolation, libfvad events, cooldown, lifecycle and callback guards\n");
    return 0;
}
