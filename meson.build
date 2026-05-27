project(
    'liballo',
    'c',
    meson_version: '>= 1.3.0',
    version: '0.1',
    default_options: [
        'warning_level=3',
        'c_std=c11',
    ],
)

cc = meson.get_compiler('c')

opt_freestanding = get_option('freestanding')

is_afl = cc.get_id() == 'clang' and cc.cmd_array()[0].contains('afl-clang')
can_do_nostdlib = not is_afl and host_machine.cpu_family() == 'x86_64' and host_machine.system() == 'linux'

if opt_freestanding and not can_do_nostdlib
    error('Freestanding builds require x86_64 Linux and a non-AFL compiler.')
endif

if not opt_freestanding
    threads_dep = dependency('threads', required: not get_option('freestanding'))
endif

liballo_headers = include_directories('include')
install_headers('include/allo.h', subdir: 'liballo')

subdir('src')
subdir('tests')

if get_option('benchmarks')
    subdir('benchmarks')
endif

if get_option('examples') and not opt_freestanding
    subdir('examples')
endif
