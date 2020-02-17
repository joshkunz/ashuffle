project('ashuffle', ['c', 'cpp'], default_options: ['cpp_std=c++17', 'warning_level=2'])

high_dep_sources = files(
    'src/ashuffle.cc',
)

low_dep_sources = files(
	'src/args.cc',
	'src/getpass.cc',
	'src/list.cc',
	'src/rule.cc',
    'src/shuffle.cc',
    'src/util.cc',
)

sources = low_dep_sources + high_dep_sources

executable_sources = sources + files('src/main.cc')

src_inc = include_directories('src')
root_inc = include_directories('.')

clientlib = dependency('libmpdclient')

ashuffle = executable(
	'ashuffle',
	executable_sources,
	dependencies: clientlib,
	install: true
)

clang_tidy = run_target('ashuffle-clang-tidy',
    command : files('scripts/run-clang-tidy') + executable_sources
)

# We only generate tests if we are *not running in tidy mode
if get_option('tests').enabled()

subdir('t')

test_c_args = [
]

test_options = [
    'werror=true',
    'b_sanitize=address',
    'b_sanitize=undefined',
]

tests = {
    'args': ['t/args_test.cc'],
    'ashuffle': ['src/ashuffle.cc', 't/ashuffle_test.cc'],
    'list': ['t/list_test.cc'],
    'rule': ['t/rule_test.cc'],
    'shuffle': ['t/shuffle_test.cc'],
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

endif # tests feature
