#include "internal/config.h"
#include <stdlib.h>
#include <stella/runtime.h>

#define COLLECT_STEPS_COUNT "STELLA_GC_COLLECT_STEPS_COUNT"
#define COLLECT_STEPS_COUNT_DEF 3

#define COLLECT_START_THRESHOLD "STELLA_GC_COLLECT_START_THRESHOLD"
#define COLLECT_START_THRESHOLD_DEF 0.8

#define JSON_STATS "STELLA_GC_STATS_JSON"
#define JSON_STATS_DEF false

void gc_config_init_env(GcConfig *config) {
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
    char *json_stats_str = getenv(JSON_STATS);
    if (json_stats_str == NULL) {
      config->collect_steps_count = JSON_STATS_DEF;
    } else {
      int json_stats = atoi(json_stats_str);
      if (json_stats != 0 && json_stats != 1) {
        stella_abi_violation(JSON_STATS
                             " is set to invalid value; expected a 0 or 1 ");
      }
      config->json_stats = json_stats == 1;
    }
  }
}