#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "allo.h"

#define MAX_POINTERS 16
#define MAX_ALLOC_SIZE 8192

typedef enum {
    OP_ALLOC = 0,
    OP_FREE = 1,
    OP_REALLOC = 2,
    OP_COUNT
} operation_t;

typedef struct {
    void *ptr;
    size_t size;
} tracker_t;

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    if (Size < 2) return 0;

    allo_t a;
    allo_t child;
    void *backing_buffer = NULL;
    int is_complex = 0; // tracking if we need to destroy child/buffer

    uint8_t selector = Data[0] % 5;
    size_t offset = 1;

    switch (selector) {
        case 0: // C Allocator
            a = make_c_allocator();
            break;
        case 1: // Page Allocator
            a = make_page_allocator();
            break;
        case 2: // Fixed Buffer Allocator
            backing_buffer = malloc(MAX_ALLOC_SIZE * MAX_POINTERS);
            a = make_fixed_buf_allocator(backing_buffer, MAX_ALLOC_SIZE * MAX_POINTERS);
            is_complex = 1;
            break;
        case 3: // Arena Allocator
            child = make_c_allocator();
            a = make_arena_allocator(&child, 4096);
            is_complex = 2;
            break;
        case 4: // Pool Allocator
            child = make_c_allocator();
            // Use 64-byte blocks for the pool fuzzer
            a = make_pool_allocator(&child, NULL, 64, 1024);
            is_complex = 3;
            break;
        default:
            return 0;
    }

    tracker_t tracked[MAX_POINTERS];
    memset(tracked, 0, sizeof(tracked));

    while (offset < Size) {
        uint8_t op_byte = Data[offset++];
        operation_t op = (op_byte & 0x03) % OP_COUNT;
        uint8_t index = (op_byte >> 2) % MAX_POINTERS;

        if (op == OP_ALLOC) {
            if (offset + 2 > Size) break;
            size_t alloc_size = ((Data[offset] << 8) | Data[offset+1]) % MAX_ALLOC_SIZE;
            offset += 2;

            if (tracked[index].ptr) {
                allo_free(&a, tracked[index].ptr);
            }
            tracked[index].ptr = allo_alloc(&a, alloc_size);
            tracked[index].size = alloc_size;
            if (tracked[index].ptr && alloc_size > 0) {
                memset(tracked[index].ptr, 0xAA, alloc_size);
            }
        } else if (op == OP_FREE) {
            if (tracked[index].ptr) {
                allo_free(&a, tracked[index].ptr);
                tracked[index].ptr = NULL;
                tracked[index].size = 0;
            }
        } else if (op == OP_REALLOC) {
            if (offset + 2 > Size) break;
            size_t new_size = ((Data[offset] << 8) | Data[offset+1]) % MAX_ALLOC_SIZE;
            offset += 2;

            void *new_ptr = allo_realloc(&a, tracked[index].ptr, tracked[index].size, new_size);
            if (new_ptr || new_size == 0) {
                tracked[index].ptr = new_ptr;
                tracked[index].size = new_size;
                if (tracked[index].ptr && new_size > 0) {
                    memset(tracked[index].ptr, 0xBB, new_size);
                }
            }
        }
    }

    for (int i = 0; i < MAX_POINTERS; i++) {
        if (tracked[i].ptr) {
            allo_free(&a, tracked[i].ptr);
        }
    }

    allo_destroy(&a);
    if (is_complex == 1) {
        free(backing_buffer);
    } else if (is_complex >= 2) {
        allo_destroy(&child);
    }

    return 0;
}
