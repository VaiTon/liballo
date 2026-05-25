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

/**
 * The main allocator structure.
 *
 * You MUST NOT create, destroy, or manipulate this structure directly.
 * Instead, use the provided factory functions (e.g., `make_c_allocator`,
 * `make_arena_allocator`) to create instances of allocators, and use the
 * `allo_alloc`, `allo_free`, and `allo_destroy` functions to manage memory and
 * the allocator's lifecycle.
 *
 * The struct is not exposed as an opaque type to allow for stack allocation and
 * embedding in other structures, but its internal state should be considered
 * private and not accessed directly. Always use the provided functions to
 * interact with the allocator.
 *
 */
typedef struct allo {
  void *(*_alloc)(struct allo *self, size_t size);
  void *(*_realloc)(struct allo *self, void *ptr, size_t old_size,
                    size_t new_size);
  void (*_free_mem)(struct allo *self, void *ptr, size_t size);
  void (*_destroy)(struct allo *self);
  _Alignas(8) char _state[ALLO_MAX_ALLOCATOR_CTX_SIZE];
} allo_t;

/*
 * Wraps the standard C library malloc/free functions.
 */
allo_error_t make_c_allocator(allo_t *out);

/*
 * The most fundamental allocator, directly requesting pages from the OS.
 *
 * Each allocation is rounded up to the nearest page size and is page-aligned.
 * Extremely slow due to system calls, but useful as a backing for other
 * allocators.
 */
allo_error_t make_page_allocator(allo_t *out);

/*
 * A simple linear allocator that uses a fixed-size external buffer.
 *
 * It cannot free individual allocations; memory is reclaimed when the buffer is
 * reset or destroyed.
 *
 * `out` must be 8-byte aligned. Use the ALLO_ALIGNED_BUF macro to create a
 * suitable buffer.
 */
allo_error_t make_fixed_buf_allocator(allo_t *out, void *buffer, size_t size);

/*
 * A growable region-based allocator that allocates memory in large blocks.
 *
 * Like the buffer allocator, it does not support individual frees, but it can
 * grow indefinitely.
 *
 * It uses the provided child allocator to manage its blocks.
 */
allo_error_t make_arena_allocator(allo_t *out, allo_t *child,
                                  size_t block_size);

/*
 * Efficiently manages a set of fixed-size blocks.
 *
 * Supports individual frees by returning blocks to a free list. Ideal for
 * frequent allocations of objects of the same size.
 *
 * If buffer is NULL, it uses the child allocator to allocate the pool buffer.
 */
allo_error_t make_pool_allocator(allo_t *out, allo_t *child, void *buffer,
                                 size_t block_size, size_t total_blocks);

/*
 * A binary buddy allocator that manages a fixed-size buffer.
 *
 * It allocates memory in powers of two, splitting larger blocks into "buddies"
 * and merging them back together when freed. This significantly reduces
 * external fragmentation compared to simple free lists.
 *
 * If buffer is NULL, it uses the child allocator to allocate the internal
 * memory.
 */
allo_error_t make_buddy_allocator(allo_t *out, allo_t *child, void *buffer,
                                  size_t size);

/**
 * Allocates memory using the provided allocator.
 * If the allocation fails, it returns NULL.
 */
static inline void *allo_alloc(allo_t *a, size_t size) {
  return a->_alloc(a, size);
}

/**
 * Frees memory allocated by the allocator. The `size` parameter must match the
 * size used during allocation. Passing an incorrect size is Undefined Behavior.
 */
static inline void allo_free(allo_t *a, void *ptr, size_t size) {
  a->_free_mem(a, ptr, size);
}

/**
 * Destroys the allocator, releasing any resources it holds. After this call,
 * using the allocator or any memory allocated by it is Undefined Behavior.
 *
 * Note: Allocators that use a child allocator do NOT destroy the child
 * allocator when they are destroyed.
 */
static inline void allo_destroy(allo_t *a) {
  if (a->_destroy) {
    a->_destroy(a);
  }
}

/**
 * Allocates memory for an array of `nmemb` elements of `size` bytes each, and
 * initializes all bytes to zero. If the allocation fails, it returns NULL.
 */
void *allo_calloc(allo_t *a, size_t nmemb, size_t size);

/**
 * Reallocates memory previously allocated by the allocator.
 *
 * The `old_size` must match the size used during the original allocation,
 * otherwise the behavior is undefined.
 *
 * If the reallocation fails, it returns NULL, and the original memory block
 * remains unchanged.
 */
void *allo_realloc(allo_t *a, void *ptr, size_t old_size, size_t new_size);

#endif /* ALLO_H */
