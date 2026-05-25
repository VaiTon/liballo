#include "allo.h"

void *c_alloc_fn(allo_t *self, size_t size) {
  (void)self;
  if (size == 0) {
    return NULL;
  }
  return malloc(size);
}

void *c_realloc_fn(allo_t *self, void *ptr, size_t old_size, size_t new_size) {
  (void)self;
  (void)old_size;
  return realloc(ptr, new_size);
}

void c_free_fn(allo_t *self, void *ptr, size_t size) {
  (void)self;
  (void)size;
  if (ptr == NULL) {
    return;
  }
  free(ptr);
}

// Factory to create a standard C allocator
allo_error_t make_c_allocator(allo_t *out) {
  if (!out)
    return ALLO_ERR_INVAL;
  *out = (allo_t){._alloc = c_alloc_fn,
                  ._realloc = c_realloc_fn,
                  ._free_mem = c_free_fn,
                  ._destroy = NULL};
  return ALLO_OK;
}
