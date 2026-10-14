#include "util/assert.h"
#include <stdbool.h>
#include <stella/runtime.h>
#include <string.h>

#include <stdlib.h>

_Noreturn void stella_gc_out_of_memory(size_t requested_bytes) {
  if (OOM_EXPECTED) {
    exit(EXIT_SUCCESS);
  }
  fprintf(stderr, "Assertions failed: unexpected OOM with %zu bytes\n",
          requested_bytes);
  exit(EXIT_FAILURE);
}

static bool starts_with(const char *const check, const char *const str) {
  size_t check_len = strlen(check);
  size_t str_len = strlen(str);
  if (str_len < check_len) {
    return false;
  }

  for (size_t i = 0; i < check_len; i++) {
    if (check[i] != str[i]) {
      return false;
    }
  }
  return true;
}

_Noreturn void stella_abi_violation(const char *message) {
  if (EXPECTED_ERR == NULL) {
    fprintf(stderr,
            "Assertions failed: unexpected ABI violation with msg: %s\n",
            message);
    exit(EXIT_FAILURE);
  } else if (!starts_with(EXPECTED_ERR, message)) {
    fprintf(stderr,
            "Assertions failed: wrong ABI violation error code. Expected: %s; "
            "actual msg: %s\n",
            EXPECTED_ERR, message);
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
