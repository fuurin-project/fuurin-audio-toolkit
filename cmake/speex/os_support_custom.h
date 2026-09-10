/* SPDX-License-Identifier: Apache-2.0 */
/* Speex 1.2.1 does not check every allocation during echo initialization.
 * The adapter unwinds a failed initialization before NULL can be dereferenced. */
#define OVERRIDE_SPEEX_ALLOC
#define OVERRIDE_SPEEX_FREE
void *zat_speex_alloc(int size);
void zat_speex_free(void *ptr);
#define speex_alloc zat_speex_alloc
#define speex_free zat_speex_free
