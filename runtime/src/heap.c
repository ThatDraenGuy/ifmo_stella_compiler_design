#include <internal/heap.h>
#include <stdbool.h>
#include <stella/gc.h>
#include <string.h>

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
  return cptr >= heap->_from_space && cptr < (heap->_from_space + heap->_end);
}

static void handle_oom(Heap *heap) {
  // TODO
}

static StellaValue perform_forward(Heap *heap, StellaValue obj) {
  if (obj == NULL || !heap_is_ptr_from_space(heap, obj)) {
    return obj;
  }
  if (obj->gc_word != 0) {
    return (StellaValue)(obj->gc_word);
  }
  size_t size = rounded_size(object_size(obj->descriptor));
  if (heap->_next + size > heap->_limit) {
    handle_oom(heap);
  }
  StellaValue newObj = memcpy(heap->_to_space + heap->_next, obj, size);
  heap->_next += size;
  obj->gc_word = (uintptr_t)newObj;
  return newObj;
}

StellaValue heap_forward(Heap *heap, StellaValue obj) {
  if (!heap->_collect_active) {
    return obj;
  }
  return perform_forward(heap, obj);
}

static StellaValue alloc_from_space(Heap *heap,
                                    const StellaObjectDescriptor *const descr) {
  size_t size = rounded_size(object_size(descr));
  if (heap->_next + size > heap->_limit) {
    // TODO start collect
    return NULL;
  } else {
    StellaValue res = (StellaValue)(heap->_from_space + heap->_next);
    res->gc_word = 0;
    heap->_next += size;
    return res;
  }
}

static StellaValue alloc_to_space(Heap *heap,
                                  const StellaObjectDescriptor *const descr) {
  size_t size = rounded_size(object_size(descr));
  heap->_limit -= size;
  if (heap->_next >= heap->_limit) {
    handle_oom(heap);
  }
  StellaValue res = (StellaValue)(heap->_from_space + heap->_limit);
  res->gc_word = 0;
  return res;
}

StellaValue heap_alloc(Heap *heap, const StellaObjectDescriptor *const descr) {
  if (heap->_collect_active) {
    return alloc_to_space(heap, descr);
  } else {
    return alloc_from_space(heap, descr);
  }
}

void heap_step_collect(Heap *heap, uint8_t steps_count) {
  if (!heap->_collect_active) {
    return;
  }

  while (steps_count-- > 0 && heap->_scan < heap->_next) {
    StellaValue obj = (StellaValue)(heap->_to_space + heap->_scan);
    // TODO fields
    size_t size = obj->gc_word;
    heap->_scan += size;
  }
  if (heap->_scan == heap->_next) {
    // end collect
    heap->_collect_active = false;
    char *tmp = heap->_to_space;
    heap->_to_space = heap->_from_space;
    heap->_from_space = tmp;
  }
}