liballo_base_sources = [
    'allo.c',
    'allo_buffer.c',
    'allo_arena.c',
    'allo_pool.c',
    'allo_buddy.c',
    'allo_mem.c',
    'allo_fallback.c',
]

liballo_hosted_sources = liballo_base_sources + [
    'allo_libc.c',
    'allo_mtx.c',
    'allo_page.c',
    'allo_gen.c',
]

if not opt_freestanding
    liballo_lib = library(
        'allo',
        liballo_hosted_sources,
        include_directories: liballo_headers,
        dependencies: threads_dep,
        c_args: ['-D_GNU_SOURCE'],
        install: true,
    )

    pkg = import('pkgconfig')
    pkg.generate(
        libraries: liballo_lib,
        subdirs: ['.'],
        version: meson.project_version(),
        name: 'liballo',
        filebase: 'liballo',
        description: 'Lightweight, extensible memory allocator library',
    )
else
    liballo_static = static_library(
        'allo_static',
        liballo_base_sources,
        include_directories: liballo_headers,
        link_args: ['-nostdlib'],
        c_args: ['-DALLO_FREESTANDING', '-ffreestanding'],
        override_options: ['b_sanitize=none', 'b_coverage=false'],
        install: false,
    )
endif

if not opt_freestanding
    liballo = declare_dependency(
        link_with: liballo_lib,
        include_directories: liballo_headers,
        dependencies: threads_dep,
    )
else
    liballo = declare_dependency(
        link_with: liballo_static,
        include_directories: liballo_headers,
    )
endif

