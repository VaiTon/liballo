#include "allo.h"

// Minimal x86_64 syscall for exit
static inline void sys_exit(int status) {
  __asm__ volatile("mov $60, %%rax\n" // SYS_exit
                   "mov %0, %%rdi\n"
                   "syscall"
                   :
                   : "r"((long)status)
                   : "rax", "rdi");
}

// Entry point for freestanding binary
void _start(void) {
  allo_t a;
  if (make_page_allocator(&a) != ALLO_OK) {
    sys_exit(4);
  }

  // Test allocation
  void *p1 = allo_alloc(&a, 100);
  if (p1 == NULL) {
    sys_exit(1);
  }

  // Write to memory to verify it's mapped correctly
  unsigned char *ptr = (unsigned char *)p1;
  for (int i = 0; i < 100; i++) {
    ptr[i] = (unsigned char)i;
  }

  // Verify memory
  for (int i = 0; i < 100; i++) {
    if (ptr[i] != (unsigned char)i) {
      sys_exit(2);
    }
  }

  // Test larger allocation (spanning multiple pages)
  void *p2 = allo_alloc(&a, 10000);
  if (p2 == NULL) {
    sys_exit(3);
  }

  allo_free(&a, p1);
  allo_free(&a, p2);
  allo_destroy(&a);

  sys_exit(0);
}
