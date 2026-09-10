/* SPDX-License-Identifier: Apache-2.0 */
#include "internal.h"
#include "aecm_alloc.h"
#include <echo_control_mobile.h>
#include <errno.h>
#include <string.h>

#ifndef CONFIG_FUURIN_AUDIO_AECM_ECHO_MODE
#define CONFIG_FUURIN_AUDIO_AECM_ECHO_MODE 3
#endif

const char *zat_aec_backend_name(void) { return "webrtc-aecm"; }

int zat_aec_validate(const struct zat_config *c)
{
	/* Sampling rate/frame size are checked by the common pipeline.
	 * AECM has its own delay estimator, not a configurable Speex filter tail. */
	(void)c;
	return 0;
}

static int configure(struct zat_pipeline *p)
{
	if (ZatAecm_WebRtcAecm_Init(p->echo, (int32_t)p->config.sample_rate)) return -EIO;
	AecmConfig config = {.cngMode = 0, .echoMode = CONFIG_FUURIN_AUDIO_AECM_ECHO_MODE};
#if defined(CONFIG_FUURIN_AUDIO_AECM_CNG) && CONFIG_FUURIN_AUDIO_AECM_CNG
	config.cngMode = 1;
#endif
	return ZatAecm_WebRtcAecm_set_config(p->echo, config) ? -EIO : 0;
}

int zat_aec_init(struct zat_pipeline *p)
{
	if (!p->config.aec_enabled) return 0;
	int rc = zat_aecm_alloc_begin(&p->stats.aec_allocated_bytes);
	if (rc) return rc;
	p->echo = ZatAecm_WebRtcAecm_Create();
	/* Init writes upstream global function pointers; serialize it with create. */
	rc = p->echo ? configure(p) : -ENOMEM;
	zat_aecm_alloc_end();
	return rc;
}

int zat_aec_process(struct zat_pipeline *p, const int16_t *mic,
		    const int16_t *render, int16_t *out)
{
	if (!p->echo) {
		memcpy(out, mic, p->samples * sizeof(*out));
		return 0;
	}
	size_t half = p->samples / 2;
	for (size_t offset = 0; offset < p->samples; offset += half) {
		if (ZatAecm_WebRtcAecm_BufferFarend(p->echo, render + offset, half)) return -EIO;
		/* Render is already aligned with capture by the caller: no additional
		 * sound-card buffering delay. Do not apply the board delay twice. */
		if (ZatAecm_WebRtcAecm_Process(p->echo, mic + offset, NULL,
					     out + offset, half, 0)) return -EIO;
	}
	return 0;
}

void zat_aec_reset(struct zat_pipeline *p)
{
	if (p->echo && configure(p)) p->stats.errors++;
}

void zat_aec_destroy(struct zat_pipeline *p)
{
	if (p->echo) ZatAecm_WebRtcAecm_Free(p->echo);
}
