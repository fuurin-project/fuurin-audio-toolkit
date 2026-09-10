/* SPDX-License-Identifier: Apache-2.0 */
#ifndef ZEPHYR_AUDIO_TOOLKIT_AUDIO_H
#define ZEPHYR_AUDIO_TOOLKIT_AUDIO_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Mono signed 16-bit PCM, 8/16 kHz, fixed 20 ms frames. */
struct zat_config {
	uint32_t sample_rate;
	uint32_t echo_tail_ms; /* Speex only: 20..200, multiple of 20 */
	int vad_mode; /* libfvad 0..3 */
	bool aec_enabled;
	bool vad_disabled; /* Skip allocation/classification when another detector owns VAD. */
};

struct zat_stats {
	uint32_t aec_allocated_bytes;
	uint32_t frames;
	uint32_t speech_frames;
	uint32_t errors;
	uint32_t input_peak;
	uint32_t output_peak;
	uint32_t input_mean_square;
	uint32_t output_mean_square;
	uint32_t clipped_samples;
};

struct zat_pipeline;
/* Allocate on the control thread before starting audio. NULL on invalid config
 * or allocation failure. One pipeline belongs to one audio worker. */
struct zat_pipeline *zat_create(const struct zat_config *config);
void zat_destroy(struct zat_pipeline *pipeline);
void zat_reset(struct zat_pipeline *pipeline);
size_t zat_frame_samples(const struct zat_pipeline *pipeline);
const struct zat_stats *zat_get_stats(const struct zat_pipeline *pipeline);
/* Build-time backend selection; available even when an instance bypasses AEC. */
const char *zat_aec_backend_name(void);

/* AEC -> output; VAD independently classifies that same output. Feed EVERY
 * output frame to the wake-word model, including frames classified as silence.
 * render must contain time-aligned DAC PCM, or explicit zeros during silence.
 * NULL render is rejected when AEC is enabled. All buffers must be distinct.
 * Returns 0 on success, negative errno on failure; speech is a frame decision,
 * not an endpoint/turn decision. No allocations or callbacks during process. */
int zat_process(struct zat_pipeline *pipeline, const int16_t *mic,
		const int16_t *render, int16_t *output, size_t samples, bool *speech);

#ifdef __cplusplus
}
#endif
#endif
