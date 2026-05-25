#ifndef ALLO_H
#define ALLO_H

#include <stddef.h>

#ifndef ALLO_NOSTDLIB
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
#else
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
#endif

#define ALLO_MAX_ALLOCATOR_CTX_SIZE 128

#define ALLO_ALIGN_UP(size, align)                                             \
  (((size) + ((size_t)(align) - 1)) & ~((size_t)(align) - 1))

#define ALLO_ALIGNED_BUF(name, size) _Alignas(8) char name[(size)]

#define ALLO_IS_ALIGNED(ptr, align)                                            \
  (((size_t)(ptr) & ((size_t)(align) - 1)) == 0)

typedef enum {
  ALLO_OK = 0,
  ALLO_ERR_NOMEM = -1,
  ALLO_ERR_INVAL = -2,
} allo_error_t;

typedef struct allo {
  void *(*_alloc)(struct allo *self, size_t size);
  void *(*_realloc)(struct allo *self, void *ptr, size_t old_size,
                    size_t new_size);
  void (*_free_mem)(struct allo *self, void *ptr);
  void (*_destroy)(struct allo *self);
  _Alignas(8) char _state[ALLO_MAX_ALLOCATOR_CTX_SIZE];
} allo_t;

// Wraps the standard C library malloc/free functions.
allo_error_t make_c_allocator(allo_t *out);

// The most fundamental allocator, directly requesting pages from the OS.
// Each allocation is rounded up to the nearest page size and is page-aligned.
// Extremely slow due to system calls, but useful as a backing for other
// allocators.
allo_error_t make_page_allocator(allo_t *out);

// A simple linear allocator that uses a fixed-size external buffer.
// It cannot free individual allocations; memory is reclaimed when the buffer is
// reset or destroyed.
// Requirements:
//  - `out` must be non-NULL.
//  - `buffer` must be non-NULL and 8-byte aligned.
//  - `size` must be > 0.
// The factory returns ALLO_OK on success and initializes `out`. On failure it
// returns ALLO_ERR_INVAL (invalid arguments) and leaves `out` uninitialized.
allo_error_t make_fixed_buf_allocator(allo_t *out, void *buffer, size_t size);

// A growable region-based allocator that allocates memory in large blocks.
// Like the buffer allocator, it does not support individual frees, but it can
// grow indefinitely. It uses the provided child allocator to manage its blocks.
allo_error_t make_arena_allocator(allo_t *out, allo_t *child,
                                  size_t block_size);

// Efficiently manages a set of fixed-size blocks.
// Supports individual frees by returning blocks to a free list. Ideal for
// frequent allocations of objects of the same size. If buffer is NULL, it
// uses the child allocator to allocate the pool buffer.
allo_error_t make_pool_allocator(allo_t *out, allo_t *child, void *buffer,
                                 size_t block_size, size_t total_blocks);

// A binary buddy allocator that manages a fixed-size buffer.
// It allocates memory in powers of two, splitting larger blocks into "buddies"
// and merging them back together when freed. This significantly reduces
// external fragmentation compared to simple free lists.
// If buffer is NULL, it uses the child allocator to allocate the internal
// memory.
allo_error_t make_buddy_allocator(allo_t *out, allo_t *child, void *buffer,
                                  size_t size);

// Helper functions to make syntax look cleaner
static inline void *allo_alloc(allo_t *a, size_t size) {
  return a->_alloc(a, size);
}

static inline void *allo_calloc(allo_t *a, size_t nmemb, size_t size) {
  if (size > 0 && nmemb > (size_t)-1 / size) {
    return NULL;
  }
  size_t total = nmemb * size;
  void *ptr = allo_alloc(a, total);
  if (ptr) {
    memset(ptr, 0, total);
  }
  return ptr;
}

static inline void allo_free(allo_t *a, void *ptr) {
  a->_free_mem(a, ptr);
}

static inline void *allo_realloc(allo_t *a, void *ptr, size_t old_size,
                                 size_t new_size) {
  if (new_size == 0) {
    allo_free(a, ptr);
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

static inline void allo_destroy(allo_t *a) {
  if (a->_destroy) {
    a->_destroy(a);
  }
}

#endif /* ALLO_H */
