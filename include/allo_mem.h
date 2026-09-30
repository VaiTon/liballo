#ifndef ALLO_MEM_H
#define ALLO_MEM_H

#include <stddef.h>
#include <stdbool.h>

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

#define ALLO_ALIGNMENT _Alignof(max_align_t)

inline static bool allo_size_add(size_t a, size_t b, size_t *out) {
  if (a > (size_t)-1 - b) {
    return false;
  }
  *out = a + b;
  return true;
}

inline static bool allo_size_mul(size_t a, size_t b, size_t *out) {
  if (a != 0 && b > (size_t)-1 / a) {
    return false;
  }
  *out = a * b;
  return true;
}

inline static bool allo_align_up_checked(size_t size, size_t align,
                                         size_t *out) {
  if (align == 0 || (align & (align - 1)) != 0 ||
      size > (size_t)-1 - (align - 1)) {
    return false;
  }
  *out = (size + align - 1) & ~(align - 1);
  return true;
}

inline static size_t allo_align_up(size_t size, size_t align) {
  return (size + align - 1) & ~(align - 1);
}

#endif /* ALLO_MEM_H */
