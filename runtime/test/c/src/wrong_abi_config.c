#include "internal/errors.h"
#include "util/assert.h"
#include <stella/gc.h>

int main(int argc, char *argv[]) {
  EXPECTED_ERR = ERR_ABI_VER;

  StellaGcConfig conf = {.abi_version = 67, .max_heap_bytes = 1024};
  stella_gc_init(&conf);
  return exit_with_assertions();
}