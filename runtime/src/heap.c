#include <internal/context.h>
#include <internal/heap.h>
#include <internal/utils.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stella/gc.h>
#include <string.h>

#define GC_WORD_NULL 0

static bool add_overflows(size_t left, size_t right) {
  return left > SIZE_MAX - right;
}

static bool multiply_overflows(size_t left, size_t right) {
  return right != 0 && left > SIZE_MAX / right;
}

static size_t object_size(const StellaObjectDescriptor *descriptor) {
  if (multiply_overflows(descriptor->slot_capacity, sizeof(StellaSlot))) {
    stella_abi_violation("object slot capacity overflows size_t");
  }
  size_t slots = descriptor->slot_capacity * sizeof(StellaSlot);
  if (add_overflows(sizeof(StellaObject), slots)) {
    stella_abi_violation("object size overflows size_t");
  }
  return sizeof(StellaObject) + slots;
}

static size_t rounded_size(size_t size) {
  size_t alignment = _Alignof(max_align_t);
  size_t remainder = size % alignment;
  if (remainder == 0) {
    return size;
  }
  size_t padding = alignment - remainder;
  if (add_overflows(size, padding)) {
    stella_abi_violation("aligned object size overflows size_t");
  }
  return size + padding;
}

static bool heap_is_ptr_from_space(const Heap *heap, void *ptr) {
  char *cptr = (char *)ptr;
  return cptr >= heap->from_space && cptr < (heap->from_space + heap->end);
}

static bool heap_is_ptr_to_space(const Heap *heap, void *ptr) {
  char *cptr = (char *)ptr;
  return cptr >= heap->to_space && cptr < (heap->to_space + heap->end);
}

static void handle_oom(Heap *heap, size_t requested_bytes) {
  GcConfig *conf = gc_config();
  if (heap->end == conf->max_heap_bytes) {
    gc_stats()->failed_allocations++;
    stella_gc_out_of_memory(requested_bytes);
  }

  // пытаемся увеличить размер кучи так, чтобы запрошенные байты влезли
  size_t new_heap_size = heap->end;
  do {
    size_t new_heap_size =
        max(conf->max_heap_bytes,
            (size_t)(conf->heap_growth_factor * (double)heap->end));
  } while (new_heap_size - heap->end < requested_bytes &&
           new_heap_size != conf->max_heap_bytes);

  if (new_heap_size - heap->end < requested_bytes) {
    gc_stats()->failed_allocations++;
    stella_gc_out_of_memory(requested_bytes);
  }

  heap->to_space = realloc(heap->to_space, new_heap_size);
  heap->from_space = realloc(heap->from_space, new_heap_size);

  //"смещаем" свежие объекты в новый конец to_space.
  memmove(heap->to_space + new_heap_size - (heap->end - heap->limit),
          heap->to_space + heap->limit, heap->end - heap->limit);
  heap->limit = new_heap_size - (heap->end - heap->limit);
  heap->end = new_heap_size;

  gc_stats()->heap_growths++;
  gc_stats()->heap_size = new_heap_size;
}

static StellaValue perform_forward(Heap *heap, StellaValue obj) {
  if (obj == NULL || !heap_is_ptr_from_space(heap, obj)) {
    return obj;
  }
  if (obj->gc_word != 0) {
    return (StellaValue)(obj->gc_word);
  }
  size_t size = rounded_size(object_size(obj->descriptor));
  if (heap->next + size > heap->limit) {
    // либо падаем, либо увеличиваем размер кучи
    handle_oom(heap, size);
  }
  StellaValue newObj = memcpy(heap->to_space + heap->next, obj, size);
  heap->next += size;
  obj->gc_word = (uintptr_t)newObj;
  return newObj;
}

StellaValue heap_forward(Heap *heap, StellaValue obj) {
  if (!heap->collect_active) {
    return obj;
  }
  return perform_forward(heap, obj);
}

static void start_collect(Heap *heap) {
  heap->collect_active = true;
  heap->scan = 0;
  heap->next = 0;
  heap->limit = heap->end;
  RootVec *perma_roots = gc_perma_roots();
  roots_foreach(perma_roots, perma_root,
                { *perma_root = perform_forward(heap, *perma_root); });

  RootVec *roots = gc_roots();
  roots_foreach(roots, root, { *root = perform_forward(heap, *root); });
}

