#include "allo.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void test_buffer_allocator(void) {
  ALLO_ALIGNED_BUF(buffer, 1024);
  allo_t a;
  assert(make_fixed_buf_allocator(&a, buffer, 1024) == ALLO_OK);

  void *p1 = allo_alloc(&a, 100);
  assert(p1 != NULL);
  // Metadata is in the struct now, not in the buffer
  assert(p1 == buffer);

  void *p2 = allo_alloc(&a, 100);
  assert(p2 != NULL);
  // 100 aligned up to 8 is 104
  assert(p2 == (char *)p1 + 104);

  void *p3 = allo_alloc(&a, 1000); // Should fail
  assert(p3 == NULL);

  allo_destroy(&a);
  printf("Fixed Buffer allocator test passed\n");
}

int main(void) {
  test_buffer_allocator();
  return 0;
}
