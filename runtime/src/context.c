#include "internal/context.h"
#include <stella/gc.h>

static GcCtx *CTX = NULL;

void stella_gc_init(const StellaGcConfig *config) {
  // TODO
}

void stella_gc_shutdown(void) {
  // TODO
}

GcCtx *gc_ctx() {
  if (CTX == NULL) {
    stella_abi_violation("Attempt to access heap while GC is not active");
  }
  return CTX;
}
