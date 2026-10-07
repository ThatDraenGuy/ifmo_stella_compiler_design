#include "internal/context.h"
#include <stella/gc.h>

static bool add_overflows(size_t left, size_t right) {
  return left > SIZE_MAX - right;
}

static void validate_descriptor(const StellaObjectDescriptor *descriptor) {
  if (descriptor == NULL) {
    stella_abi_violation("null object descriptor");
  }
  if (descriptor->abi_version != STELLA_C_GC_ABI_VERSION) {
    stella_abi_violation("incompatible descriptor ABI version");
  }
  if (descriptor->kind < STELLA_OBJECT_DATA ||
      descriptor->kind > STELLA_OBJECT_INDIRECTION) {
    stella_abi_violation("unknown object kind");
  }
  uint32_t known_flags = STELLA_DESCRIPTOR_STATIC | STELLA_DESCRIPTOR_UPDATABLE;
  if ((descriptor->flags & ~known_flags) != 0) {
    stella_abi_violation("unknown descriptor flag");
  }
  if ((descriptor->flags & STELLA_DESCRIPTOR_STATIC) != 0 &&
      (descriptor->flags & STELLA_DESCRIPTOR_UPDATABLE) != 0) {
    stella_abi_violation("static descriptor is marked updatable");
  }
  if (add_overflows(descriptor->managed_count, descriptor->primitive_count)) {
    stella_abi_violation("active field count overflows size_t");
  }
  if (descriptor->managed_count + descriptor->primitive_count >
      descriptor->slot_capacity) {
    stella_abi_violation("active fields exceed slot capacity");
  }
  if ((descriptor->kind == STELLA_OBJECT_CLOSURE ||
       descriptor->kind == STELLA_OBJECT_THUNK) &&
      descriptor->entry_code == NULL) {
    stella_abi_violation("executable descriptor has no entry code");
  }
  if ((descriptor->kind == STELLA_OBJECT_DATA ||
       descriptor->kind == STELLA_OBJECT_INDIRECTION) &&
      descriptor->entry_code != NULL) {
    stella_abi_violation("non-executable descriptor has entry code");
  }
  if (descriptor->kind == STELLA_OBJECT_THUNK && descriptor->entry_arity != 0) {
    stella_abi_violation("thunk descriptor has non-zero entry arity");
  }
  if ((descriptor->kind == STELLA_OBJECT_DATA ||
       descriptor->kind == STELLA_OBJECT_INDIRECTION) &&
      descriptor->entry_arity != 0) {
    stella_abi_violation("non-executable descriptor has non-zero entry arity");
  }
}

static void validate_object(StellaValue obj) {
  if (obj == NULL) {
    stella_abi_violation("null object");
  }
  validate_descriptor(obj->descriptor);
  if ((obj->descriptor->flags & STELLA_DESCRIPTOR_STATIC) == 0 &&
      !heap_owns_obj(gc_heap(), obj)) {
    stella_abi_violation("heap object is not owned by the collector");
  }
}

static void validate_heap_object(StellaValue obj) {
  if (obj == NULL) {
    stella_abi_violation("null object");
  }
  if (!heap_owns_obj(gc_heap(), obj)) {
    stella_abi_violation("object is not owned by the collector");
  }
  validate_descriptor(obj->descriptor);
}

static void validate_root(StellaRoot root) {
  if (root == NULL || (!roots_contains(gc_roots(), root) &&
                       !roots_contains(gc_perma_roots(), root))) {
    stella_abi_violation("Attempt to access unregistered root");
  }
}

StellaValue stella_gc_alloc(const StellaObjectDescriptor *descriptor) {
  validate_descriptor(descriptor);
  if ((descriptor->flags & STELLA_DESCRIPTOR_STATIC) != 0) {
    stella_abi_violation("heap allocation requested with static descriptor");
  }

  Heap *heap = gc_heap();
  heap_step_collect(heap, gc_config()->collect_steps_count);
  StellaValue res = heap_alloc(heap, descriptor);
  gc_stats()->allocs++;
  return res;
}

void stella_object_init_managed(StellaValue object, size_t index,
                                StellaValue value) {
  validate_heap_object(object);
  if (index >= object->descriptor->managed_count) {
    stella_abi_violation("managed initialisation index out of range");
  }
  object->fields[index].managed = value;
}

