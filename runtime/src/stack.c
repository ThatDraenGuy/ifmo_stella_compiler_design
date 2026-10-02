#include <stddef.h>

typedef struct item {
} item;

// быстрый и грязный стэк на основе типичного
typedef struct Vector {
  size_t _size;
  size_t _capacity;
  item *_data;
} Vector;

Vector vector_new();
void vector_push(Vector *self, item item);
item vector_pop(Vector *self);
void vector_destroy(Vector *self);