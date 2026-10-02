#include "internal/context.h"
#include <stella/gc.h>

StellaValue stella_gc_alloc(const StellaObjectDescriptor *descriptor) {
  // TODO validations
  GcCtx *ctx = gc_ctx();
  return heap_alloc(gc_heap(ctx), descriptor);
}

void stella_object_init_managed(StellaValue object, size_t index,
                                StellaValue value);

void stella_object_init_primitive(StellaValue object, size_t index,
                                  StellaPrimitive value);

StellaValue stella_gc_read_managed(StellaValue *object_root, size_t index) {
  // TODO validations
  GcCtx *ctx = gc_ctx();
  StellaValue field = ((*object_root)->fields[index].managed);
  return heap_forward(gc_heap(ctx), field);
}

void stella_gc_write_managed(StellaValue *object_root, size_t index,
                             StellaValue *value_root);

StellaPrimitive stella_object_read_primitive(StellaValue object, size_t index);

void stella_object_write_primitive(StellaValue object, size_t index,
                                   StellaPrimitive value);

void stella_gc_update_object(StellaValue *object_root,
                             const StellaObjectDescriptor *new_descriptor,
                             StellaValue *const new_managed_roots[],
                             const StellaPrimitive new_primitives[]);
