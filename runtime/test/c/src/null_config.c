#include "internal/errors.h"
#include "util/assert.h"
#include <stella/gc.h>

int main(int argc, char *argv[]) {
  EXPECTED_ERR = ERR_NULL;

  stella_gc_init(NULL);
  return exit_with_assertions();
}