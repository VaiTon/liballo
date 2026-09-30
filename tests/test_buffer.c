#include "allo.h"
#include "test_harness.h"

void test_buffer_validation(void) {
  printf("Testing Fixed Buffer Allocator: Validation\n");
  allo_t a;
  assert(make_fixed_buf_allocator(NULL, NULL, 0) == ALLO_ERR_INVAL);
  assert(make_fixed_buf_allocator(&a, NULL, 0) == ALLO_ERR_INVAL);
  assert(make_fixed_buf_allocator(&a, (void *)1, 1024) ==
         ALLO_ERR_INVAL); // Unaligned

  char buffer[64];
  void *unaligned = (void *)((uintptr_t)buffer + 1);
  assert(make_fixed_buf_allocator(&a, unaligned, 63) == ALLO_ERR_INVAL);

  printf("Fixed Buffer validation tests passed!\n");
}

void test_buffer_allocator(void) {
  printf("Testing Fixed Buffer Allocator: Basic\n");
  ALLO_ALIGNED_BUF(buffer, 1024);
  assert(((uintptr_t)buffer % _Alignof(max_align_t)) == 0);
  allo_t a;
  assert(make_fixed_buf_allocator(&a, buffer, 1024) == ALLO_OK);

  void *p1 = allo_alloc(&a, 100);
  assert(p1 != NULL);
  // Metadata is in the struct now, not in the buffer
  assert(p1 == buffer);

  void *p2 = allo_alloc(&a, 100);
  assert(p2 != NULL);
  // Each allocation starts at max_align_t alignment.
  size_t stride =
      (100 + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1);
  assert(p2 == (char *)p1 + stride);

  void *p3 = allo_alloc(&a, 1000); // Should fail
  assert(p3 == NULL);

  allo_destroy(&a);
  printf("Fixed Buffer basic tests passed!\n");
}


void test_buffer_boundary(void) {
  printf("Testing Fixed Buffer Allocator: Boundary\n");
  char buffer[128];

  size_t offset = (-(uintptr_t)buffer) & (_Alignof(max_align_t) - 1);
  void *aligned = (char *)buffer + offset;

  allo_t a;
  assert(make_fixed_buf_allocator(&a, aligned, 64) == ALLO_OK);

  void *p = allo_alloc(&a, 64);
  assert(p != NULL);
  void *q = allo_alloc(&a, 1);
  assert(q == NULL);
  allo_destroy(&a);
  printf("Fixed Buffer boundary tests passed!\n");
}

void test_buffer_alignment(void) {
  ALLO_ALIGNED_BUF(buffer, 1024);
  allo_t a;
  assert(make_fixed_buf_allocator(&a, buffer, sizeof(buffer)) == ALLO_OK);
  for (size_t size = 1; size <= 32; ++size) {
    void *p = allo_alloc(&a, size);
    assert(p != NULL);
    assert(((uintptr_t)p % _Alignof(max_align_t)) == 0);
  }
  allo_destroy(&a);

  if (_Alignof(max_align_t) > 8) {
    void *misaligned = (char *)buffer + 8;
    assert(make_fixed_buf_allocator(&a, misaligned, 64) == ALLO_ERR_INVAL);
  }
}


int main(void) {
  test_buffer_validation();
  test_buffer_allocator();
  test_buffer_boundary();
  test_buffer_alignment();
  return 0;
}
