freestanding_test_cases = [
    'test_buffer',
    'test_arena',
    'test_pool',
    'test_common',
    'test_buddy',
    'test_fallback',
]

hosted_test_cases = freestanding_test_cases + [
    'test_page',
    'test_realloc',
    'test_calloc',
    'test_mtx',
    'test_c',
]

if opt_freestanding
    foreach t : freestanding_test_cases
        exe = executable(
            t + '_nostdlib',
            [t + '.c', 'nolibc_start.c'],
            dependencies: liballo,
            c_args: [
                '-DALLO_FREESTANDING',
                '-fno-stack-protector',
            ],
            link_args: ['-nostdlib', '-static'],
            override_options: ['b_sanitize=none', 'b_coverage=false'],
        )
        test(t + '_nostdlib', exe)
    endforeach
else
    foreach t : hosted_test_cases
        exe = executable(
            t,
            t + '.c',
            dependencies: liballo,
        )
        test(t, exe)
    endforeach

    afl = find_program('afl-fuzz', required: false)
    if afl.found()
        executable('fuzz_allocator', 'fuzz_allocator.c', dependencies: [liballo])
    endif
endif
