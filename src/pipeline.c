/* SPDX-License-Identifier: Apache-2.0 */
#include "internal.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct zat_pipeline *zat_create(const struct zat_config *c)
{
	if (!c || (c->sample_rate != 8000 && c->sample_rate != 16000) ||
	    c->vad_mode < 0 || c->vad_mode > 3 ||
	    (c->aec_enabled && zat_aec_validate(c))) return NULL;
	struct zat_pipeline *p = calloc(1, sizeof(*p));
	if (!p) return NULL;
	p->config = *c;
	p->samples = c->sample_rate / 50;
	if ((!c->vad_disabled && zat_vad_init(p)) || zat_aec_init(p)) {
		zat_destroy(p);
		return NULL;
	}
	return p;
}

void zat_destroy(struct zat_pipeline *p)
{
	if (!p) return;
	zat_aec_destroy(p);
	if (p->vad) fvad_free(p->vad);
	free(p);
}

void zat_reset(struct zat_pipeline *p)
{
	if (!p) return;
	zat_aec_reset(p);
	if (p->vad) {
		fvad_reset(p->vad);
		fvad_set_sample_rate(p->vad, (int)p->config.sample_rate);
		fvad_set_mode(p->vad, p->config.vad_mode);
	}
}

size_t zat_frame_samples(const struct zat_pipeline *p) { return p ? p->samples : 0; }
const struct zat_stats *zat_get_stats(const struct zat_pipeline *p)
{
	return p ? &p->stats : NULL;
}

int zat_process(struct zat_pipeline *p, const int16_t *mic,
		const int16_t *render, int16_t *out, size_t n, bool *speech)
{
	if (speech) *speech = false;
	if (!p || !mic || !out || !speech || n != p->samples ||
	    mic == out || (render && (render == out || render == mic)) ||
	    (p->echo && !render)) {
		if (p) p->stats.errors++;
		return -EINVAL;
	}
	int rc = zat_aec_process(p, mic, render, out);
	if (rc != 0) { p->stats.errors++; return rc; }
	int decision = p->vad ? fvad_process(p->vad, out, n) : 0;
	if (decision < 0) { p->stats.errors++; return -EIO; }
	*speech = decision != 0;
	p->stats.frames++;
	p->stats.speech_frames += *speech;
	uint64_t in_energy = 0, out_energy = 0;
	uint32_t in_peak = 0, out_peak = 0;
	for (size_t i = 0; i < n; i++) {
		int32_t a = mic[i], b = out[i];
		uint32_t aa = (uint32_t)(a < 0 ? -a : a);
		uint32_t bb = (uint32_t)(b < 0 ? -b : b);
		if (aa > in_peak) in_peak = aa;
		if (bb > out_peak) out_peak = bb;
		in_energy += (uint32_t)(a * a);
		out_energy += (uint32_t)(b * b);
		p->stats.clipped_samples += (a == INT16_MIN || a == INT16_MAX);
	}
	p->stats.input_peak = in_peak;
	p->stats.output_peak = out_peak;
	p->stats.input_mean_square = (uint32_t)(in_energy / n);
	p->stats.output_mean_square = (uint32_t)(out_energy / n);
	return 0;
}
