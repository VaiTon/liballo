// Workaround for strict C11 which disables 'asm'
#ifndef __cplusplus
  #define asm __asm__
#endif

#include "allo.h"
#include "ubench.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Constants
// ============================================================================

#define TORTURE_COUNT 256
#define RACE_COUNT 512
#define REPEAT_COUNT 1000
#define FRAG_COUNT 512
#define STRIDE_COUNT 128
#define RAMP_COUNT 32
#define BATCH_SIZES 8

#define ARENA_SIZE (256 * 1024UL * 1024UL)
#define BUFFER_SIZE (256 * 1024UL * 1024UL)
#define BUDDY_TOTAL_SIZE (64 * 1024UL * 1024UL)
#define POOL_BLOCK_SIZE 64UL
#define POOL_NUM_BLOCKS 10000

// ============================================================================
// Fixtures
// ============================================================================

// --- libc ---
struct libc_fixture {
  allo_t a;
};
UBENCH_F_SETUP(libc_fixture) {
  make_c_allocator(&ubench_fixture->a);
}
UBENCH_F_TEARDOWN(libc_fixture) {
  allo_destroy(&ubench_fixture->a);
}

// --- page ---
struct page_fixture {
  allo_t a;
};
UBENCH_F_SETUP(page_fixture) {
  make_page_allocator(&ubench_fixture->a);
}
UBENCH_F_TEARDOWN(page_fixture) {
  allo_destroy(&ubench_fixture->a);
}

// --- arena ---
struct arena_fixture {
  allo_t child;
  allo_t a;
};
UBENCH_F_SETUP(arena_fixture) {
  make_c_allocator(&ubench_fixture->child);
  make_arena_allocator(&ubench_fixture->a, &ubench_fixture->child, ARENA_SIZE);
}
UBENCH_F_TEARDOWN(arena_fixture) {
  allo_destroy(&ubench_fixture->a);
  allo_destroy(&ubench_fixture->child);
}

// --- pool ---
struct pool_fixture {
  allo_t child;
  allo_t a;
};
UBENCH_F_SETUP(pool_fixture) {
  make_c_allocator(&ubench_fixture->child);
  make_pool_allocator(&ubench_fixture->a, &ubench_fixture->child, NULL,
                      POOL_BLOCK_SIZE, POOL_NUM_BLOCKS);
}
UBENCH_F_TEARDOWN(pool_fixture) {
  allo_destroy(&ubench_fixture->a);
  allo_destroy(&ubench_fixture->child);
}

// --- buddy ---
struct buddy_fixture {
  allo_t child;
  allo_t a;
  size_t sizes[TORTURE_COUNT];
  size_t realloc_sizes[TORTURE_COUNT];
};
UBENCH_F_SETUP(buddy_fixture) {
  srand(42);
  make_c_allocator(&ubench_fixture->child);
  make_buddy_allocator(&ubench_fixture->a, &ubench_fixture->child, NULL,
                       BUDDY_TOTAL_SIZE);
  for (int i = 0; i < TORTURE_COUNT; i++) {
    ubench_fixture->sizes[i] = (size_t)(rand() % 4096) + 1;
    ubench_fixture->realloc_sizes[i] = (size_t)(rand() % 2048) + 1;
  }
}
UBENCH_F_TEARDOWN(buddy_fixture) {
  allo_destroy(&ubench_fixture->a);
  allo_destroy(&ubench_fixture->child);
}

// --- buffer ---
struct buffer_fixture {
  char *buf;
  allo_t a;
};
UBENCH_F_SETUP(buffer_fixture) {
  ubench_fixture->buf = malloc(BUFFER_SIZE);
  memset(ubench_fixture->buf, 0, BUFFER_SIZE);
  make_fixed_buf_allocator(&ubench_fixture->a, ubench_fixture->buf,
                           BUFFER_SIZE);
}
UBENCH_F_TEARDOWN(buffer_fixture) {
  allo_destroy(&ubench_fixture->a);
  free(ubench_fixture->buf);
}

// --- gen ---
struct gen_fixture {
  allo_t a;
};
UBENCH_F_SETUP(gen_fixture) {
  make_gen_allocator(&ubench_fixture->a);
}
UBENCH_F_TEARDOWN(gen_fixture) {
  allo_destroy(&ubench_fixture->a);
}

// --- mtx (thread-safe wrapper over libc) ---
struct mtx_fixture {
  allo_t base;
  allo_t a;
};
UBENCH_F_SETUP(mtx_fixture) {
  make_c_allocator(&ubench_fixture->base);
  make_mtx_allocator(&ubench_fixture->a, &ubench_fixture->base);
}
UBENCH_F_TEARDOWN(mtx_fixture) {
  allo_destroy(&ubench_fixture->a);
  allo_destroy(&ubench_fixture->base);
}

// --- mtx_buddy (thread-safe wrapper over buddy) ---
struct mtx_buddy_fixture {
  allo_t child;
  allo_t buddy;
  allo_t a;
};
UBENCH_F_SETUP(mtx_buddy_fixture) {
  make_c_allocator(&ubench_fixture->child);
  make_buddy_allocator(&ubench_fixture->buddy, &ubench_fixture->child, NULL,
                       BUDDY_TOTAL_SIZE);
  make_mtx_allocator(&ubench_fixture->a, &ubench_fixture->buddy);
}
UBENCH_F_TEARDOWN(mtx_buddy_fixture) {
  allo_destroy(&ubench_fixture->a);
  allo_destroy(&ubench_fixture->buddy);
  allo_destroy(&ubench_fixture->child);
}

