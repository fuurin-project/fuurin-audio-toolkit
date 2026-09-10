/* SPDX-License-Identifier: Apache-2.0 */
#include "internal.h"
#include <errno.h>

int zat_vad_init(struct zat_pipeline *p)
{
	p->vad = fvad_new();
	if (!p->vad) return -ENOMEM;
	if (fvad_set_sample_rate(p->vad, (int)p->config.sample_rate) ||
	    fvad_set_mode(p->vad, p->config.vad_mode)) return -EINVAL;
	return 0;
}
