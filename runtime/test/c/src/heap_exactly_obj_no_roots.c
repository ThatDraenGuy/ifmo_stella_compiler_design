#include "internal/errors.h"
#include "internal/utils.h"
#include "util/assert.h"
#include <stella/gc.h>
#include <stella/scalar.h>

int main(int argc, char *argv[]) {
  size_t obj_size = rounded_size(object_size(&stella_blank_descriptor));
  StellaGcConfig conf = {.abi_version = STELLA_C_GC_ABI_VERSION,
                         .max_heap_bytes = obj_size};
  stella_gc_init(&conf);
  stella_gc_alloc(&stella_blank_descriptor);
  stella_gc_alloc(&stella_blank_descriptor);
  stella_gc_alloc(&stella_blank_descriptor);
  stella_gc_alloc(&stella_blank_descriptor);
  return exit_with_assertions();
}