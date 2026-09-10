/* SPDX-License-Identifier: Apache-2.0 */
#ifndef ZAT_RENDER_HISTORY_H
#define ZAT_RENDER_HISTORY_H
#include <stddef.h>
#include <stdint.h>

#define ZAT_MAX_FRAME_SAMPLES 320
#define ZAT_RENDER_HISTORY_FRAMES 8
struct zat_render_block {
	uint32_t start_us;
	size_t samples;
	int16_t pcm[ZAT_MAX_FRAME_SAMPLES];
};
struct zat_render_history {
	uint32_t write;
	struct zat_render_block blocks[ZAT_RENDER_HISTORY_FRAMES];
};

/* This is render-reference history for AEC, NOT microphone pre-roll.
 * Zero-initialize before use. Caller serializes push/read (e.g. DMA IRQ mask).
 * Timestamps share a wrapping uint32 microsecond clock, and describe the first
 * physical sample. Push only when playback actually starts, after digital gain.
 * Read every capture frame, including silence, to expire old timestamps. */
int zat_render_push(struct zat_render_history *h, uint32_t start_us,
		    const int16_t *pcm, size_t samples);
/* Uncovered intervals are silence. delay_us delays the render reference;
 * keep delay <=60 ms with 20 ms frames and this eight-block history. */
int zat_render_read(struct zat_render_history *h, uint32_t capture_start_us,
		    uint32_t rate, uint32_t delay_us, int16_t *out, size_t samples);
#endif
