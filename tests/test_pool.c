#include "allo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_pool_allocator(void) {
  allo_t child;
  assert(make_c_allocator(&child) == ALLO_OK);
  allo_t a;
  assert(make_pool_allocator(&a, &child, NULL, 64, 10) == ALLO_OK);
  void *p1 = allo_alloc(&a, 64);
  assert(p1 != NULL);

  void *p2 = allo_alloc(&a, 64);
  assert(p2 != NULL);
  assert(p1 != p2);

  allo_free(&a, p1, 64);
  void *p3 = allo_alloc(&a, 64);
  assert(p3 == p1); // Should reuse the block

  allo_destroy(&a);
  printf("Pool allocator test passed\n");
}

void test_pool_exhaustive(void) {
  size_t count = 10;
  allo_t child;
  assert(make_c_allocator(&child) == ALLO_OK);
  allo_t a;
  assert(make_pool_allocator(&a, &child, NULL, 64, count) == ALLO_OK);
  void *pointers[10];

  for (size_t i = 0; i < count; ++i) {
    pointers[i] = allo_alloc(&a, 64);
    assert(pointers[i] != NULL);
  }

  // Exhausted
  assert(allo_alloc(&a, 64) == NULL);

  // Free one and reuse
  allo_free(&a, pointers[5], 64);
  void *p = allo_alloc(&a, 64);
  assert(p == pointers[5]);

  allo_destroy(&a);
  printf("Pool exhaustive test passed\n");
}

void test_pool_validation(void) {
  printf("Testing Pool Allocator: Validation\n");
  allo_t a, child;
  assert(make_c_allocator(&child) == ALLO_OK);

  assert(make_pool_allocator(NULL, &child, NULL, 64, 10) == ALLO_ERR_INVAL);
  // Special case: if buffer is NULL, child MUST NOT be NULL
  assert(make_pool_allocator(&a, NULL, NULL, 64, 10) == ALLO_ERR_INVAL);
  assert(make_pool_allocator(&a, &child, NULL, 0, 10) == ALLO_ERR_INVAL);
  assert(make_pool_allocator(&a, &child, NULL, 64, 0) == ALLO_ERR_INVAL);

  allo_destroy(&child);
  printf("Pool validation tests passed!\n");
}

int main(void) {
  test_pool_validation();
  test_pool_allocator();
  test_pool_exhaustive();
  return 0;
}
