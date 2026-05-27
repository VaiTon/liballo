#ifndef ALLO_MEM_H
#define ALLO_MEM_H

#include <stddef.h>

#ifndef ALLO_FREESTANDING
  #include "allo_assert.h"
  #include <string.h>

// Use standard library functions
inline static void *allo_memcpy(void *dst, const void *src, size_t n) {
  ALLO_ASSERT(dst != NULL && src != NULL);
  ALLO_ASSERT((char *)dst + n <= (char *)src || (char *)src + n <= (char *)dst);
  return memcpy(dst, src, n);
}
inline static void *allo_memset(void *s, int c, size_t n) {
  ALLO_ASSERT(s != NULL);
  return memset(s, c, n);
}

#else
void *allo_memcpy(void *dest, const void *src, size_t n);
void *allo_memset(void *s, int c, size_t n);
#endif

inline static size_t allo_align_up(size_t size, size_t align) {
  return (size + align - 1) & ~(align - 1);
}

#endif /* ALLO_MEM_H */
