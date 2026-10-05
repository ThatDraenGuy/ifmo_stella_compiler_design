#include "internal/roots.h"
#include "internal/context.h"
#include <stdlib.h>
#include <stella/gc.h>

#define START_CAPACITY 10
#define GROW_FACTOR 2

void roots_init(RootVec *vec) {
  vec->size = 0;
  vec->capacity = START_CAPACITY;
  vec->data = malloc(sizeof(StellaRoot) * START_CAPACITY);
}

void roots_push(RootVec *vec, StellaRoot item) {
  if (vec->capacity == vec->size) {
    vec->capacity *= GROW_FACTOR;
    vec->data = realloc(vec->data, vec->capacity);
  }
  vec->data[vec->size] = item;
}

StellaRoot roots_get(RootVec *vec, size_t idx) {
  // TODO validation
  return vec->data[idx];
}

StellaRoot roots_pop(RootVec *vec) {
  // TODO validaiton
  return vec->data[--vec->size];
  // TODO shrinking
}

bool roots_contains(RootVec *vec, StellaRoot maybe_root) {
  roots_foreach(vec, root, {
    if (root == maybe_root)
      return true;
  });
  return false;
}

size_t roots_size(RootVec *vec) { return vec->size; }
void roots_destroy(RootVec *vec) { free(vec->data); }

// roots
void stella_gc_push_root(StellaValue *root) {
  // TODO validation
  RootVec *roots = gc_roots();
  roots_push(roots, root);
}

void stella_gc_pop_root(StellaValue *root) {
  // TODO validation
  RootVec *roots = gc_roots();
  StellaRoot popped = roots_pop(roots);
  if (popped != root) {
    stella_abi_violation("TODO");
  }
}

void stella_gc_register_permanent_root(StellaValue *root) {
  // TODO validation
  RootVec *roots = gc_perma_roots();
  roots_push(roots, root);
}