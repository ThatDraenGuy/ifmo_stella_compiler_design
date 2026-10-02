#ifndef STELLA_GC_HEAP_H
#define STELLA_GC_HEAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stella/runtime.h>

typedef struct Heap {
  size_t _scan;
  size_t _next;
  size_t _limit;
  size_t _end;
  char *_to_space;
  char *_from_space;
  bool _collect_active;
} Heap;

StellaValue heap_alloc(Heap *heap, const StellaObjectDescriptor *const descr);
void heap_step_collect(Heap *heap, uint8_t steps_count);
StellaValue heap_forward(Heap *heap, StellaValue obj);

#endif