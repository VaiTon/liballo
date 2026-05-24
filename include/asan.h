#pragma once

// ASAN integration helpers
// Prefer checking __SANITIZE_ADDRESS__ (set by compiler) or clang's
// __has_feature(address_sanitizer). Guard __has_feature behind a
// clang-specific check to avoid preprocessing errors on non-clang
// compilers that do not provide __has_feature.
#if defined(__SANITIZE_ADDRESS__) || (defined(__clang__) && __has_feature(address_sanitizer))
  #include <sanitizer/asan_interface.h>
  #define ALLOC_POISON(addr, size) ASAN_POISON_MEMORY_REGION(addr, size)
  #define ALLOC_UNPOISON(addr, size) ASAN_UNPOISON_MEMORY_REGION(addr, size)
#else
  #define ALLOC_POISON(addr, size) ((void)(addr), (void)(size))
  #define ALLOC_UNPOISON(addr, size) ((void)(addr), (void)(size))
#endif
