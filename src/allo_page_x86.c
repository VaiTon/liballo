#define SYS_mmap 9
#define SYS_munmap 11
#define SYS_mremap 25
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED ((void *)-1)
#define MREMAP_MAYMOVE 1

static inline long my_syscall6(long n, long a1, long a2, long a3, long a4,
                               long a5, long a6) {
  long ret;
  register long r10 __asm__("r10") = a4;
  register long r8 __asm__("r8") = a5;
  register long r9 __asm__("r9") = a6;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8),
                     "r"(r9)
                   : "rcx", "r11", "memory");
  return ret;
}

static inline long my_syscall2(long n, long a1, long a2) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(n), "D"(a1), "S"(a2)
                   : "rcx", "r11", "memory");
  return ret;
}

static inline long my_syscall4(long n, long a1, long a2, long a3, long a4) {
  long ret;
  register long r10 __asm__("r10") = a4;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
                   : "rcx", "r11", "memory");
  return ret;
}

static inline void *os_mmap(size_t length) {
  long ret =
      my_syscall6(SYS_mmap, 0, (long)length, (long)(PROT_READ | PROT_WRITE),
                  (long)(MAP_PRIVATE | MAP_ANONYMOUS), -1, 0);
  if (ret < 0 && ret >= -4095) {
    return NULL;
  }
  return (void *)ret;
}

static inline int os_munmap(void *addr, size_t length) {
  return (int)my_syscall2(SYS_munmap, (long)addr, (long)length);
}

static inline void *os_mremap(void *addr, size_t old_len, size_t new_len) {
  long ret = my_syscall4(SYS_mremap, (long)addr, (long)old_len, (long)new_len,
                         MREMAP_MAYMOVE);
  if (ret < 0 && ret >= -4095) {
    return NULL;
  }
  return (void *)ret;
}

static inline size_t os_get_page_size(void) {
  return 4096;
}
