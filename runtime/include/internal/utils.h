#ifndef STELLA_GC_UTILS_H
#define STELLA_GC_UTILS_H

#include <stddef.h>

static inline size_t max(size_t l, size_t r) { return l > r ? l : r; }
static inline size_t min(size_t l, size_t r) { return l < r ? l : r; }

#endif