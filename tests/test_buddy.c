#include "allo.h"
#include "test_harness.h"
#include <stdint.h>

void test_buddy_basic(void) {
  printf("Testing Buddy Allocator: Basic\n");
  allo_t c_alloc, buddy;
  static ALLO_ALIGNED_BUF(buffer, (1024 * 1024UL) + (128 * 1024UL));
  assert(make_fixed_buf_allocator(&c_alloc, buffer, sizeof(buffer)) == ALLO_OK);
  assert(make_buddy_allocator(&buddy, &c_alloc, NULL, 1024 * 1024UL) ==
         ALLO_OK);

  // Should get 128 (min is 32, but we use 128 here or whatever)
  void *p1 = allo_alloc(&buddy, 100);
  assert(p1 != NULL);
  memset(p1, 0xAA, 100);

  void *p2 = allo_alloc(&buddy, 200); // Should get 256
  assert(p2 != NULL);
  memset(p2, 0xBB, 200);

  void *p3 = allo_alloc(&buddy, 512 * 1024UL); // 512KB
  assert(p3 != NULL);

  allo_free(&buddy, p1, 100);
  allo_free(&buddy, p2, 200);

  // p1 and p2 are buddies? No, 128 and 256.
  // But they should eventually coalesce if we free everything.

  allo_free(&buddy, p3, 512 * 1024UL);

  // Try a large one after coalescing
  void *p4 = allo_alloc(&buddy, 1024 * 1024UL);
  assert(p4 != NULL);
  allo_free(&buddy, p4, 1024 * 1024UL);

  allo_destroy(&buddy);
  allo_destroy(&c_alloc);
  printf("Basic Buddy tests passed!\n");
}

void test_buddy_alignment(void) {
  printf("Testing Buddy Allocator: Alignment\n");
  static ALLO_ALIGNED_BUF(buffer, (64 * 1024UL) + (16 * 1024UL));
  allo_t fixed;
  assert(make_fixed_buf_allocator(&fixed, buffer, sizeof(buffer)) == ALLO_OK);
  allo_t buddy;
  assert(make_buddy_allocator(&buddy, &fixed, NULL, 64 * 1024UL) == ALLO_OK);

  for (int i = 0; i < 100; ++i) {
    void *p = allo_alloc(&buddy, 1);
    assert(p != NULL);
    assert(((uintptr_t)p % _Alignof(max_align_t)) == 0);
    allo_free(&buddy, p, 1);
    if (i % 10 == 0)
      printf("alignment iter %d\n", i);
  }

  allo_destroy(&buddy);
  allo_destroy(&fixed);
  printf("Alignment Buddy tests passed!\n");
}

void test_buddy_torture(void) {
  printf("Testing Buddy Allocator: Torture\n");
  static ALLO_ALIGNED_BUF(buffer, 256 * 1024UL);
  allo_t fixed, buddy;
  assert(make_fixed_buf_allocator(&fixed, buffer, sizeof(buffer)) == ALLO_OK);
  assert(make_buddy_allocator(&buddy, &fixed, NULL, 128 * 1024UL) == ALLO_OK);

  void *ptrs[1024];
  int count = 0;

  // 1. Allocate many small blocks to force deep splits
  for (int i = 0; i < 64; ++i) {
    ptrs[count] = allo_alloc(&buddy, 1024);
    assert(ptrs[count] != NULL);
    memset(ptrs[count], 0xCC, 1024);
    count++;
  }

  // 2. Allocate some large blocks
  ptrs[count] = allo_alloc(&buddy, 32 * 1024UL);
  assert(ptrs[count] != NULL);
  count++;

  // 3. Free everything to force coalescing
  for (int i = 0; i < 64; ++i) {
    allo_free(&buddy, ptrs[i], 1024);
  }
  allo_free(&buddy, ptrs[64], 32 * 1024UL);

  // 4. Try to allocate the whole thing again (proves perfect coalescing)
  void *big = allo_alloc(&buddy, 128 * 1024UL);
  assert(big != NULL);
  allo_free(&buddy, big, 128 * 1024UL);

  // 5. Randomish sequence
  for (int i = 0; i < 100; ++i) {
    size_t sz = (i % 7 + 1) * 1024UL;
    void *p = allo_alloc(&buddy, sz);
    if (p)
      allo_free(&buddy, p, sz);
  }

  allo_destroy(&buddy);
  allo_destroy(&fixed);
  printf("Buddy torture tests passed!\n");
}

void test_buddy_shrink_in_place(void) {
  ALLO_ALIGNED_BUF(child_buffer, 1024);
  allo_t child, buddy;
  assert(make_fixed_buf_allocator(&child, child_buffer, sizeof(child_buffer)) ==
         ALLO_OK);
  assert(make_buddy_allocator(&buddy, &child, NULL, 64) == ALLO_OK);

  unsigned char *p = allo_alloc(&buddy, 64);
  assert(p != NULL);
  memset(p, 0xA7, 64);
  assert(allo_realloc(&buddy, p, 64, 32) == p);
  for (size_t i = 0; i < 32; ++i)
    assert(p[i] == 0xA7);

  void *right = allo_alloc(&buddy, 32);
  assert(right == p + 32);
  allo_free(&buddy, right, 32);
  allo_free(&buddy, p, 32);
  assert(allo_alloc(&buddy, 64) == p);
  allo_destroy(&buddy);
  allo_destroy(&child);
}


void test_buddy_validation(void) {
  printf("Testing Buddy Allocator: Validation\n");
  static ALLO_ALIGNED_BUF(buffer, 1024);
  allo_t child, a;
  assert(make_fixed_buf_allocator(&child, buffer, sizeof(buffer)) == ALLO_OK);

  assert(make_buddy_allocator(NULL, &child, NULL, 1024) == ALLO_ERR_INVAL);
  assert(make_buddy_allocator(&a, NULL, NULL, 1024) == ALLO_ERR_INVAL);
  assert(make_buddy_allocator(&a, &child, NULL, 0) == ALLO_ERR_INVAL);
  assert(make_buddy_allocator(&a, &child, NULL, 16) == ALLO_ERR_INVAL);

  ALLO_ALIGNED_BUF(external, 1024);
  assert(make_buddy_allocator(&a, &child, external, 1000) == ALLO_ERR_INVAL);
  if (_Alignof(max_align_t) > 8) {
    assert(make_buddy_allocator(&a, &child, (char *)external + 8, 1024) ==
           ALLO_ERR_INVAL);
  }

#ifndef ALLO_FREESTANDING
  allo_t c;
  assert(make_c_allocator(&c) == ALLO_OK);
  assert(make_buddy_allocator(&a, &c, NULL, SIZE_MAX) == ALLO_ERR_INVAL);
  allo_destroy(&c);
#endif

  allo_destroy(&child);
  printf("Buddy validation tests passed!\n");
}

int main(void) {
  test_buddy_validation();
  test_buddy_basic();
  test_buddy_alignment();
  test_buddy_torture();
  test_buddy_shrink_in_place();
  return 0;
}
