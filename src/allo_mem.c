#include "allo_mem.h"

#ifdef ALLO_FREESTANDING
  #include "allo_assert.h"

void *allo_memcpy(void *dest, const void *src, size_t n) {
  ALLO_ASSERT(dest != NULL && src != NULL);
  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;
  for (size_t i = 0; i < n; i++) {
    d[i] = s[i];
  }
  return dest;
}

void *allo_memset(void *s, int c, size_t n) {
  ALLO_ASSERT(s != NULL);
  unsigned char *p = (unsigned char *)s;
  while (n--) {
    *p++ = (unsigned char)c;
  }
  return s;
}

/* Some freestanding compilers lower aggregate initialization and structure
 * copies to the conventional libc symbol names even with -ffreestanding.
 * Provide those symbols locally so the static library remains self-contained.
 */
void *memcpy(void *dest, const void *src, size_t n) {
  return allo_memcpy(dest, src, n);
}

void *memset(void *s, int c, size_t n) {
  return allo_memset(s, c, n);
}
#endif
