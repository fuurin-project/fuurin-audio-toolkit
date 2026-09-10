/* SPDX-License-Identifier: Apache-2.0 */
#include <fuurin_audio/render_history.h>
#include <errno.h>
#include <string.h>

int zat_render_push(struct zat_render_history *h, uint32_t start,
		    const int16_t *pcm, size_t n)
{
	if (!h || !pcm || !n || n > ZAT_MAX_FRAME_SAMPLES) return -EINVAL;
	struct zat_render_block *b = &h->blocks[h->write];
	b->start_us = start;
	b->samples = n;
	memcpy(b->pcm, pcm, n * sizeof(*pcm));
	h->write = (h->write + 1) % ZAT_RENDER_HISTORY_FRAMES;
	return 0;
}

int zat_render_read(struct zat_render_history *h, uint32_t start,
		    uint32_t rate, uint32_t delay, int16_t *out, size_t n)
{
	if (!h || !out || !n || n > ZAT_MAX_FRAME_SAMPLES ||
	    (rate != 8000 && rate != 16000) || delay > 60000) return -EINVAL;
	memset(out, 0, n * sizeof(*out));
	/* Oldest first, so a newer playback event wins any timestamp overlap. */
	for (size_t j = 0; j < ZAT_RENDER_HISTORY_FRAMES; j++) {
		struct zat_render_block *b = &h->blocks[(h->write + j) % ZAT_RENDER_HISTORY_FRAMES];
		if (!b->samples) continue;
		int32_t age = (int32_t)(start - delay - b->start_us);
		if (age > 200000) { b->samples = 0; continue; }
		if (age < -200000) continue;
		/* Round to the nearest sample; 16 kHz has a 62.5 us period. */
		int64_t scaled = (int64_t)age * rate;
		int offset = (int)((scaled + (scaled >= 0 ? 500000 : -500000)) / 1000000);
		int dst = offset < 0 ? -offset : 0;
		int src = offset > 0 ? offset : 0;
		if (dst >= (int)n || src >= (int)b->samples) continue;
		size_t count = n - (size_t)dst;
		if (count > b->samples - (size_t)src) count = b->samples - (size_t)src;
		memcpy(out + dst, b->pcm + src, count * sizeof(*out));
	}
	return 0;
}
