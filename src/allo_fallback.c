#include "allo.h"
#include "allo_assert.h"
#include "allo_mem.h"

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
  } else if (allo_contains(ctx->primary, ptr) != ALLO_CONTAINS_YES) {
    /* The pointer cannot be safely routed later if primary does not
     * positively identify it.  Do not expose an unfreeable allocation. */
    allo_free(ctx->primary, ptr, size);
    return NULL;
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
  if (primary_contains == ALLO_CONTAINS_YES) {
    void *new_ptr = allo_realloc(ctx->primary, ptr, old_size, new_size);
    if (new_ptr) {
      return new_ptr;
    }

    // If it was definitely in primary, move it to fallback manually when the
    // primary allocator cannot satisfy the resize.
    new_ptr = allo_alloc(ctx->fallback, new_size);
    if (!new_ptr) {
      return NULL;
    }
    size_t copy_size = old_size < new_size ? old_size : new_size;
    allo_memcpy(new_ptr, ptr, copy_size);
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
  if (primary_contains == ALLO_CONTAINS_YES) {
    allo_free(ctx->primary, ptr, size);
  } else if (primary_contains == ALLO_CONTAINS_NO) {
    allo_free(ctx->fallback, ptr, size);
  }
}

allo_contains_t fallback_contains_fn(allo_t *self, const void *ptr) {
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
  /* A fallback must be able to identify every pointer served by primary.
   * Without this guarantee, a pointer that spilled to fallback could later be
   * passed to primary's free/realloc implementation. */
  if (!out || !primary || !fallback || !primary->_contains) {
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