static size_t resolve_obj_size(const StellaObjectDescriptor *const descr) {
  size_t obj_size = object_size(descr);
  size_t round_size = rounded_size(obj_size);
  gc_stats()->requested_bytes += obj_size;
  gc_stats()->rounded_bytes += round_size;
  return round_size;
}

static StellaValue alloc_to_space(Heap *heap,
                                  const StellaObjectDescriptor *const descr,
                                  size_t size) {
  if (heap->next >= heap->limit - size) {
    handle_oom(heap, size);
  }
  heap->limit -= size;
  StellaValue res = (StellaValue)(heap->to_space + heap->limit);
  res->gc_word = GC_WORD_NULL;
  return res;
}

static StellaValue alloc_from_space(Heap *heap,
                                    const StellaObjectDescriptor *const descr,
                                    size_t size) {
  if (heap->next + size >= heap->limit) {
    start_collect(heap);
    return alloc_to_space(heap, descr, size);
  } else {
    StellaValue res = (StellaValue)(heap->from_space + heap->next);
    res->gc_word = GC_WORD_NULL;
    heap->next += size;
    return res;
  }
}

static size_t count_occupied_bytes(const Heap *heap) {
  return (heap->next + (heap->end - heap->limit));
}

StellaValue heap_alloc(Heap *heap, const StellaObjectDescriptor *const descr) {
  size_t size = resolve_obj_size(descr);
  StellaValue res;
  if (heap->collect_active) {
    res = alloc_to_space(heap, descr, size);
  } else {
    res = alloc_from_space(heap, descr, size);
  }

  res->gc_word = GC_WORD_NULL;
  res->fields = descr->slot_capacity == 0 ? NULL : (StellaSlot *)(res + 1);

  gc_stats()->occupied_bytes = count_occupied_bytes(heap);
  if (gc_stats()->occupied_bytes > gc_stats()->maximum_occupied_bytes) {
    gc_stats()->maximum_occupied_bytes = gc_stats()->occupied_bytes;
  }
  return res;
}

static void shrink_if_underfilled(Heap *heap) {
  //"сжимаем" кучу если после сборки в ней осталось "много" свободного места
  GcConfig *conf = gc_config();
  size_t used = heap->next + (heap->end - heap->limit);
  if (used < (size_t)(conf->heap_shrink_threshold * (double)heap->end) &&
      heap->end > conf->min_heap_bytes) {
    size_t new_heap_size =
        min(conf->min_heap_bytes,
            (size_t)((double)heap->end / conf->heap_growth_factor));
    memmove(heap->to_space + new_heap_size - (heap->end - heap->limit),
            heap->to_space + heap->limit, heap->end - heap->limit);
    heap->to_space = realloc(heap->to_space, new_heap_size);
    heap->from_space = realloc(heap->from_space, new_heap_size);
    heap->limit = new_heap_size - (heap->end - heap->limit);
    heap->end = new_heap_size;

    gc_stats()->heap_shrinks++;
    gc_stats()->heap_size = new_heap_size;
  }
}

void heap_step_collect(Heap *heap, uint8_t steps_count) {
  if (!heap->collect_active) {
    return;
  }

  while (steps_count-- > 0 && heap->scan < heap->next) {
    StellaValue obj = (StellaValue)(heap->to_space + heap->scan);

    for (size_t i = 0; i < obj->descriptor->managed_count; i++) {
      StellaValue field = obj->fields[i].managed;
      obj->fields[i].managed = heap_forward(heap, field);
    }
    // TODO get rid of size recalculation?
    size_t size = rounded_size(object_size(obj->descriptor));
    heap->scan += size;
    gc_stats()->collection_steps++;
  }
  if (heap->scan == heap->next) {
    // end collect
    shrink_if_underfilled(heap);
    heap->collect_active = false;
    char *tmp = heap->to_space;
    heap->to_space = heap->from_space;
    heap->from_space = tmp;
  }
}

bool heap_owns_obj(const Heap *heap, StellaValue obj) {
  // TODO check object borders?
  // actually, probably unneccessary
  return heap_is_ptr_to_space(heap, obj) || heap_is_ptr_from_space(heap, obj);
}

void heap_init(Heap *heap, size_t start_bytes) {
  heap->scan = 0;
  heap->next = 0;
  heap->limit = start_bytes;
  heap->end = start_bytes;
  heap->collect_active = false;
  heap->to_space = malloc(start_bytes);
  heap->from_space = malloc(start_bytes);
}

void heap_destroy(Heap *heap) {
  free(heap->to_space);
  free(heap->from_space);
}