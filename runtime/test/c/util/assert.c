#include "util/assert.h"
#include <stddef.h>
#include <stdio.h>

bool OOM_EXPECTED = false;

const char *EXPECTED_ERR = NULL;

int exit_with_assertions() {
  if (OOM_EXPECTED) {
    fprintf(stderr, "Assertions failed: expected OOM; got none");
    return 1;
  }
  if (EXPECTED_ERR) {
    fprintf(stderr, "Assertions failed: expected error with code %s; got none",
            EXPECTED_ERR);
    return 1;
  }
  return 0;
}