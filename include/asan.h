#pragma once

// ASAN integration helpers
#if defined(__SANITIZE_ADDRESS__) ||                                           \
    (defined(__has_feature) && __has_feature(address_sanitizer))
  #include <sanitizer/asan_interface.h>
  #define ALLOC_POISON(addr, size) ASAN_POISON_MEMORY_REGION(addr, size)
  #define ALLOC_UNPOISON(addr, size) ASAN_UNPOISON_MEMORY_REGION(addr, size)
#else
  #define ALLOC_POISON(addr, size) ((void)(addr), (void)(size))
  #define ALLOC_UNPOISON(addr, size) ((void)(addr), (void)(size))
#endif