// --- fallback (pool → libc) ---
struct fallback_fixture {
  allo_t primary;
  allo_t fallback;
  allo_t fb;
  void *pool_buf;
};
UBENCH_F_SETUP(fallback_fixture) {
  size_t pool_size = 64 * 1024;
  ubench_fixture->pool_buf = malloc(pool_size);
  make_pool_allocator(&ubench_fixture->primary, NULL, ubench_fixture->pool_buf,
                      64, 1000);
  make_c_allocator(&ubench_fixture->fallback);
  make_fallback_allocator(&ubench_fixture->fb, &ubench_fixture->primary,
                          &ubench_fixture->fallback);
}
UBENCH_F_TEARDOWN(fallback_fixture) {
  allo_destroy(&ubench_fixture->fb);
  allo_destroy(&ubench_fixture->primary);
  allo_destroy(&ubench_fixture->fallback);
  free(ubench_fixture->pool_buf);
}

// --- fallback_arena (arena → libc) ---
//
// Models a common pattern: a per-frame/request arena that falls back to the
// heap once the slab is exhausted. The arena here is intentionally small so
// the overflow path is exercised at a predictable frequency.
#define FALLBACK_ARENA_SIZE (4 * 1024UL)
struct fallback_arena_fixture {
  allo_t heap;
  allo_t arena;
  allo_t fb;
};
UBENCH_F_SETUP(fallback_arena_fixture) {
  make_c_allocator(&ubench_fixture->heap);
  make_arena_allocator(&ubench_fixture->arena, &ubench_fixture->heap,
                       FALLBACK_ARENA_SIZE);
  make_fallback_allocator(&ubench_fixture->fb, &ubench_fixture->arena,
                          &ubench_fixture->heap);
}
UBENCH_F_TEARDOWN(fallback_arena_fixture) {
  allo_destroy(&ubench_fixture->fb);
  allo_destroy(&ubench_fixture->arena);
  allo_destroy(&ubench_fixture->heap);
}

// ============================================================================
// Shared random state (size distributions)
// ============================================================================

struct race_state {
  size_t sizes[RACE_COUNT];
};
static struct race_state global_race_state;

struct frag_state {
  size_t sizes[FRAG_COUNT];
};
static struct frag_state global_frag_state;

// Size classes representative of a real allocator workload:
//   ~50% tiny  (1–64 B)
//   ~30% small (65–512 B)
//   ~15% medium (513–4096 B)
//   ~5%  large (4097–65536 B)
static size_t global_workload_sizes[RACE_COUNT];

static void init_race_state(void) {
  srand(123);
  for (int i = 0; i < RACE_COUNT; i++)
    global_race_state.sizes[i] = (size_t)(rand() % 2048) + 1;
}

static void init_frag_state(void) {
  srand(999);
  for (int i = 0; i < FRAG_COUNT; i++)
    global_frag_state.sizes[i] = (size_t)(rand() % 256) + 1;
}

static void init_workload_sizes(void) {
  srand(77777);
  for (int i = 0; i < RACE_COUNT; i++) {
    int bucket = rand() % 100;
    if (bucket < 50)
      global_workload_sizes[i] = (size_t)(rand() % 64) + 1;
    else if (bucket < 80)
      global_workload_sizes[i] = (size_t)(rand() % 448) + 65;
    else if (bucket < 95)
      global_workload_sizes[i] = (size_t)(rand() % 3584) + 513;
    else
      global_workload_sizes[i] = (size_t)(rand() % 61440) + 4097;
  }
}

// ============================================================================
// Macro families for repetitive benchmark definitions
// ============================================================================

// Alloc-only (no free): tests pure allocation throughput for bump allocators.
#define DEFINE_ALLOC_BENCHMARK(FIXTURE, NAME, SIZE)                            \
  UBENCH_F(FIXTURE, alloc_only_##NAME) {                                       \
    void *ptr = allo_alloc(&ubench_fixture->a, SIZE);                          \
    ubench_do_nothing(ptr);                                                    \
  }

// Alloc + free loop: the bread-and-butter benchmark.
#define DEFINE_ALLOC_FREE_BENCHMARK(FIXTURE, NAME, SIZE)                       \
  UBENCH_F(FIXTURE, alloc_free_##NAME) {                                       \
    for (int i = 0; i < REPEAT_COUNT; i++) {                                   \
      void *ptr = allo_alloc(&ubench_fixture->a, SIZE);                        \
      ubench_do_nothing(ptr);                                                  \
      allo_free(&ubench_fixture->a, ptr, SIZE);                                \
    }                                                                          \
  }

// calloc + free loop.
#define DEFINE_CALLOC_BENCHMARK(NAME, FIXTURE, SIZE)                           \
  UBENCH_F(FIXTURE, calloc_free_##NAME) {                                      \
    for (int i = 0; i < REPEAT_COUNT; i++) {                                   \
      void *ptr = allo_calloc(&ubench_fixture->a, 1, SIZE);                    \
      ubench_do_nothing(ptr);                                                  \
      allo_free(&ubench_fixture->a, ptr, SIZE);                                \
    }                                                                          \
  }

// Expand all three size tiers for a fixture.
#define DEFINE_ALLOC_BENCHMARKS(FIXTURE, NAME)                                 \
  DEFINE_ALLOC_BENCHMARK(FIXTURE, 8b, 8)                                       \
  DEFINE_ALLOC_BENCHMARK(FIXTURE, 1kb, 1024)                                   \
  DEFINE_ALLOC_BENCHMARK(FIXTURE, 64kb, 64 * 1024)

