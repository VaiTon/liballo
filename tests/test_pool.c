#include "allo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_pool_allocator(void) {
  allo_t child = make_c_allocator();
  allo_t a = make_pool_allocator(&child, NULL, 64, 10);
  void *p1 = allo_alloc(&a, 64);
  assert(p1 != NULL);

  void *p2 = allo_alloc(&a, 64);
  assert(p2 != NULL);
  assert(p1 != p2);

  allo_free(&a, p1);
  void *p3 = allo_alloc(&a, 64);
  assert(p3 == p1); // Should reuse the block

  allo_destroy(&a);
  printf("Pool allocator test passed\n");
}

void test_pool_exhaustive(void) {
  size_t count = 10;
  allo_t child = make_c_allocator();
  allo_t a = make_pool_allocator(&child, NULL, 64, count);
  void *pointers[10];

  for (size_t i = 0; i < count; ++i) {
    pointers[i] = allo_alloc(&a, 64);
    assert(pointers[i] != NULL);
  }

  // Exhausted
  assert(allo_alloc(&a, 64) == NULL);

  // Free one and reuse
  allo_free(&a, pointers[5]);
  void *p = allo_alloc(&a, 64);
  assert(p == pointers[5]);

  allo_destroy(&a);
  printf("Pool exhaustive test passed\n");
}

int main(void) {
  test_pool_allocator();
  test_pool_exhaustive();
  return 0;
}
