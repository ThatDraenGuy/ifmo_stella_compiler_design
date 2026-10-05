#ifndef STELLA_GC_CONTEXT_H
#define STELLA_GC_CONTEXT_H

#include "internal/config.h"
#include "internal/heap.h"
#include "internal/roots.h"
#include "internal/stats.h"
#include <stdbool.h>

GcConfig *gc_config();
Heap *gc_heap();
RootVec *gc_roots();
RootVec *gc_perma_roots();
GcStats *gc_stats();

#endif