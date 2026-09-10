/* SPDX-License-Identifier: Apache-2.0 */
#ifndef ZAT_AECM_COMPAT_H
#define ZAT_AECM_COMPAT_H
/* The upstream file uses <algorithm> only for any_of. Keep its predicate and
 * iteration semantics without pulling a full C++ standard library into Zephyr. */
template <typename Iterator, typename Predicate>
static inline bool zat_any_of(Iterator first, Iterator last, Predicate pred)
{
	for (; first != last; ++first) if (pred(*first)) return true;
	return false;
}
#endif