#define DEFINE_ALLOC_FREE_BENCHMARKS(FIXTURE, NAME)                            \
  DEFINE_ALLOC_FREE_BENCHMARK(FIXTURE, 8b, 8)                                  \
  DEFINE_ALLOC_FREE_BENCHMARK(FIXTURE, 1kb, 1024)                              \
  DEFINE_ALLOC_FREE_BENCHMARK(FIXTURE, 64kb, 64 * 1024)

// Race benchmark: bulk alloc then bulk free (tests cache reuse and locality).
#define DEFINE_RACE_BENCHMARK(NAME, FIXTURE)                                   \
  UBENCH_F(FIXTURE, race_##NAME) {                                             \
    void *ptrs[RACE_COUNT];                                                    \
    for (int i = 0; i < RACE_COUNT; i++) {                                     \
      ptrs[i] = allo_alloc(&ubench_fixture->a, global_race_state.sizes[i]);    \
      ubench_do_nothing(ptrs[i]);                                              \
    }                                                                          \
    for (int i = 0; i < RACE_COUNT; i++)                                       \
      allo_free(&ubench_fixture->a, ptrs[i], global_race_state.sizes[i]);      \
  }

// ============================================================================
// Original benchmarks (preserved exactly)
// ============================================================================

DEFINE_ALLOC_BENCHMARKS(libc_fixture, libc)
DEFINE_ALLOC_BENCHMARKS(arena_fixture, arena)
DEFINE_ALLOC_BENCHMARKS(buffer_fixture, buffer)
DEFINE_ALLOC_BENCHMARKS(gen_fixture, gen)
DEFINE_ALLOC_BENCHMARKS(buddy_fixture, buddy)

DEFINE_ALLOC_FREE_BENCHMARKS(libc_fixture, libc)
DEFINE_ALLOC_FREE_BENCHMARKS(buddy_fixture, buddy)
DEFINE_ALLOC_FREE_BENCHMARKS(gen_fixture, gen)
DEFINE_ALLOC_FREE_BENCHMARKS(mtx_fixture, mtx)

DEFINE_CALLOC_BENCHMARK(1kb, libc_fixture, 1024)
DEFINE_CALLOC_BENCHMARK(1kb, buddy_fixture, 1024)

UBENCH_F(libc_fixture, realloc_move) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, 1024);
    ptr = allo_realloc(&ubench_fixture->a, ptr, 1024, 2048);
    ubench_do_nothing(ptr);
    ptr = allo_realloc(&ubench_fixture->a, ptr, 2048, 1024);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->a, ptr, 1024);
  }
}

UBENCH_F(arena_fixture, realloc_inplace) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, 8);
    ptr = allo_realloc(&ubench_fixture->a, ptr, 8, 16);
    ubench_do_nothing(ptr);
  }
}

UBENCH_F(buddy_fixture, realloc_move) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, 1024);
    ptr = allo_realloc(&ubench_fixture->a, ptr, 1024, 2048);
    ubench_do_nothing(ptr);
    ptr = allo_realloc(&ubench_fixture->a, ptr, 2048, 1024);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->a, ptr, 1024);
  }
}

UBENCH_F(buffer_fixture, realloc_inplace) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, 8);
    ptr = allo_realloc(&ubench_fixture->a, ptr, 8, 16);
    ubench_do_nothing(ptr);
  }
}

UBENCH_F(buddy_fixture, torture_mix) {
  void *ptrs[TORTURE_COUNT];
  for (int i = 0; i < TORTURE_COUNT; i++) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, ubench_fixture->sizes[i]);
    ubench_do_nothing(ptrs[i]);
  }
  for (int i = 0; i < TORTURE_COUNT; i += 2) {
    if (ptrs[i]) {
      allo_free(&ubench_fixture->a, ptrs[i], ubench_fixture->sizes[i]);
      ptrs[i] = NULL;
    }
  }
  for (int i = 0; i < TORTURE_COUNT; i += 2) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, ubench_fixture->realloc_sizes[i]);
    ubench_do_nothing(ptrs[i]);
  }
  for (int i = 0; i < TORTURE_COUNT; i++) {
    if (ptrs[i]) {
      size_t sz = (i % 2 == 0) ? ubench_fixture->realloc_sizes[i]
                               : ubench_fixture->sizes[i];
      allo_free(&ubench_fixture->a, ptrs[i], sz);
    }
  }
}

DEFINE_RACE_BENCHMARK(libc, libc_fixture)
DEFINE_RACE_BENCHMARK(buddy, buddy_fixture)
DEFINE_RACE_BENCHMARK(gen, gen_fixture)

UBENCH_F(page_fixture, single_page_alloc_free) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, 4096);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->a, ptr, 4096);
  }
}

#define DEFINE_LIFECYCLE_BENCHMARK(NAME, ...)                                  \
  UBENCH(lifecycle, NAME) {                                                    \
    __VA_ARGS__                                                                \
  }

DEFINE_LIFECYCLE_BENCHMARK(libc, {
  allo_t a;
  make_c_allocator(&a);
  allo_destroy(&a);
})
DEFINE_LIFECYCLE_BENCHMARK(arena, {
  allo_t child, a;
  make_c_allocator(&child);
  make_arena_allocator(&a, &child, 64 * 1024);
  allo_destroy(&a);
  allo_destroy(&child);
})
DEFINE_LIFECYCLE_BENCHMARK(pool, {
  allo_t child, a;
  make_c_allocator(&child);
  make_pool_allocator(&a, &child, NULL, 64, 100);
  allo_destroy(&a);
  allo_destroy(&child);
})
DEFINE_LIFECYCLE_BENCHMARK(buddy, {
  allo_t child, a;
  make_c_allocator(&child);
  make_buddy_allocator(&a, &child, NULL, 64 * 1024);
  allo_destroy(&a);
  allo_destroy(&child);
})
DEFINE_LIFECYCLE_BENCHMARK(mtx, {
  allo_t base, a;
  make_c_allocator(&base);
  make_mtx_allocator(&a, &base);
  allo_destroy(&a);
  allo_destroy(&base);
})

