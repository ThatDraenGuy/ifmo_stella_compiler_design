#include "internal/errors.h"
#include "util/assert.h"
#include <stella/gc.h>
#include <stella/scalar.h>

int main(int argc, char *argv[]) {
  OOM_EXPECTED = true;

  StellaGcConfig conf = {.abi_version = STELLA_C_GC_ABI_VERSION,
                         .max_heap_bytes = sizeof(StellaObject) - 1};
  stella_gc_init(&conf);
  StellaValue root = NULL;
  stella_gc_push_root(&root);
  stella_gc_alloc(&stella_blank_descriptor);
  return exit_with_assertions();
}