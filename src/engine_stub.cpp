// SPDX-License-Identifier: Apache-2.0
#include "engine.h"
bool mww_init(const unsigned char *, size_t, void *, size_t) { return false; }
void mww_deinit() {}
bool mww_reset() { return false; }
unsigned mww_stride() { return 0; }
unsigned mww_arena_used() { return 0; }
bool mww_process(const int16_t *, size_t, MwwScoreCallback, void *) { return false; }
