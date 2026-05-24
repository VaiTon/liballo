#include "allo.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

void test_null_and_zero(void) {
  allo_t a;
  assert(make_fixed_buf_allocator(&a, NULL, 0) == ALLO_ERR_INVAL);
}

void test_unaligned_buffer(void) {
  char buffer[64];
  void *unaligned = (void *)((uintptr_t)buffer + 1);
  allo_t a;
  assert(make_fixed_buf_allocator(&a, unaligned, 63) == ALLO_ERR_INVAL);
}

void test_boundary_alloc(void) {
  char buffer[64];
  void *aligned = (void *)(((uintptr_t)buffer + 7) & ~((uintptr_t)7));
  allo_t a;
  assert(make_fixed_buf_allocator(&a, aligned, 64) == ALLO_OK);

  void *p = allo_alloc(&a, 64);
  assert(p != NULL);
  void *q = allo_alloc(&a, 1);
  assert(q == NULL);
  allo_destroy(&a);
}

int main(void) {
  test_null_and_zero();
  test_unaligned_buffer();
  test_boundary_alloc();
  printf("buffer validation tests passed\n");
  return 0;
}
