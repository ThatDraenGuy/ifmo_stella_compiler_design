#ifndef STELLA_GC_CONTEXT_H
#define STELLA_GC_CONTEXT_H

#include "internal/heap.h"
#include <stdbool.h>

typedef struct GcCtx {
  Heap heap;
} GcCtx;

GcCtx *gc_ctx();

static inline Heap *gc_heap(GcCtx *ctx) { return &ctx->heap; }

#endif