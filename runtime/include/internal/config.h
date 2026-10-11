#ifndef STELLA_GC_CONFIG_H
#define STELLA_GC_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct GcConfig {
  size_t max_heap_bytes;
  uint8_t collect_steps_count;
  double collect_start_threshold;
  bool json_stats;
} GcConfig;

void gc_config_init_env(GcConfig *config);

#endif