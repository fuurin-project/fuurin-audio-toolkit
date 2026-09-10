// SPDX-License-Identifier: Apache-2.0
#include <zephyr/kernel.h>
#include <sys_malloc.h>
#include <string.h>
#if defined(CONFIG_FUURIN_AUDIO_AFE)
#include <fuurin_audio/audio.h>
#else
#include <fuurin_audio/detector.h>
#endif
#define CHECK(x) do { if (!(x)) { printk("FAIL line %u: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void)
{
    struct sys_memory_stats before, after;
    CHECK(!malloc_runtime_stats_get(&before));
    for (int run = 0; run < 8; ++run) {
#if defined(CONFIG_FUURIN_AUDIO_AFE)
        struct zat_config config = {.sample_rate=16000, .echo_tail_ms=80, .vad_mode=2,
            .aec_enabled=IS_ENABLED(CONFIG_FUURIN_AUDIO_AEC), .vad_disabled=true};
        struct zat_pipeline *p = zat_create(&config);
        CHECK(p);
        int16_t mic[320]={0}, render[320]={0}, out[320];
        bool speech = true;
        for (int i=0; i<20; ++i) CHECK(!zat_process(p, mic, render, out, 320, &speech));
        CHECK(!speech);
        CHECK(zat_get_stats(p)->frames == 20);
        CHECK((zat_get_stats(p)->aec_allocated_bytes > 0) == config.aec_enabled);
        zat_reset(p);
        CHECK(!zat_process(p, mic, render, out, 320, &speech));
        zat_destroy(p);
#else
        struct fuurin_audio_config config;
        fuurin_audio_config_defaults(&config);
        CHECK(!fuurin_audio_init(&config));
        int16_t quiet[160]={0};
        for (int i=0; i<20; ++i) CHECK(!fuurin_audio_process(quiet,160,i*10));
        CHECK(!fuurin_audio_reset());
        CHECK(!fuurin_audio_deinit());
#endif
        CHECK(!malloc_runtime_stats_get(&after));
        CHECK(before.allocated_bytes == after.allocated_bytes);
    }
    printk("PASS: optional configuration, 8 lifecycle cycles, no TFLM, no heap growth\n");
    return 0;
}