UBENCH_F(fallback_fixture, fast_path_8b) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->fb, 8);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->fb, ptr, 8);
  }
}
UBENCH_F(fallback_fixture, slow_path_1kb) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->fb, 1024);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->fb, ptr, 1024);
  }
}

// ============================================================================
// NEW: Size-class sweep
//
// Allocate at every power-of-two from 8 B to 64 KiB so charts of
// latency-vs-size can be drawn. Each benchmark is a single alloc+free pair
// (REPEAT_COUNT iterations) at a fixed size, giving one data point per cell.
// ============================================================================

#define DEFINE_SIZE_SWEEP_BENCHMARK(FIXTURE, SIZE, LABEL)                      \
  UBENCH_F(FIXTURE, size_sweep_##LABEL) {                                      \
    for (int i = 0; i < REPEAT_COUNT; i++) {                                   \
      void *ptr = allo_alloc(&ubench_fixture->a, SIZE);                        \
      ubench_do_nothing(ptr);                                                  \
      allo_free(&ubench_fixture->a, ptr, SIZE);                                \
    }                                                                          \
  }

#define DEFINE_ALL_SIZE_SWEEPS(FIXTURE)                                        \
  DEFINE_SIZE_SWEEP_BENCHMARK(FIXTURE, 8, 8b)                                  \
  DEFINE_SIZE_SWEEP_BENCHMARK(FIXTURE, 16, 16b)                                \
  DEFINE_SIZE_SWEEP_BENCHMARK(FIXTURE, 32, 32b)                                \
  DEFINE_SIZE_SWEEP_BENCHMARK(FIXTURE, 64, 64b)                                \
  DEFINE_SIZE_SWEEP_BENCHMARK(FIXTURE, 128, 128b)                              \
  DEFINE_SIZE_SWEEP_BENCHMARK(FIXTURE, 256, 256b)                              \
  DEFINE_SIZE_SWEEP_BENCHMARK(FIXTURE, 512, 512b)                              \
  DEFINE_SIZE_SWEEP_BENCHMARK(FIXTURE, 1024, 1kb)                              \
  DEFINE_SIZE_SWEEP_BENCHMARK(FIXTURE, 2048, 2kb)                              \
  DEFINE_SIZE_SWEEP_BENCHMARK(FIXTURE, 4096, 4kb)                              \
  DEFINE_SIZE_SWEEP_BENCHMARK(FIXTURE, 8192, 8kb)                              \
  DEFINE_SIZE_SWEEP_BENCHMARK(FIXTURE, 16384, 16kb)                            \
  DEFINE_SIZE_SWEEP_BENCHMARK(FIXTURE, 32768, 32kb)                            \
  DEFINE_SIZE_SWEEP_BENCHMARK(FIXTURE, 65536, 64kb)

DEFINE_ALL_SIZE_SWEEPS(libc_fixture)
DEFINE_ALL_SIZE_SWEEPS(buddy_fixture)
DEFINE_ALL_SIZE_SWEEPS(gen_fixture)

// Arena and buffer are bump allocators: only alloc is meaningful here.
// We report alloc_only so the graphs stay comparable (free is a no-op).
#define DEFINE_BUMP_SIZE_SWEEP(FIXTURE, SIZE, LABEL)                           \
  UBENCH_F(FIXTURE, size_sweep_##LABEL) {                                      \
    void *ptr = allo_alloc(&ubench_fixture->a, SIZE);                          \
    ubench_do_nothing(ptr);                                                    \
  }

#define DEFINE_ALL_BUMP_SIZE_SWEEPS(FIXTURE)                                   \
  DEFINE_BUMP_SIZE_SWEEP(FIXTURE, 8, 8b)                                       \
  DEFINE_BUMP_SIZE_SWEEP(FIXTURE, 16, 16b)                                     \
  DEFINE_BUMP_SIZE_SWEEP(FIXTURE, 32, 32b)                                     \
  DEFINE_BUMP_SIZE_SWEEP(FIXTURE, 64, 64b)                                     \
  DEFINE_BUMP_SIZE_SWEEP(FIXTURE, 128, 128b)                                   \
  DEFINE_BUMP_SIZE_SWEEP(FIXTURE, 256, 256b)                                   \
  DEFINE_BUMP_SIZE_SWEEP(FIXTURE, 512, 512b)                                   \
  DEFINE_BUMP_SIZE_SWEEP(FIXTURE, 1024, 1kb)                                   \
  DEFINE_BUMP_SIZE_SWEEP(FIXTURE, 2048, 2kb)                                   \
  DEFINE_BUMP_SIZE_SWEEP(FIXTURE, 4096, 4kb)                                   \
  DEFINE_BUMP_SIZE_SWEEP(FIXTURE, 8192, 8kb)                                   \
  DEFINE_BUMP_SIZE_SWEEP(FIXTURE, 16384, 16kb)                                 \
  DEFINE_BUMP_SIZE_SWEEP(FIXTURE, 32768, 32kb)                                 \
  DEFINE_BUMP_SIZE_SWEEP(FIXTURE, 65536, 64kb)

