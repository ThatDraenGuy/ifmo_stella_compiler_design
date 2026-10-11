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
  stella_gc_out_of_memory(requested_bytes);
}

static StellaValue perform_forward(Heap *heap, StellaValue obj) {
  if (obj == NULL || !heap_is_ptr_from_space(heap, obj)) {
    return obj;
  }
  if (obj->gc_word != GC_WORD_NULL) {
    return (StellaValue)(obj->gc_word);
  }
  size_t size = rounded_size(object_size(obj->descriptor));
  if (heap->next + size > heap->limit) {
    // падаем
    handle_oom(heap, size);
  }
  StellaValue newObj = memcpy(heap->to_space + heap->next, obj, size);

  // the most evil thing imaginable. why.
  newObj->fields =
      obj->descriptor->slot_capacity == 0 ? NULL : (StellaSlot *)(newObj + 1);

  heap->next += size;
  obj->gc_word = (uintptr_t)newObj;
  return newObj;
}

static void perform_step_collect(Heap *heap) {
  StellaValue obj = (StellaValue)(heap->to_space + heap->scan);

  for (size_t i = 0; i < obj->descriptor->managed_count; i++) {
    StellaValue field = obj->fields[i].managed;
    obj->fields[i].managed = perform_forward(heap, field);
  }
  // TODO get rid of size recalculation?
  size_t size = rounded_size(object_size(obj->descriptor));
  heap->scan += size;
  gc_stats()->collection_steps++;
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
  gc_stats()->collections++;
}

static void finish_collect(Heap *heap) {
  while (heap->scan < heap->next) {
    perform_step_collect(heap);
  }
  // end collect
  heap->collect_active = false;
  char *tmp = heap->to_space;
  heap->to_space = heap->from_space;
  heap->from_space = tmp;

  gc_stats()->out_of_budget_scans++;
}

static size_t resolve_obj_size(const StellaObjectDescriptor *const descr) {
  size_t obj_size = object_size(descr);
  size_t round_size = rounded_size(obj_size);
  gc_stats()->requested_bytes += obj_size;
  gc_stats()->rounded_bytes += round_size;
  return round_size;
}

static StellaValue alloc_from_space(Heap *heap,
                                    const StellaObjectDescriptor *const descr,
                                    size_t size);
static StellaValue alloc_to_space(Heap *heap,
                                  const StellaObjectDescriptor *const descr,
                                  size_t size);

static bool will_heap_overflow(const Heap *heap, size_t size_to_be_added) {
  return gc_config()->collect_start_threshold == 1.0
             ? heap->next + size_to_be_added >= heap->limit
             : heap->next + size_to_be_added + (heap->end - heap->limit) >=
                   (size_t)((double)heap->end *
                            gc_config()->collect_start_threshold);
}

static StellaValue alloc_to_space(Heap *heap,
                                  const StellaObjectDescriptor *const descr,
                                  size_t size) {
  if (will_heap_overflow(heap, size)) {
    // если места нет - завершаем сборку и выделяем объект в from-space
    finish_collect(heap);
    return alloc_from_space(heap, descr, size);
  }
  heap->limit -= size;
  StellaValue res = (StellaValue)(heap->to_space + heap->limit);
  res->gc_word = GC_WORD_NULL;
  return res;
}

static StellaValue alloc_from_space(Heap *heap,
                                    const StellaObjectDescriptor *const descr,
                                    size_t size) {
  if (will_heap_overflow(heap, size)) {
    // если места нет - начинаем сборку и выделяем объект в конце to-space
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
  res->descriptor = descr;

  gc_stats()->occupied_bytes = count_occupied_bytes(heap);
  if (gc_stats()->occupied_bytes > gc_stats()->maximum_occupied_bytes) {
    gc_stats()->maximum_occupied_bytes = gc_stats()->occupied_bytes;
  }
  return res;
}

void heap_step_collect(Heap *heap, uint8_t steps_count) {
  if (!heap->collect_active) {
    return;
  }

  while (steps_count-- > 0 && heap->scan < heap->next) {
    perform_step_collect(heap);
  }
  if (heap->scan == heap->next) {
    // end collect
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