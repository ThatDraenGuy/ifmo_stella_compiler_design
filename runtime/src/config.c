#include "internal/config.h"
#include "internal/utils.h"
#include <stdlib.h>
#include <stella/runtime.h>

#define MIN_HEAP_BYTES "STELLA_GC_MIN_HEAP_BYTES"

#define COLLECT_STEPS_COUNT "STELLA_GC_COLLECT_STEPS_COUNT"
#define COLLECT_STEPS_COUNT_DEF 3

#define COLLECT_START_THRESHOLD "STELLA_GC_COLLECT_START_THRESHOLD"
#define COLLECT_START_THRESHOLD_DEF 1.0

#define HEAP_GROWTH_FACTOR "STELLA_GC_HEAP_GROWTH_FACTOR"
#define HEAP_GROWTH_FACTOR_DEF 2.0

#define HEAP_SHRINK_THRESHOLD "STELLA_GC_HEAP_SHRINK_THRESHOLD"
#define HEAP_SHRINK_THRESHOLD_DEF 0.25

void gc_config_init_env(GcConfig *config) {
  {
    char *min_heap_bytes_str = getenv(MIN_HEAP_BYTES);
    if (min_heap_bytes_str == NULL) {
      config->min_heap_bytes = config->max_heap_bytes;
    } else {
      long long min_heap_bytes = atoll(min_heap_bytes_str);
      if (min_heap_bytes <= 0) {
        stella_abi_violation(MIN_HEAP_BYTES
                             " is set to invalid value; expected a positive "
                             "integer number of bytes");
      }
      config->min_heap_bytes = min_heap_bytes;
    }
  }

  {
    char *collect_steps_count_str = getenv(COLLECT_STEPS_COUNT);
    if (collect_steps_count_str == NULL) {
      config->collect_steps_count = COLLECT_STEPS_COUNT_DEF;
    } else {
      int collect_steps_count = atoi(collect_steps_count_str);
      if (collect_steps_count <= 0) {
        stella_abi_violation(COLLECT_STEPS_COUNT
                             " is set to invalid value; expected a positive "
                             "integer number of bytes");
      }
      config->collect_steps_count = collect_steps_count;
    }
  }

  {
    char *collect_start_threshold_str = getenv(COLLECT_START_THRESHOLD);
    if (collect_start_threshold_str == NULL) {
      config->collect_start_threshold = COLLECT_START_THRESHOLD_DEF;
    } else {
      double collect_start_threshold = atof(collect_start_threshold_str);
      if (collect_start_threshold <= 0 || collect_start_threshold > 1.0) {
        stella_abi_violation(COLLECT_START_THRESHOLD
                             " is set to invalid value; expected a floating "
                             "number between 0 and 1");
      }
      config->collect_start_threshold = collect_start_threshold;
    }
  }

  {
    char *heap_growth_factor_str = getenv(HEAP_GROWTH_FACTOR);
    if (heap_growth_factor_str == NULL) {
      config->heap_growth_factor = HEAP_GROWTH_FACTOR_DEF;
    } else {
      double heap_growth_factor = atof(heap_growth_factor_str);
      if (heap_growth_factor <= 1.0) {
        stella_abi_violation(COLLECT_START_THRESHOLD
                             " is set to invalid value; expected a floating "
                             "number bigger than 1");
      }
      config->collect_start_threshold = heap_growth_factor;
    }
  }

  {
    char *heap_shrink_threshold_str = getenv(HEAP_SHRINK_THRESHOLD);
    if (heap_shrink_threshold_str == NULL) {
      config->heap_shrink_threshold = HEAP_SHRINK_THRESHOLD_DEF;
    } else {
      double heap_shrink_threshold = atof(heap_shrink_threshold_str);
      if (heap_shrink_threshold >= 1.0 ||
          heap_shrink_threshold >= 1.0 / config->heap_growth_factor) {
        stella_abi_violation(
            COLLECT_START_THRESHOLD
            " is set to invalid value; expected a floating "
            "number smaller than 1 and `1 / heap growth factor`");
      }
      config->collect_start_threshold = heap_shrink_threshold;
    }
  }
}