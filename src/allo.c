#include "allo.h"
#include "allo_mem.h"

void *allo_calloc(allo_t *a, size_t nmemb, size_t size) {
  if (size > 0 && nmemb > (size_t)-1 / size) {
    return NULL;
  }
  size_t total = nmemb * size;
  void *ptr = allo_alloc(a, total);
  if (ptr) {
    allo_memset(ptr, 0, total);
  }
  return ptr;
}

void *allo_realloc(allo_t *a, void *ptr, size_t old_size, size_t new_size) {
  if (new_size == 0) {
    allo_free(a, ptr, old_size);
    return NULL;
  }
  if (ptr == NULL) {
    return allo_alloc(a, new_size);
  }
  if (old_size == new_size) {
    return ptr;
  }
  return a->_realloc(a, ptr, old_size, new_size);
}
