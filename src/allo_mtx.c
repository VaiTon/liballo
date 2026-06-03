#include "allo.h"

#include <threads.h>

typedef struct {
  allo_t *target;
  mtx_t mutex;
} allo_mtx_ctx_t;

static void *allo_mtx_alloc(allo_t *self, size_t size) {
  allo_mtx_ctx_t *ctx = (allo_mtx_ctx_t *)self->_state;
  mtx_lock(&ctx->mutex);
  void *ptr = allo_alloc(ctx->target, size);
  mtx_unlock(&ctx->mutex);
  return ptr;
}

static void *allo_mtx_realloc(allo_t *self, void *ptr, size_t old_size,
                              size_t new_size) {
  allo_mtx_ctx_t *ctx = (allo_mtx_ctx_t *)self->_state;
  mtx_lock(&ctx->mutex);
  void *new_ptr = allo_realloc(ctx->target, ptr, old_size, new_size);
  mtx_unlock(&ctx->mutex);
  return new_ptr;
}

static void allo_mtx_free(allo_t *self, void *ptr, size_t size) {
  allo_mtx_ctx_t *ctx = (allo_mtx_ctx_t *)self->_state;
  mtx_lock(&ctx->mutex);
  allo_free(ctx->target, ptr, size);
  mtx_unlock(&ctx->mutex);
}

static void allo_mtx_destroy(allo_t *self) {
  allo_mtx_ctx_t *ctx = (allo_mtx_ctx_t *)self->_state;
  mtx_destroy(&ctx->mutex);
}

allo_contains_t allo_mtx_contains(allo_t *self, const void *ptr) {
  allo_mtx_ctx_t *ctx = (allo_mtx_ctx_t *)self->_state;

  mtx_lock(&ctx->mutex);
  allo_contains_t result = allo_contains(ctx->target, ptr);
  mtx_unlock(&ctx->mutex);

  return result;
}

allo_error_t make_mtx_allocator(allo_t *out, allo_t *target) {
  if (!out || !target) {
    return ALLO_ERR_INVAL;
  }

  allo_mtx_ctx_t *ctx = (allo_mtx_ctx_t *)out->_state;
  ctx->target = target;
  if (mtx_init(&ctx->mutex, mtx_plain) != thrd_success) {
    return ALLO_ERR_NOMEM; // Best fit error code
  }

  out->_alloc = allo_mtx_alloc;
  out->_realloc = allo_mtx_realloc;
  out->_free_mem = allo_mtx_free;
  out->_destroy = allo_mtx_destroy;
  out->_contains = allo_mtx_contains;

  return ALLO_OK;
}
