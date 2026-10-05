#ifndef STELLA_GC_HEAP_H
#define STELLA_GC_HEAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stella/runtime.h>

typedef struct Heap {
  size_t scan;
  size_t next;
  size_t limit;
  size_t end;
  char *to_space;
  char *from_space;
  bool collect_active;
} Heap;

void heap_init(Heap *heap, size_t start_bytes);
void heap_destroy(Heap *heap);
StellaValue heap_alloc(Heap *heap, const StellaObjectDescriptor *const descr);
void heap_step_collect(Heap *heap, uint8_t steps_count);
StellaValue heap_forward(Heap *heap, StellaValue obj);
bool heap_owns_obj(const Heap *heap, StellaValue obj);

#endif