#include "allo.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void test_buffer_allocator(void) {
  char buffer[1024];
  // Align buffer manually to be sure the starting point is aligned
  void *aligned_buffer = (void *)(((uintptr_t)buffer + 7) & ~7);
  allo_t a = make_fixed_buf_allocator(aligned_buffer, 1024);

  void *p1 = allo_alloc(&a, 100);
  assert(p1 != NULL);
  // Metadata is in the struct now, not in the buffer
  assert(p1 == aligned_buffer);

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
