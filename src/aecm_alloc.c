/* SPDX-License-Identifier: Apache-2.0 */
#include "aecm_alloc.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* All upstream allocation occurs during create, not process/reset. Per-block
 * ownership also keeps diagnostics correct when another instance is destroyed. */
static atomic_flag creating = ATOMIC_FLAG_INIT;
static uint32_t *owner;
struct block {
	max_align_t alignment;
	uint32_t *owner;
	size_t size;
};

int zat_aecm_alloc_begin(uint32_t *bytes)
{
	if (atomic_flag_test_and_set(&creating)) return -EBUSY;
	owner = bytes;
	*bytes = 0;
	return 0;
}

void zat_aecm_alloc_end(void)
{
	owner = NULL;
	atomic_flag_clear(&creating);
}

static void *allocate(size_t size, uint32_t *counter)
{
	if (!counter || size > UINT32_MAX - sizeof(struct block) ||
	    *counter > UINT32_MAX - sizeof(struct block) - size) return NULL;
	struct block *b = malloc(sizeof(*b) + size);
	if (!b) return NULL;
	b->owner = counter;
	b->size = size;
	*counter += (uint32_t)(sizeof(*b) + size);
	return b + 1;
}

void *zat_aecm_malloc(size_t size) { return allocate(size, owner); }

void *zat_aecm_calloc(size_t count, size_t size)
{
	if (size && count > SIZE_MAX / size) return NULL;
	void *ptr = zat_aecm_malloc(count * size);
	if (ptr) memset(ptr, 0, count * size);
	return ptr;
}

void zat_aecm_free(void *ptr)
{
	if (!ptr) return;
	struct block *b = (struct block *)ptr - 1;
	*b->owner -= (uint32_t)(sizeof(*b) + b->size);
	free(b);
}

void *zat_aecm_realloc(void *ptr, size_t size)
{
	if (!ptr) return zat_aecm_malloc(size);
	if (!size) { zat_aecm_free(ptr); return NULL; }
	struct block *b = (struct block *)ptr - 1;
	void *next = allocate(size, b->owner);
	if (!next) return NULL;
	memcpy(next, ptr, size < b->size ? size : b->size);
	zat_aecm_free(ptr);
	return next;
}