DEFINE_ALL_BUMP_SIZE_SWEEPS(arena_fixture)
DEFINE_ALL_BUMP_SIZE_SWEEPS(buffer_fixture)

// ============================================================================
// NEW: Fragmentation stress
//
// Allocate FRAG_COUNT blocks of random sizes [1,256 B], then free every other
// one to leave a Swiss-cheese heap, then re-allocate into the holes. This
// exercises the allocator's free-list / buddy-merge logic under realistic
// fragmentation and is the workload where buddy and gen should diverge most.
// ============================================================================

UBENCH_F(libc_fixture, frag_stress) {
  void *ptrs[FRAG_COUNT];
  // Phase 1: fill
  for (int i = 0; i < FRAG_COUNT; i++) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, global_frag_state.sizes[i]);
    ubench_do_nothing(ptrs[i]);
  }
  // Phase 2: free every other block (creates fragmentation)
  for (int i = 0; i < FRAG_COUNT; i += 2)
    allo_free(&ubench_fixture->a, ptrs[i], global_frag_state.sizes[i]);
  // Phase 3: re-allocate into the gaps
  for (int i = 0; i < FRAG_COUNT; i += 2) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, global_frag_state.sizes[i]);
    ubench_do_nothing(ptrs[i]);
  }
  // Cleanup
  for (int i = 0; i < FRAG_COUNT; i++)
    allo_free(&ubench_fixture->a, ptrs[i], global_frag_state.sizes[i]);
}

UBENCH_F(buddy_fixture, frag_stress) {
  void *ptrs[FRAG_COUNT];
  for (int i = 0; i < FRAG_COUNT; i++) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, global_frag_state.sizes[i]);
    ubench_do_nothing(ptrs[i]);
  }
  for (int i = 0; i < FRAG_COUNT; i += 2)
    allo_free(&ubench_fixture->a, ptrs[i], global_frag_state.sizes[i]);
  for (int i = 0; i < FRAG_COUNT; i += 2) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, global_frag_state.sizes[i]);
    ubench_do_nothing(ptrs[i]);
  }
  for (int i = 0; i < FRAG_COUNT; i++)
    allo_free(&ubench_fixture->a, ptrs[i], global_frag_state.sizes[i]);
}

UBENCH_F(gen_fixture, frag_stress) {
  void *ptrs[FRAG_COUNT];
  for (int i = 0; i < FRAG_COUNT; i++) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, global_frag_state.sizes[i]);
    ubench_do_nothing(ptrs[i]);
  }
  for (int i = 0; i < FRAG_COUNT; i += 2)
    allo_free(&ubench_fixture->a, ptrs[i], global_frag_state.sizes[i]);
  for (int i = 0; i < FRAG_COUNT; i += 2) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, global_frag_state.sizes[i]);
    ubench_do_nothing(ptrs[i]);
  }
  for (int i = 0; i < FRAG_COUNT; i++)
    allo_free(&ubench_fixture->a, ptrs[i], global_frag_state.sizes[i]);
}

// ============================================================================
// NEW: Realistic mixed-size workload
//
// Uses the global_workload_sizes[] distribution (~50% tiny, 30% small,
// 15% medium, 5% large) to mimic an application heap. Runs in two patterns:
//   sequential  — alloc all, then free all (batch pattern, cache-friendly)
//   interleaved — alloc+free each block immediately (streaming pattern)
// ============================================================================

UBENCH_F(libc_fixture, workload_sequential) {
  void *ptrs[RACE_COUNT];
  for (int i = 0; i < RACE_COUNT; i++) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, global_workload_sizes[i]);
    ubench_do_nothing(ptrs[i]);
  }
  for (int i = 0; i < RACE_COUNT; i++)
    allo_free(&ubench_fixture->a, ptrs[i], global_workload_sizes[i]);
}

UBENCH_F(buddy_fixture, workload_sequential) {
  void *ptrs[RACE_COUNT];
  for (int i = 0; i < RACE_COUNT; i++) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, global_workload_sizes[i]);
    ubench_do_nothing(ptrs[i]);
  }
  for (int i = 0; i < RACE_COUNT; i++)
    allo_free(&ubench_fixture->a, ptrs[i], global_workload_sizes[i]);
}

UBENCH_F(gen_fixture, workload_sequential) {
  void *ptrs[RACE_COUNT];
  for (int i = 0; i < RACE_COUNT; i++) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, global_workload_sizes[i]);
    ubench_do_nothing(ptrs[i]);
  }
  for (int i = 0; i < RACE_COUNT; i++)
    allo_free(&ubench_fixture->a, ptrs[i], global_workload_sizes[i]);
}

UBENCH_F(libc_fixture, workload_interleaved) {
  for (int i = 0; i < RACE_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, global_workload_sizes[i]);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->a, ptr, global_workload_sizes[i]);
  }
}

UBENCH_F(buddy_fixture, workload_interleaved) {
  for (int i = 0; i < RACE_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, global_workload_sizes[i]);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->a, ptr, global_workload_sizes[i]);
  }
}

UBENCH_F(gen_fixture, workload_interleaved) {
  for (int i = 0; i < RACE_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, global_workload_sizes[i]);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->a, ptr, global_workload_sizes[i]);
  }
}

// ============================================================================
// NEW: Realloc growth/shrink patterns
//
// Three patterns that show up in real code:
//   ramp_up   — repeated doublings (e.g. growing a vector)
//   ramp_down — repeated halvings (e.g. trimming a buffer)
//   zigzag    — alternating grow/shrink around a pivot (e.g. streaming codec)
// ============================================================================

