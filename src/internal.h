/* SPDX-License-Identifier: Apache-2.0 */
#ifndef ZAT_INTERNAL_H
#define ZAT_INTERNAL_H
#include <fuurin_audio/audio.h>
#include <fvad.h>
struct zat_pipeline {
	struct zat_config config;
	size_t samples;
	void *echo;
	Fvad *vad;
	struct zat_stats stats;
};
int zat_aec_validate(const struct zat_config *config);
int zat_aec_init(struct zat_pipeline *p);
int zat_aec_process(struct zat_pipeline *p, const int16_t *mic,
		     const int16_t *render, int16_t *out);
void zat_aec_reset(struct zat_pipeline *p);
void zat_aec_destroy(struct zat_pipeline *p);
int zat_vad_init(struct zat_pipeline *p);
#endif
