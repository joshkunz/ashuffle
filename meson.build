project('ashuffle', 'c', version: '1.0.3', default_options: ['c_std=c99'])

add_global_arguments([
	'-Wextra',
	'-pedantic' ],
	language: 'c'
)

sources = [
	'src/args.c',
	'src/getpass.c',
	'src/list.c',
	'src/rule.c',
	'src/shuffle.c',
    'src/util.c',
]

clientlib = dependency('libmpdclient')

ashuffle = executable(
	'ashuffle',
	sources + ['src/ashuffle.c'],
	dependencies: clientlib,
	install: true
)

subdir('t')

test_c_args = [
    '-Wall', '-Wextra', '-pedantic',
]

test_options = [
    'werror=true',
    'b_sanitize=address',
    'b_sanitize=undefined',
]

src_inc = include_directories('src')

tests = {
    'list': ['t/list_test.c'],
    'shuffle': ['t/shuffle_test.c'],
    'rule': ['t/rule_test.c'],
    'args': ['t/args_test.c'],
}

foreach test_name, test_sources : tests
    test_exe = executable(
        test_name + '_test',
        sources + test_sources,
        include_directories : src_inc,
        dependencies : [libtap_dep, testhelpers_dep, clientfake_dep],
        c_args : test_c_args,
        override_options : test_options,
    )
    test(test_name, test_exe)
endforeach
