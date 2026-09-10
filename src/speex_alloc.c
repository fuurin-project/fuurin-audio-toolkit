/* SPDX-License-Identifier: Apache-2.0 */
#include <speex/speex_echo.h>
#include <setjmp.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>

/* Initialization only: serialize the upstream allocator's context. Independent
 * pipelines may process concurrently; processing never touches this state. */
static atomic_flag initializing = ATOMIC_FLAG_INIT;
static jmp_buf failed;
struct allocation {
	struct allocation *next;
	max_align_t alignment;
};
static struct allocation *head;
static size_t allocated;
#ifdef ZAT_TESTING
int zat_test_alloc_fail_after = -1;
#endif

void *zat_speex_alloc(int size)
{
#ifdef ZAT_TESTING
	if (zat_test_alloc_fail_after == 0) longjmp(failed, 1);
	if (zat_test_alloc_fail_after > 0) zat_test_alloc_fail_after--;
#endif
	struct allocation *a = calloc(1, sizeof(*a) + (size_t)size);
	if (!a) longjmp(failed, 1);
	a->next = head;
	head = a;
	allocated += sizeof(*a) + (size_t)size;
	return a + 1;
}

void zat_speex_free(void *ptr)
{
	if (ptr) free((struct allocation *)ptr - 1);
}

SpeexEchoState *zat_echo_create(int frame, int tail, size_t *bytes)
{
	if (atomic_flag_test_and_set(&initializing)) return NULL;
	head = NULL;
	allocated = 0;
	if (setjmp(failed)) {
		while (head) {
			struct allocation *next = head->next;
			free(head);
			head = next;
		}
		atomic_flag_clear(&initializing);
		return NULL;
	}
	SpeexEchoState *echo = speex_echo_state_init(frame, tail);
	*bytes = allocated;
	head = NULL;
	atomic_flag_clear(&initializing);
	return echo;
}
