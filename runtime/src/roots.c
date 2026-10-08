#include "internal/roots.h"
#include "internal/context.h"
#include "internal/stats.h"
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
    vec->data = realloc(vec->data, sizeof(StellaRoot) * vec->capacity);
  }
  vec->data[vec->size++] = item;
}

StellaRoot roots_get(RootVec *vec, size_t idx) {
  if (idx < 0 || idx >= vec->size) {
    stella_abi_violation("index out of vector range");
  }
  return vec->data[idx];
}

StellaRoot roots_pop(RootVec *vec) {
  if (vec->size <= 0) {
    stella_abi_violation("empty vector pop");
  }
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
  RootVec *roots = gc_roots();

  if (root == NULL) {
    stella_abi_violation("null root slot");
  }
  if (roots_contains(roots, root) || roots_contains(gc_perma_roots(), root)) {
    stella_abi_violation("duplicate root registration");
  }
  roots_push(roots, root);

  gc_stats()->dynamic_root_depth = roots_size(roots);
  if (roots_size(roots) > gc_stats()->maximum_dynamic_root_depth) {
    gc_stats()->maximum_dynamic_root_depth = roots_size(roots);
  }
}

void stella_gc_pop_root(StellaValue *root) {
  RootVec *roots = gc_roots();
  StellaRoot popped = roots_pop(roots);
  if (popped != root) {
    stella_abi_violation("Popped root does not match expected root");
  }
  gc_stats()->dynamic_root_depth = roots_size(roots);
}

void stella_gc_register_permanent_root(StellaValue *root) {
  RootVec *roots = gc_perma_roots();

  if (root == NULL) {
    stella_abi_violation("null root slot");
  }
  if (roots_contains(roots, root) || roots_contains(gc_roots(), root)) {
    stella_abi_violation("duplicate root registration");
  }
  roots_push(roots, root);
  gc_stats()->permanent_root_count = roots_size(roots);
}