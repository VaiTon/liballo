#include "allo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_c_allocator(void) {
  allo_t a = make_c_allocator();
  void *p1 = allo_alloc(&a, 100);
  assert(p1 != NULL);
  memset(p1, 0xAA, 100);
  allo_free(&a, p1);
  allo_destroy(&a);
  printf("C allocator test passed\n");
}

int main(void) {
  test_c_allocator();
  return 0;
}
