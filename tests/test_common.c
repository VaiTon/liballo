#include "allo.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void test_alignment(void) {
  allo_t child = make_c_allocator();
  allo_t a = make_arena_allocator(&child, 1024);
  void *p1 = allo_alloc(&a, 1);
  void *p2 = allo_alloc(&a, 1);
  assert(((uintptr_t)p1 % 8) == 0);
  assert(((uintptr_t)p2 % 8) == 0);
  assert((char *)p2 >= (char *)p1 + 8);

  allo_destroy(&a);

  char buffer[1024];
  // Align buffer manually to be sure the starting point is aligned
  void *aligned_buffer = (void *)(((uintptr_t)buffer + 7) & ~7);
  a = make_fixed_buf_allocator(aligned_buffer, 1024);
  p1 = allo_alloc(&a, 1);
  p2 = allo_alloc(&a, 1);
  assert(((uintptr_t)p1 % 8) == 0);
  assert(((uintptr_t)p2 % 8) == 0);
  assert((char *)p2 >= (char *)p1 + 8);
  allo_destroy(&a);

  printf("Alignment test passed\n");
}

void test_zero_alloc(void) {
  allo_t c = make_c_allocator();
  void *raw_buf = malloc(100);
  allo_t b = make_fixed_buf_allocator(raw_buf, 100);
  allo_t child = make_c_allocator();
  allo_t a = make_arena_allocator(&child, 1024);
  allo_t p = make_pool_allocator(&child, NULL, 64, 10);

  // Behavior for 0 is now enforced to return NULL.
  assert(allo_alloc(&c, 0) == NULL);
  assert(allo_alloc(&b, 0) == NULL);
  assert(allo_alloc(&a, 0) == NULL);
  assert(allo_alloc(&p, 0) == NULL);

  // free(NULL) should be a safe no-op.
  allo_free(&c, NULL);
  allo_free(&b, NULL);
  allo_free(&a, NULL);
  allo_free(&p, NULL);

  allo_destroy(&c);
  allo_destroy(&b);
  free(raw_buf);
  allo_destroy(&a);
  allo_destroy(&p);
  printf("Zero allocation test passed\n");
}

int main(void) {
  test_alignment();
  test_zero_alloc();
  return 0;
}