void stella_object_init_primitive(StellaValue object, size_t index,
                                  StellaPrimitive value) {
  validate_heap_object(object);
  if (index >= object->descriptor->primitive_count) {
    stella_abi_violation("primitive initialisation index out of range");
  }
  size_t physical = object->descriptor->managed_count + index;
  object->fields[physical].primitive = value;
}

StellaValue stella_gc_read_managed(StellaValue *object_root, size_t index) {
  validate_root(object_root);
  validate_object(*object_root);
  if (index >= (*object_root)->descriptor->managed_count) {
    stella_abi_violation("managed read index out of range");
  }
  StellaValue field = ((*object_root)->fields[index].managed);
  StellaValue forwarded = heap_forward(gc_heap(), field);
  (*object_root)->fields[index].managed = forwarded;
  gc_stats()->managed_reads++;
  gc_stats()->read_barrier_activations++;
  return forwarded;
}

void stella_gc_write_managed(StellaValue *object_root, size_t index,
                             StellaValue *value_root) {
  validate_root(object_root);
  validate_root(value_root);
  validate_object(*object_root);
  if (((*object_root)->descriptor->flags & STELLA_DESCRIPTOR_STATIC) != 0) {
    stella_abi_violation("managed write to static object");
  }
  if (index >= (*object_root)->descriptor->managed_count) {
    stella_abi_violation("managed write index out of range");
  }
  (*object_root)->fields[index].managed = *value_root;
  gc_stats()->managed_writes++;
}

StellaPrimitive stella_object_read_primitive(StellaValue object, size_t index) {
  validate_object(object);
  if (index >= object->descriptor->primitive_count) {
    stella_abi_violation("primitive read index out of range");
  }
  StellaPrimitive res =
      object->fields[object->descriptor->managed_count + index].primitive;
  gc_stats()->primitive_reads++;
  return res;
}

void stella_object_write_primitive(StellaValue object, size_t index,
                                   StellaPrimitive value) {
  validate_object(object);
  if ((object->descriptor->flags & STELLA_DESCRIPTOR_STATIC) != 0) {
    stella_abi_violation("primitive write to static object");
  }
  if (index >= object->descriptor->primitive_count) {
    stella_abi_violation("primitive write index out of range");
  }
  object->fields[object->descriptor->managed_count + index].primitive = value;
  gc_stats()->primitive_writes++;
}

void stella_gc_update_object(StellaValue *object_root,
                             const StellaObjectDescriptor *new_descriptor,
                             StellaValue *const new_managed_roots[],
                             const StellaPrimitive new_primitives[]) {
  validate_root(object_root);
  validate_heap_object(*object_root);
  const StellaObjectDescriptor *old_descriptor = (*object_root)->descriptor;
  validate_descriptor(new_descriptor);
  if ((old_descriptor->flags & STELLA_DESCRIPTOR_UPDATABLE) == 0 ||
      (new_descriptor->flags & STELLA_DESCRIPTOR_STATIC) != 0) {
    stella_abi_violation("incompatible object-layout update flags");
  }
  if (old_descriptor->slot_capacity != new_descriptor->slot_capacity) {
    stella_abi_violation("object-layout update changes slot capacity");
  }
  if (new_descriptor->managed_count != 0 && new_managed_roots == NULL) {
    stella_abi_violation("missing managed values for object-layout update");
  }
  if (new_descriptor->primitive_count != 0 && new_primitives == NULL) {
    stella_abi_violation("missing primitive values for object-layout update");
  }
  for (size_t i = 0; i < new_descriptor->managed_count; ++i) {
    validate_root(new_managed_roots[i]);
  }

  for (size_t i = 0; i < new_descriptor->managed_count; ++i) {
    (*object_root)->fields[i].managed = *new_managed_roots[i];
    gc_stats()->managed_writes++;
  }
  for (size_t i = 0; i < new_descriptor->primitive_count; ++i) {
    size_t physical = new_descriptor->managed_count + i;
    (*object_root)->fields[physical].primitive = new_primitives[i];
    gc_stats()->primitive_writes++;
  }
  size_t active =
      new_descriptor->managed_count + new_descriptor->primitive_count;
  for (size_t i = active; i < new_descriptor->slot_capacity; ++i) {
    (*object_root)->fields[i].primitive = 0;
  }
  (*object_root)->descriptor = new_descriptor;
}
