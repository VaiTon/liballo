#define _GNU_SOURCE

#include "linux_syscall_support.h"
#include <sys/mman.h>

static inline void *allo_os_mmap(size_t length) {
  return sys_mmap(0, length, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

static inline int allo_os_munmap(void *addr, size_t length) {
  return sys_munmap(addr, length);
}

static inline void *allo_os_mremap(void *addr, size_t old_len, size_t new_len) {
  return sys_mremap(addr, old_len, new_len, MREMAP_MAYMOVE);
}

static inline size_t allo_os_get_page_size(void) {
  return 4096;
}
