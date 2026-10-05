#ifndef STELLA_GC_ROOTS_H
#define STELLA_GC_ROOTS_H

#include "stella/runtime.h"
#include <stdbool.h>

typedef StellaValue *StellaRoot;

typedef struct RootVec {
  size_t size;
  size_t capacity;
  StellaRoot *data;
} RootVec;

void roots_init(RootVec *vec);

void roots_push(RootVec *vec, StellaRoot item);
StellaRoot roots_get(RootVec *vec, size_t idx);
StellaRoot roots_pop(RootVec *vec);

bool roots_contains(RootVec *vec, StellaRoot root);

size_t roots_size(RootVec *vec);
void roots_destroy(RootVec *vec);

#define roots_foreach(Vector, item_var, Action)                                \
  {                                                                            \
    StellaRoot item_var;                                                       \
    size_t index = 0;                                                          \
    for (item_var = roots_get(Vector, index); index < roots_size(Vector);      \
         index++, item_var = roots_get(Vector, index))                         \
      Action                                                                   \
  }

#endif