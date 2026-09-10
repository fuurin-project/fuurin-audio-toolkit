/* SPDX-License-Identifier: Apache-2.0 */
#include <fuurin_audio/audio.h>
#include <fuurin_audio/render_history.h>
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void test_bypass(uint32_t rate)
{
	struct zat_config c = {rate, 80, 2, false, false};
	struct zat_pipeline *p = zat_create(&c);
	assert(p);
	size_t n = zat_frame_samples(p);
	int16_t in[320] = {0}, out[320];
	bool speech = true;
	assert(zat_process(p, in, NULL, out, n, &speech) == 0);
	assert(!speech && !memcmp(in, out, n * sizeof(*out)));
	in[0] = INT16_MIN; in[1] = INT16_MAX;
	assert(zat_process(p, in, NULL, out, n, &speech) == 0);
	assert(!memcmp(in, out, n * sizeof(*out)));
	assert(zat_get_stats(p)->clipped_samples == 2);
	assert(zat_get_stats(p)->input_peak == 32768);
	assert(zat_process(p, in, NULL, out, n-1, &speech) == -EINVAL);
	assert(!speech);
	assert(zat_process(p, in, NULL, in, n, &speech) == -EINVAL);
	zat_reset(p);
	memset(in, 0, sizeof(in));
	assert(zat_process(p, in, NULL, out, n, &speech) == 0 && !speech);
	zat_destroy(p);
}

static void test_reference(void)
{
	struct zat_render_history h = {0};
	int16_t pcm[320], out[320];
	for (int i = 0; i < 320; i++) pcm[i] = (int16_t)(i + 1);
	/* Wrap the microsecond clock within a playback block. */
	uint32_t t = UINT32_MAX - 9999;
	assert(zat_render_push(&h, t, pcm, 320) == 0);
	assert(zat_render_read(&h, t + 10000, 16000, 0, out, 320) == 0);
	for (int i = 0; i < 160; i++) assert(out[i] == pcm[i + 160]);
	for (int i = 160; i < 320; i++) assert(out[i] == 0);
	assert(zat_render_read(&h, t + 10000, 16000, 10000, out, 320) == 0);
	assert(!memcmp(pcm, out, sizeof(pcm)));
	assert(zat_render_read(&h, t - 10000, 16000, 0, out, 320) == 0);
	for (int i = 0; i < 160; i++) assert(out[i] == 0);
	for (int i = 160; i < 320; i++) assert(out[i] == pcm[i - 160]);
	/* Adjacent blocks must form one continuous reference. */
	assert(zat_render_push(&h, t + 20000, pcm, 320) == 0);
	assert(zat_render_read(&h, t + 10000, 16000, 0, out, 320) == 0);
	for (int i = 0; i < 320; i++) assert(out[i] == pcm[(i + 160) % 320]);
	assert(zat_render_read(&h, t + 40000, 16000, 0, out, 320) == 0);
	for (int i = 0; i < 320; i++) assert(out[i] == 0);
	assert(zat_render_read(&h, t + 300000, 16000, 0, out, 320) == 0);
	assert(h.blocks[0].samples == 0 && h.blocks[1].samples == 0);
}

static void test_echo(uint32_t rate)
{
	struct zat_config c = {rate, 80, 2, true, false};
	struct zat_pipeline *p = zat_create(&c);
	assert(p);
	size_t n = zat_frame_samples(p);
	int16_t render[320] = {0}, mic[320] = {0}, out[320], delay[128] = {0};
	uint32_t seed = 7, pos = 0;
	double before = 0, after = 0;
	bool speech;
	assert(zat_process(p, mic, NULL, out, n, &speech) == -EINVAL);
	/* Deterministic broadband far-end with two delayed linear echo paths.
	 * Measure after adaptation, without noise suppression or near-end speech. */
	for (int frame = 0; frame < 600; frame++) {
		for (size_t i = 0; i < n; i++) {
			seed = seed * 1664525u + 1013904223u;
			render[i] = (int16_t)((int)(seed >> 19) - 4096);
			delay[pos % 128] = render[i];
			mic[i] = (int16_t)(delay[(pos - 40) % 128] / 2 +
					   delay[(pos - 73) % 128] / 4);
			pos++;
		}
		assert(zat_process(p, mic, render, out, n, &speech) == 0);
		if (frame >= 500) for (size_t i = 0; i < n; i++) {
			before += (double)mic[i] * mic[i];
			after += (double)out[i] * out[i];
		}
	}
	double erle = 10 * log10(before / (after + 1));
	printf("%u Hz synthetic linear echo ERLE: %.1f dB, AEC allocations: %u bytes\n",
	       rate, erle, zat_get_stats(p)->aec_allocated_bytes);
	assert(erle > 12.0);
	assert(zat_get_stats(p)->frames == 600);
	zat_reset(p);
	memset(mic, 0, sizeof(mic)); memset(render, 0, sizeof(render));
	for (int i = 0; i < 10; i++) {
		assert(zat_process(p, mic, render, out, n, &speech) == 0);
		assert(!speech);
	}
	zat_destroy(p);
}

int main(void)
{
	extern int zat_test_alloc_fail_after;
	/* Exercise every Speex allocation failure, including partial FFT setup. */
	struct zat_config config = {16000, 80, 2, true, false};
	int success = 0;
	for (int i = 0; i < 100; i++) {
		zat_test_alloc_fail_after = i;
		struct zat_pipeline *p = zat_create(&config);
		if (p) { zat_destroy(p); success = 1; break; }
	}
	assert(success);
	zat_test_alloc_fail_after = -1;
	assert(!zat_create(NULL));
	struct zat_config bad = {44100, 80, 2, true, false};
	assert(!zat_create(&bad));
	bad.sample_rate = 16000; bad.vad_mode = 4;
	assert(!zat_create(&bad));
	bad.vad_mode = 2; bad.echo_tail_ms = 81;
	assert(!zat_create(&bad));
	/* AEC-only mode must not allocate or call libfvad. */
	struct zat_config aec_only = {16000, 80, 2, true, true};
	struct zat_pipeline *only = zat_create(&aec_only);
	assert(only);
	int16_t quiet[320] = {0}, reference[320] = {0}, output[320];
	bool detected = true;
	assert(zat_process(only, quiet, reference, output, 320, &detected) == 0);
	assert(!detected && zat_get_stats(only)->speech_frames == 0);
	zat_reset(only);
	assert(zat_process(only, quiet, reference, output, 320, &detected) == 0);
	zat_destroy(only);
	test_bypass(8000); test_bypass(16000);
	test_reference();
	test_echo(8000); test_echo(16000);
	puts("All audio toolkit tests passed");
	return 0;
}
