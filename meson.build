project('ashuffle', 'c', default_options: ['c_std=c11'])

add_global_arguments([
	'-Wextra',
	'-pedantic' ],
	language: 'c'
)

high_dep_sources = [
    'src/ashuffle.c',
]

low_dep_sources = [
	'src/args.c',
	'src/getpass.c',
	'src/list.c',
	'src/rule.c',
    'src/shuffle.c',
    'src/util.c',
]

sources = low_dep_sources + high_dep_sources

executable_sources = sources + ['src/main.c']

src_inc = include_directories('src')
root_inc = include_directories('.')

clientlib = dependency('libmpdclient')

ashuffle = executable(
	'ashuffle',
	executable_sources,
	dependencies: clientlib,
	install: true
)

if get_option('tidy_mode')
    clang_tidy = run_target('clang-tidy',
        command : ['clang-tidy',
                   '-p', meson.build_root(),
                  ] + executable_sources,
    )
endif

# We only generate tests if we are *not running in tidy mode
if not get_option('tidy_mode')

subdir('t')

test_c_args = [
    '-Wall', '-Wextra', '-pedantic',
]

test_options = [
    'werror=true',
    'b_sanitize=address',
    'b_sanitize=undefined',
]

tests = {
    'args': ['t/args_test.c'],
    'ashuffle': ['src/ashuffle.c', 't/ashuffle_test.c'],
    'list': ['t/list_test.c'],
    'rule': ['t/rule_test.c'],
    'shuffle': ['t/shuffle_test.c'],
}

foreach test_name, test_sources : tests
    test_exe = executable(
        test_name + '_test',
        low_dep_sources + test_sources,
        include_directories : src_inc,
        dependencies : [libtap_dep, testhelpers_dep, clientfake_dep],
        c_args : test_c_args,
        override_options : test_options,
    )
    test(test_name, test_exe)
endforeach

endif # tidy_mode
