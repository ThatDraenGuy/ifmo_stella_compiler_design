#include "internal/context.h"
#include "internal/roots.h"
#include <internal/stats.h>
#include <stella/gc.h>

void stats_init(GcStats *stats) {
  stats->allocs = 0;
  stats->collection_steps = 0;
  stats->dynamic_root_depth = 0;
  stats->failed_allocations = 0;
  stats->managed_reads = 0;
  stats->managed_writes = 0;
  stats->maximum_dynamic_root_depth = 0;
  stats->maximum_occupied_bytes = 0;
  stats->occupied_bytes = 0;
  stats->permanent_root_count = 0;
  stats->primitive_reads = 0;
  stats->primitive_writes = 0;
  stats->read_barrier_activations = 0;
  stats->requested_bytes = 0;
  stats->rounded_bytes = 0;

  stats->heap_growths = 0;
  stats->heap_shrinks = 0;
  stats->heap_size = 0;
}

void stella_gc_print_statistics(FILE *output) {
  GcStats *stats = gc_stats();
  if (output == NULL) {
    stella_abi_violation("null statistics output stream");
  }

  fprintf(output, "GC stats\n");
  fprintf(output, "requested bytes: %zu\n", stats->requested_bytes);
  fprintf(output, "requested bytes (with rounding): %zu\n",
          stats->rounded_bytes);
  fprintf(output, "alloc calls: %zu\n", stats->allocs);
  fprintf(output, "collection steps: %zu\n", stats->collection_steps);
  fprintf(output, "currently occupied bytes: %zu\n", stats->occupied_bytes);
  fprintf(output, "maximum occupied bytes: %zu\n",
          stats->maximum_occupied_bytes);
  fprintf(output, "managed reads count: %zu\n", stats->managed_reads);
  fprintf(output, "managed writes count: %zu\n", stats->managed_writes);
  fprintf(output, "primitive reads count: %zu\n", stats->primitive_reads);
  fprintf(output, "primitive writes count: %zu\n", stats->primitive_writes);
  fprintf(output, "read barrier activations: %zu\n",
          stats->read_barrier_activations);
  fprintf(output, "current dynamic roots count: %zu\n",
          stats->dynamic_root_depth);
  fprintf(output, "maximum dynamic roots count: %zu\n",
          stats->maximum_dynamic_root_depth);
  fprintf(output, "permanent roots count: %zu\n", stats->permanent_root_count);
  fprintf(output, "failed allocations: %zu\n", stats->failed_allocations);
  fprintf(output, "heap growth count: %zu\n", stats->heap_growths);
  fprintf(output, "heap shrink count: %zu\n", stats->heap_shrinks);
  fprintf(output, "current heap size: %zu\n", stats->heap_size);
}

void stella_gc_print_roots(FILE *output) {
  RootVec *roots = gc_roots();
  RootVec *perma_roots = gc_perma_roots();
  if (output == NULL) {
    stella_abi_violation("null statistics output stream");
  }

  fprintf(output, "GC roots:\nPermanent:\n");
  roots_foreach(perma_roots, root, {
    fprintf(output, "root #%zu: addr %p, value %p\n", index, root, *root);
  });

  fprintf(output, "Dynamic:\n");
  roots_foreach(roots, root, {
    fprintf(output, "root #%zu: addr %p, value %p", index, root, *root);
  });
}

void stella_gc_print_state(FILE *output) {
  if (output == NULL) {
    stella_abi_violation("null statistics output stream");
  }

  Heap *heap = gc_heap();
  fprintf(output, "GC state\n");
  fprintf(output, "Collect is %s\n",
          heap->collect_active ? "ACTIVE" : "NOT ACTIVE");
  fprintf(output, "scan: %zu\n", heap->scan);
  fprintf(output, "next: %zu\n", heap->next);
  fprintf(output, "lim:  %zu\n", heap->limit);
  fprintf(output, "end:  %zu\n", heap->end);
}