UBENCH_F(libc_fixture, realloc_ramp_up) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, 8);
    size_t sz = 8;
    for (int step = 0; step < RAMP_COUNT; step++) {
      size_t next = sz * 2;
      ptr = allo_realloc(&ubench_fixture->a, ptr, sz, next);
      ubench_do_nothing(ptr);
      sz = next;
    }
    allo_free(&ubench_fixture->a, ptr, sz);
  }
}

UBENCH_F(buddy_fixture, realloc_ramp_up) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, 8);
    size_t sz = 8;
    for (int step = 0; step < RAMP_COUNT; step++) {
      // Cap at 1/4 of buddy pool so we don't exhaust the 64 MiB slab.
      size_t next = (sz * 2 < BUDDY_TOTAL_SIZE / 4) ? sz * 2 : sz;
      ptr = allo_realloc(&ubench_fixture->a, ptr, sz, next);
      ubench_do_nothing(ptr);
      sz = next;
    }
    allo_free(&ubench_fixture->a, ptr, sz);
  }
}

UBENCH_F(libc_fixture, realloc_ramp_down) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    size_t start = (size_t)1 << RAMP_COUNT; // 4 GiB — capped below
    // Stay within 16 MiB for reasonable test speed
    start = 16 * 1024 * 1024UL;
    void *ptr = allo_alloc(&ubench_fixture->a, start);
    size_t sz = start;
    for (int step = 0; step < RAMP_COUNT && sz > 8; step++) {
      size_t next = sz / 2;
      ptr = allo_realloc(&ubench_fixture->a, ptr, sz, next);
      ubench_do_nothing(ptr);
      sz = next;
    }
    allo_free(&ubench_fixture->a, ptr, sz);
  }
}

UBENCH_F(libc_fixture, realloc_zigzag) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, 512);
    size_t pivot = 512;
    for (int step = 0; step < RAMP_COUNT; step++) {
      size_t next = (step % 2 == 0) ? pivot * 2 : pivot;
      ptr = allo_realloc(&ubench_fixture->a, ptr,
                         (step % 2 == 0) ? pivot : pivot * 2, next);
      ubench_do_nothing(ptr);
    }
    allo_free(&ubench_fixture->a, ptr, pivot);
  }
}

UBENCH_F(buddy_fixture, realloc_zigzag) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, 512);
    size_t pivot = 512;
    for (int step = 0; step < RAMP_COUNT; step++) {
      size_t next = (step % 2 == 0) ? pivot * 2 : pivot;
      ptr = allo_realloc(&ubench_fixture->a, ptr,
                         (step % 2 == 0) ? pivot : pivot * 2, next);
      ubench_do_nothing(ptr);
    }
    allo_free(&ubench_fixture->a, ptr, pivot);
  }
}

// ============================================================================
// NEW: Batch allocation (LIFO / FIFO / random-order free)
//
// Allocates BATCH_COUNT blocks of the same size, then frees them in a
// different order. This exercises free-list shape and coalescing differently
// from the one-at-a-time pattern.
// ============================================================================

#define BATCH_COUNT 256

#define DEFINE_BATCH_BENCHMARK(FIXTURE, NAME, SIZE, FREE_ORDER)                \
  UBENCH_F(FIXTURE, batch_##NAME##_##FREE_ORDER) {                             \
    void *ptrs[BATCH_COUNT];                                                   \
    for (int i = 0; i < BATCH_COUNT; i++) {                                    \
      ptrs[i] = allo_alloc(&ubench_fixture->a, SIZE);                          \
      ubench_do_nothing(ptrs[i]);                                              \
    }                                                                          \
    _batch_free_##FREE_ORDER(&ubench_fixture->a, ptrs, BATCH_COUNT, SIZE);     \
  }

static inline void _batch_free_lifo(allo_t *a, void **ptrs, int n, size_t sz) {
  for (int i = n - 1; i >= 0; i--)
    allo_free(a, ptrs[i], sz);
}

static inline void _batch_free_fifo(allo_t *a, void **ptrs, int n, size_t sz) {
  for (int i = 0; i < n; i++)
    allo_free(a, ptrs[i], sz);
}

// Fisher-Yates with a local RNG so the order is deterministic and doesn't
// disturb global_race_state.
static inline void _batch_free_random(allo_t *a, void **ptrs, int n,
                                      size_t sz) {
  unsigned int rng = 54321u;
  for (int i = n - 1; i > 0; i--) {
    rng = rng * 1664525u + 1013904223u;
    int j = (int)(rng % (unsigned)(i + 1));
    void *tmp = ptrs[i];
    ptrs[i] = ptrs[j];
    ptrs[j] = tmp;
  }
  for (int i = 0; i < n; i++)
    allo_free(a, ptrs[i], sz);
}

DEFINE_BATCH_BENCHMARK(libc_fixture, 64b, 64, lifo)
DEFINE_BATCH_BENCHMARK(libc_fixture, 64b, 64, fifo)
DEFINE_BATCH_BENCHMARK(libc_fixture, 64b, 64, random)
DEFINE_BATCH_BENCHMARK(buddy_fixture, 64b, 64, lifo)
DEFINE_BATCH_BENCHMARK(buddy_fixture, 64b, 64, fifo)
DEFINE_BATCH_BENCHMARK(buddy_fixture, 64b, 64, random)
DEFINE_BATCH_BENCHMARK(gen_fixture, 64b, 64, lifo)
DEFINE_BATCH_BENCHMARK(gen_fixture, 64b, 64, fifo)
DEFINE_BATCH_BENCHMARK(gen_fixture, 64b, 64, random)

