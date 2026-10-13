#ifndef STELLA_GC_UTILS_H
#define STELLA_GC_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stella/runtime.h>

static inline size_t max(size_t l, size_t r) { return l > r ? l : r; }
static inline size_t min(size_t l, size_t r) { return l < r ? l : r; }

static inline bool add_overflows(size_t left, size_t right) {
  return left > SIZE_MAX - right;
}

static inline bool multiply_overflows(size_t left, size_t right) {
  return right != 0 && left > SIZE_MAX / right;
}

static inline size_t object_size(const StellaObjectDescriptor *descriptor) {
  if (multiply_overflows(descriptor->slot_capacity, sizeof(StellaSlot))) {
    stella_abi_violation("object slot capacity overflows size_t");
  }
  size_t slots = descriptor->slot_capacity * sizeof(StellaSlot);
  if (add_overflows(sizeof(StellaObject), slots)) {
    stella_abi_violation("object size overflows size_t");
  }
  return sizeof(StellaObject) + slots;
}

static inline size_t rounded_size(size_t size) {
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

#endif