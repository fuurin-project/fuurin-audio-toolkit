/* SPDX-License-Identifier: Apache-2.0 */
#include "internal.h"
#include <errno.h>
#include <string.h>
#include <speex/speex_echo.h>

SpeexEchoState *zat_echo_create(int frame, int tail, size_t *bytes);

const char *zat_aec_backend_name(void) { return "speexdsp"; }

int zat_aec_validate(const struct zat_config *c)
{
	return c->echo_tail_ms >= 20 && c->echo_tail_ms <= 200 &&
	       c->echo_tail_ms % 20 == 0 ? 0 : -EINVAL;
}

int zat_aec_init(struct zat_pipeline *p)
{
	if (!p->config.aec_enabled) return 0;
	int rate = (int)p->config.sample_rate;
	/* Two 10 ms AEC blocks per 20 ms application frame reduce working RAM. */
	size_t bytes = 0;
	p->echo = zat_echo_create((int)p->samples / 2,
		(int)(p->config.sample_rate * p->config.echo_tail_ms / 1000), &bytes);
	if (!p->echo) return -ENOMEM;
	p->stats.aec_allocated_bytes = (uint32_t)bytes;
	return speex_echo_ctl(p->echo, SPEEX_ECHO_SET_SAMPLING_RATE, &rate);
}

int zat_aec_process(struct zat_pipeline *p, const int16_t *mic,
		     const int16_t *render, int16_t *out)
{
	if (!p->echo) {
		memcpy(out, mic, p->samples * sizeof(*out));
		return 0;
	}
	size_t half = p->samples / 2;
	speex_echo_cancellation(p->echo, mic, render, out);
	speex_echo_cancellation(p->echo, mic + half, render + half, out + half);
	return 0;
}

void zat_aec_reset(struct zat_pipeline *p)
{
	if (p->echo) speex_echo_state_reset(p->echo);
}

void zat_aec_destroy(struct zat_pipeline *p)
{
	if (p->echo) speex_echo_state_destroy(p->echo);
}
