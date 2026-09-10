// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstddef>
#include <cstdint>
using MwwScoreCallback = void (*)(float, void *);
bool mww_init(const unsigned char *, size_t, void *, size_t);
void mww_deinit();
bool mww_reset();
unsigned mww_stride();
unsigned mww_arena_used();
bool mww_process(const int16_t *, size_t, MwwScoreCallback, void *);
