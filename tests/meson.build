test_cases = [
    'test_c',
    'test_buffer',
    'test_arena',
    'test_pool',
    'test_common',
    'test_page',
    'test_realloc',
]

# Standard tests
foreach t : test_cases
    exe = executable(
        t,
        t + '.c',
        include_directories: liballo_headers,
        link_with: liballo_lib,
    )
    test(t, exe)
endforeach

# Special nostdlib test case (Linux x86_64 only)
if host_machine.cpu_family() == 'x86_64' and host_machine.system() == 'linux'
    test_page_nostdlib_exe = executable(
        'test_page_nostdlib',
        [
            'test_page_nostdlib.c',
            '../src/allo_page.c',
            '../src/allo_utils.c',
        ],
        include_directories: liballo_headers,
        c_args: ['-DALLO_NOSTDLIB', '-ffreestanding', '-fno-stack-protector'],
        link_args: ['-nostdlib', '-static'],
        override_options: ['b_sanitize=none'],
    )
    test('test_page_nostdlib', test_page_nostdlib_exe)
endif