DEFINE_BATCH_BENCHMARK(libc_fixture, 1kb, 1024, lifo)
DEFINE_BATCH_BENCHMARK(libc_fixture, 1kb, 1024, fifo)
DEFINE_BATCH_BENCHMARK(buddy_fixture, 1kb, 1024, lifo)
DEFINE_BATCH_BENCHMARK(buddy_fixture, 1kb, 1024, fifo)
DEFINE_BATCH_BENCHMARK(gen_fixture, 1kb, 1024, lifo)
DEFINE_BATCH_BENCHMARK(gen_fixture, 1kb, 1024, fifo)

// ============================================================================
// NEW: Pool-specific benchmarks
//
// The pool allocator is fixed-size so we only have one meaningful size, but
// there are interesting patterns: sequential vs. random-order free, and the
// pool-exhaustion path where blocks must be re-requested from the child.
// ============================================================================

UBENCH_F(pool_fixture, alloc_free_64b) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->a, POOL_BLOCK_SIZE);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->a, ptr, POOL_BLOCK_SIZE);
  }
}

// Drain most of the pool then refill — exercises the "grow from child" path.
UBENCH_F(pool_fixture, exhaust_and_refill) {
  void *ptrs[POOL_NUM_BLOCKS];
  int fill = (POOL_NUM_BLOCKS * 9) / 10; // drain 90%
  for (int i = 0; i < fill; i++) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, POOL_BLOCK_SIZE);
    ubench_do_nothing(ptrs[i]);
  }
  for (int i = 0; i < fill; i++)
    allo_free(&ubench_fixture->a, ptrs[i], POOL_BLOCK_SIZE);
  // Now re-allocate — blocks should come straight from the free list.
  for (int i = 0; i < fill; i++) {
    ptrs[i] = allo_alloc(&ubench_fixture->a, POOL_BLOCK_SIZE);
    ubench_do_nothing(ptrs[i]);
  }
  for (int i = 0; i < fill; i++)
    allo_free(&ubench_fixture->a, ptrs[i], POOL_BLOCK_SIZE);
}

// ============================================================================
// NEW: Arena-specific patterns
//
// Arena resets (bulk-free everything in O(1)) are a key feature absent from
// other allocators. We measure: alloc-fill, reset cost, and post-reset reuse.
// ============================================================================

UBENCH_F(arena_fixture, bulk_reset_reuse) {
  for (int rep = 0; rep < 100; rep++) {
    // Allocate a bunch of mixed-size objects
    for (int i = 0; i < STRIDE_COUNT; i++) {
      size_t sz = (size_t)(i % 128) + 8;
      void *ptr = allo_alloc(&ubench_fixture->a, sz);
      ubench_do_nothing(ptr);
    }
    // Simulate a bulk-reset: destroy and recreate the arena over the same
    // child allocator (models a per-frame arena reset pattern).
    allo_destroy(&ubench_fixture->a);
    make_arena_allocator(&ubench_fixture->a, &ubench_fixture->child,
                         ARENA_SIZE);
  }
}

// Measures the throughput advantage of arena for short-lived object graphs
// (e.g. per-request allocations in a server).
UBENCH_F(arena_fixture, short_lived_objects) {
  for (int rep = 0; rep < REPEAT_COUNT; rep++) {
    void *a = allo_alloc(&ubench_fixture->a, 64);
    void *b = allo_alloc(&ubench_fixture->a, 128);
    void *c = allo_alloc(&ubench_fixture->a, 256);
    ubench_do_nothing(a);
    ubench_do_nothing(b);
    ubench_do_nothing(c);
    // Recreate the arena to reclaim all memory at once (destroy+reinit
    // over the same child is the liballo equivalent of a bump-pointer reset).
    allo_destroy(&ubench_fixture->a);
    make_arena_allocator(&ubench_fixture->a, &ubench_fixture->child,
                         ARENA_SIZE);
  }
}

// ============================================================================
// NEW: mtx (mutex-wrapped) overhead quantification
//
// These benchmarks isolate the cost of the mutex wrapper itself by pairing
// mtx_fixture (libc + lock) against libc_fixture (libc, no lock) at equal
// sizes. The difference is pure locking overhead.
// mtx_fixture alloc_free_{8b,1kb,64kb} are already registered in the
// original section above; here we add the buddy-wrapped variant.
// ============================================================================

DEFINE_ALLOC_FREE_BENCHMARKS(mtx_buddy_fixture, mtx_buddy)

UBENCH_F(mtx_fixture, calloc_free_1kb) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_calloc(&ubench_fixture->a, 1, 1024);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->a, ptr, 1024);
  }
}

// ============================================================================
// NEW: Fallback path benchmarks
//
// These extend the original two fallback benchmarks with scenarios designed
// to stress the dispatch decision path.
// ============================================================================

// Hit/miss ratio: alternate between pool-size (fast) and oversized (slow).
UBENCH_F(fallback_fixture, alternating_path) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    size_t sz = (i % 2 == 0) ? 8 : 1024;
    void *ptr = allo_alloc(&ubench_fixture->fb, sz);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->fb, ptr, sz);
  }
}

// All fast-path (tests that the fallback check doesn't add measurable cost).
UBENCH_F(fallback_fixture, all_fast_256b) {
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->fb, 32);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->fb, ptr, 32);
  }
}

