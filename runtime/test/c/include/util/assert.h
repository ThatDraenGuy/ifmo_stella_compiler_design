#ifndef STELLA_GC_TEST_ASSERT_H
#define STELLA_GC_TEST_ASSERT_H

#include <stdbool.h>
#include <stdlib.h>
#include <stella/runtime.h>

extern bool OOM_EXPECTED;
extern const char *EXPECTED_ERR;

int exit_with_assertions();

extern const StellaObjectDescriptor stella_blank_descriptor;

#endif