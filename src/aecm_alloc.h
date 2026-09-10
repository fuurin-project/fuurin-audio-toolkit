/* SPDX-License-Identifier: Apache-2.0 */
#ifndef ZAT_AECM_ALLOC_H
#define ZAT_AECM_ALLOC_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int zat_aecm_alloc_begin(uint32_t *bytes);
void zat_aecm_alloc_end(void);
void *zat_aecm_malloc(size_t size);
void *zat_aecm_calloc(size_t count, size_t size);
void *zat_aecm_realloc(void *ptr, size_t size);
void zat_aecm_free(void *ptr);
#ifdef __cplusplus
}
#endif
#endif
