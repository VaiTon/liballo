#include "allo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_page_allocator(void) {
  allo_t a = make_page_allocator();
  void *p1 = allo_alloc(&a, 100);
  assert(p1 != NULL);
  memset(p1, 0xAA, 100);

  void *p2 = allo_alloc(&a, 5000); // Usually > 1 page
  assert(p2 != NULL);
  assert(p2 != p1);
  memset(p2, 0xBB, 5000);

  allo_free(&a, p1);
  allo_free(&a, p2);
  allo_destroy(&a);
  printf("Page allocator test passed\n");
}

int main(void) {
  test_page_allocator();
  return 0;
}
