#include "internal/context.h"
#include "internal/config.h"
#include "internal/roots.h"
#include <stdlib.h>
#include <stella/gc.h>

typedef struct GcCtx {
  Heap heap;
  GcConfig config;
  RootVec roots;
  RootVec perma_roots;
  GcStats stats;
} GcCtx;

static GcCtx *CTX = NULL;

void stella_gc_init(const StellaGcConfig *config) {
  if (CTX != NULL) {
    stella_abi_violation("collector initialised while active");
  }
  if (config == NULL) {
    stella_abi_violation("null collector configuration");
  }
  if (config->abi_version != STELLA_C_GC_ABI_VERSION) {
    stella_abi_violation("incompatible collector ABI version");
  }
  if (config->max_heap_bytes == 0) {
    stella_abi_violation("zero maximum heap size");
  }

  CTX = malloc(sizeof(GcCtx));
  CTX->config.max_heap_bytes = config->max_heap_bytes;
  // TODO fill config

  stats_init(&CTX->stats);
  roots_init(&CTX->roots);
  roots_init(&CTX->perma_roots);

  heap_init(&CTX->heap, CTX->config.min_heap_bytes);
  CTX->stats.heap_size = CTX->config.min_heap_bytes;
}

void stella_gc_shutdown(void) {
  if (CTX == NULL) {
    stella_abi_violation("shutdown called for non-initialised collector");
  }
  if (roots_size(&CTX->roots) != 0) {
    stella_abi_violation(
        "shutdown called with roots still present in the stack");
  }

  roots_destroy(&CTX->roots);
  roots_destroy(&CTX->perma_roots);
  heap_destroy(&CTX->heap);

  free(CTX);
}

inline GcCtx *gc_ctx() {
  if (CTX == NULL) {
    stella_abi_violation("Attempt to access GC while it is not active");
  }
  return CTX;
}

GcConfig *gc_config() { return &gc_ctx()->config; }
Heap *gc_heap() { return &gc_ctx()->heap; }
RootVec *gc_roots() { return &gc_ctx()->roots; };
RootVec *gc_perma_roots() { return &gc_ctx()->perma_roots; }
GcStats *gc_stats() { return &gc_ctx()->stats; }
