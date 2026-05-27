#include "allo.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  allo_t backing_alloc;
  if (make_page_allocator(&backing_alloc) != ALLO_OK) {
    fprintf(stderr, "Failed to create page allocator\n");
    return 1;
  }

  for (size_t i = 0; i < 1000; i++) {
    allo_t arena;
    if (make_arena_allocator(&arena, &backing_alloc, 1024 * 1024UL) !=
        ALLO_OK) {
      fprintf(stderr, "Failed to create arena allocator\n");
      abort();
    }

    void *ptr1 = allo_alloc(&arena, 256);
    if (!ptr1) {
      fprintf(stderr, "Failed to allocate memory from arena\n");
      abort();
    }
    printf("Allocated 256 bytes at %p\n", ptr1);

    void *ptr2 = allo_alloc(&arena, 512);
    if (!ptr2) {
      fprintf(stderr, "Failed to allocate memory from arena\n");
      abort();
    }
    printf("Allocated 512 bytes at %p\n", ptr2);

    allo_destroy(&arena);
  }

  allo_destroy(&backing_alloc);
  printf("Destroyed backing allocator\n");

  return 0;
}
