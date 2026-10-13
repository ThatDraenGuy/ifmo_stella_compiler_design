#include "internal/errors.h"
#include "util/assert.h"
#include <stella/gc.h>

int main(int argc, char *argv[]) {
  expected_err = ERR_NULL;
  stella_gc_init(NULL);
  return 0;
}