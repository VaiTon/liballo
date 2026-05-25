#include "allo.h"
#include <assert.h>
#include <string.h>

typedef struct {
  allo_t *primary;
  allo_t *fallback;
} fallback_context_t;

static_assert(sizeof(fallback_context_t) <= ALLO_MAX_ALLOCATOR_CTX_SIZE,
              "Fallback allocator context exceeds maximum size");

static void *fallback_alloc_fn(allo_t *self, size_t size) {
  fallback_context_t *ctx = (fallback_context_t *)self->_state;
  void *ptr = allo_alloc(ctx->primary, size);
  if (!ptr) {
    ptr = allo_alloc(ctx->fallback, size);
  }
  return ptr;
}

static void *fallback_realloc_fn(allo_t *self, void *ptr, size_t old_size,
                                 size_t new_size) {
  if (!ptr) {
    return fallback_alloc_fn(self, new_size);
  }
  fallback_context_t *ctx = (fallback_context_t *)self->_state;

  allo_contains_t primary_contains = allo_contains(ctx->primary, ptr);
  if (primary_contains == ALLO_CONTAINS_YES ||
      primary_contains == ALLO_CONTAINS_UNKNOWN) {
    void *new_ptr = allo_realloc(ctx->primary, ptr, old_size, new_size);
    if (new_ptr) {
      return new_ptr;
    }

    // If it was definitely in primary, we must move it to fallback manually.
    // If it was UNKNOWN, it might have been in primary but realloc failed,
    // so we try fallback as well.
    new_ptr = allo_alloc(ctx->fallback, new_size);
    if (!new_ptr) {
      return NULL;
    }
    size_t copy_size = old_size < new_size ? old_size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    allo_free(ctx->primary, ptr, old_size);
    return new_ptr;
  }

  // Pointer was definitely in fallback
  return allo_realloc(ctx->fallback, ptr, old_size, new_size);
}

static void fallback_free_fn(allo_t *self, void *ptr, size_t size) {
  if (!ptr)
    return;
  fallback_context_t *ctx = (fallback_context_t *)self->_state;
  allo_contains_t primary_contains = allo_contains(ctx->primary, ptr);
  if (primary_contains == ALLO_CONTAINS_YES ||
      primary_contains == ALLO_CONTAINS_UNKNOWN) {
    allo_free(ctx->primary, ptr, size);
  } else {
    allo_free(ctx->fallback, ptr, size);
  }
}

allo_contains_t fallback_contains_fn(allo_t *self, void *ptr) {
  fallback_context_t *ctx = (fallback_context_t *)self->_state;
  allo_contains_t primary = allo_contains(ctx->primary, ptr);
  if (primary == ALLO_CONTAINS_YES)
    return ALLO_CONTAINS_YES;
  allo_contains_t fallback = allo_contains(ctx->fallback, ptr);
  if (fallback == ALLO_CONTAINS_YES)
    return ALLO_CONTAINS_YES;
  if (primary == ALLO_CONTAINS_UNKNOWN || fallback == ALLO_CONTAINS_UNKNOWN)
    return ALLO_CONTAINS_UNKNOWN;
  return ALLO_CONTAINS_NO;
}

allo_error_t make_fallback_allocator(allo_t *out, allo_t *primary,
                                     allo_t *fallback) {
  if (!out || !primary || !fallback) {
    return ALLO_ERR_INVAL;
  }

  *out = (allo_t){._alloc = fallback_alloc_fn,
                  ._realloc = fallback_realloc_fn,
                  ._free_mem = fallback_free_fn,
                  ._destroy = NULL, // Does not own sub-allocators
                  ._contains = fallback_contains_fn};

  fallback_context_t *ctx = (fallback_context_t *)out->_state;
  ctx->primary = primary;
  ctx->fallback = fallback;

  return ALLO_OK;
}
