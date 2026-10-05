#ifndef STELLA_GC_CONFIG_H
#define STELLA_GC_CONFIG_H

#include <stddef.h>
#include <stdint.h>

typedef struct GcConfig {
  size_t min_heap_bytes;
  size_t max_heap_bytes;
  uint8_t collect_steps_count;

  double heap_growth_factor;
  double heap_shrink_threshold;
} GcConfig;

#endif