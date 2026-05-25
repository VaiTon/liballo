#ifndef ALLO_MEM_H
#define ALLO_MEM_H

#include <stddef.h>

#ifndef ALLO_NOSTDLIB
  #include <string.h>
  #define allo_memcpy memcpy
  #define allo_memset memset
#else
void *allo_memcpy(void *dest, const void *src, size_t n);
void *allo_memset(void *s, int c, size_t n);
#endif

#endif /* ALLO_MEM_H */
