/* SPDX-License-Identifier: Apache-2.0 */
#include "internal.h"
#include <errno.h>
#include <string.h>
const char *zat_aec_backend_name(void) { return "none"; }
int zat_aec_validate(const struct zat_config *c) { (void)c; return -ENOTSUP; }
int zat_aec_init(struct zat_pipeline *p) { return p->config.aec_enabled ? -ENOTSUP : 0; }
void zat_aec_reset(struct zat_pipeline *p) { (void)p; }
void zat_aec_destroy(struct zat_pipeline *p) { (void)p; }
int zat_aec_process(struct zat_pipeline *p, const int16_t *mic, const int16_t *render, int16_t *out)
{ (void)render; memcpy(out, mic, p->samples * sizeof(*out)); return 0; }