// Arena-backed primary that quickly overflows — models frame allocator.
UBENCH_F(fallback_arena_fixture, overflow_heavy) {
  // Each alloc is larger than the 4 KiB arena primary so we always fall back.
  for (int i = 0; i < REPEAT_COUNT; i++) {
    void *ptr = allo_alloc(&ubench_fixture->fb, 8192);
    ubench_do_nothing(ptr);
    allo_free(&ubench_fixture->fb, ptr, 8192);
  }
}

UBENCH_F(fallback_arena_fixture, overflow_none) {
  // Tiny allocs that stay inside the 4 KiB arena, then reclaim via
  // destroy+reinit (liballo's equivalent of a bump-pointer reset).
  for (int rep = 0; rep < 100; rep++) {
    for (int i = 0; i < 16; i++) {
      void *ptr = allo_alloc(&ubench_fixture->fb, 64);
      ubench_do_nothing(ptr);
    }
    allo_destroy(&ubench_fixture->fb);
    allo_destroy(&ubench_fixture->arena);
    make_arena_allocator(&ubench_fixture->arena, &ubench_fixture->heap,
                         FALLBACK_ARENA_SIZE);
    make_fallback_allocator(&ubench_fixture->fb, &ubench_fixture->arena,
                            &ubench_fixture->heap);
  }
}

// ============================================================================
// NEW: Extended calloc benchmarks
//
// calloc must zero-fill, so it's always slower than malloc for the same size.
// These benchmarks capture how the gap scales with size.
// ============================================================================

DEFINE_CALLOC_BENCHMARK(64b, libc_fixture, 64)
DEFINE_CALLOC_BENCHMARK(4kb, libc_fixture, 4096)
DEFINE_CALLOC_BENCHMARK(64kb, libc_fixture, 65536)
DEFINE_CALLOC_BENCHMARK(64b, buddy_fixture, 64)
DEFINE_CALLOC_BENCHMARK(4kb, buddy_fixture, 4096)
DEFINE_CALLOC_BENCHMARK(64b, gen_fixture, 64)
DEFINE_CALLOC_BENCHMARK(1kb, gen_fixture, 1024)

// ============================================================================
// NEW: Lifecycle benchmarks for extended allocator set
// ============================================================================

DEFINE_LIFECYCLE_BENCHMARK(buddy_large, {
  allo_t child, a;
  make_c_allocator(&child);
  make_buddy_allocator(&a, &child, NULL, 64 * 1024 * 1024);
  allo_destroy(&a);
  allo_destroy(&child);
})
DEFINE_LIFECYCLE_BENCHMARK(arena_large, {
  allo_t child, a;
  make_c_allocator(&child);
  make_arena_allocator(&a, &child, 256 * 1024 * 1024);
  allo_destroy(&a);
  allo_destroy(&child);
})
DEFINE_LIFECYCLE_BENCHMARK(fallback, {
  allo_t primary, fallback, fb;
  void *buf = malloc(64 * 1024);
  make_pool_allocator(&primary, NULL, buf, 64, 1000);
  make_c_allocator(&fallback);
  make_fallback_allocator(&fb, &primary, &fallback);
  allo_destroy(&fb);
  allo_destroy(&primary);
  allo_destroy(&fallback);
  free(buf);
})

// ============================================================================
// NEW: Stride / access-pattern sensitivity
//
// These benchmarks keep the allocation size constant but vary how many live
// allocations exist simultaneously before any are freed. A large stride count
// means many pointers are live at once, which stresses the allocator's ability
// to manage a large working set without excessive fragmentation.
// ============================================================================

#define DEFINE_STRIDE_BENCHMARK(FIXTURE, NAME, SIZE, LIVE)                     \
  UBENCH_F(FIXTURE, stride_##NAME##_live##LIVE) {                              \
    void *ptrs[LIVE];                                                          \
    for (int rep = 0; rep < REPEAT_COUNT / LIVE + 1; rep++) {                  \
      for (int i = 0; i < LIVE; i++) {                                         \
        ptrs[i] = allo_alloc(&ubench_fixture->a, SIZE);                        \
        ubench_do_nothing(ptrs[i]);                                            \
      }                                                                        \
      for (int i = 0; i < LIVE; i++)                                           \
        allo_free(&ubench_fixture->a, ptrs[i], SIZE);                          \
    }                                                                          \
  }

DEFINE_STRIDE_BENCHMARK(libc_fixture, 64b, 64, 1)
DEFINE_STRIDE_BENCHMARK(libc_fixture, 64b, 64, 8)
DEFINE_STRIDE_BENCHMARK(libc_fixture, 64b, 64, 32)
DEFINE_STRIDE_BENCHMARK(libc_fixture, 64b, 64, STRIDE_COUNT)

DEFINE_STRIDE_BENCHMARK(buddy_fixture, 64b, 64, 1)
DEFINE_STRIDE_BENCHMARK(buddy_fixture, 64b, 64, 8)
DEFINE_STRIDE_BENCHMARK(buddy_fixture, 64b, 64, 32)
DEFINE_STRIDE_BENCHMARK(buddy_fixture, 64b, 64, STRIDE_COUNT)

DEFINE_STRIDE_BENCHMARK(gen_fixture, 64b, 64, 1)
DEFINE_STRIDE_BENCHMARK(gen_fixture, 64b, 64, 8)
DEFINE_STRIDE_BENCHMARK(gen_fixture, 64b, 64, 32)
DEFINE_STRIDE_BENCHMARK(gen_fixture, 64b, 64, STRIDE_COUNT)

// ============================================================================
// Entry point
// ============================================================================

UBENCH_STATE();

int main(int argc, const char *const argv[]) {
  init_race_state();
  init_frag_state();
  init_workload_sizes();
  return ubench_main(argc, argv);
}
