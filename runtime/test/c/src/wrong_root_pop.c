#include "internal/errors.h"
#include "util/assert.h"
#include <stella/gc.h>
#include <stella/scalar.h>

int main(int argc, char *argv[]) {
  EXPECTED_ERR = ERR_ROOT_MISMATCH;

  StellaGcConfig conf = {.abi_version = STELLA_C_GC_ABI_VERSION,
                         .max_heap_bytes = sizeof(StellaObject) - 1};
  stella_gc_init(&conf);
  StellaValue root1 = NULL;
  StellaValue root2 = NULL;
  stella_gc_push_root(&root1);
  stella_gc_pop_root(&root2);
  return exit_with_assertions();
}