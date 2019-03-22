project('ashuffle', 'c', version: '1.0.3', default_options: ['c_std=c99'])

add_global_arguments([
	'-Wextra',
	'-pedantic' ],
	language: 'c'
)

sources = [
	'src/args.c',
	'src/ashuffle.c',
	'src/getpass.c',
	'src/list.c',
	'src/rule.c',
	'src/shuffle.c'
]

clientlib = dependency('libmpdclient')

ashuffle = executable(
	'ashuffle',
	sources,
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
list_test = executable('list_test',
                       ['src/list.c', 't/list_test.c'],
                       include_directories : src_inc,
                       dependencies : libtap_dep,
                       c_args : test_c_args,
                       override_options : test_options)
test('list', list_test)

shuffle_test = executable('shuffle_test',
                          ['src/list.c', 'src/shuffle.c', 't/shuffle_test.c'],
                          include_directories : src_inc,
                          dependencies : libtap_dep,
                          c_args : test_c_args,
                          override_options : test_options)
test('shuffle', shuffle_test)

clientlib_inc = clientlib.partial_dependency(includes : true)
rule_test = executable('rule_test',
                       ['src/list.c', 'src/rule.c', 't/mpdclient_fake.c', 't/rule_test.c'],
                       include_directories : src_inc,
                       dependencies : [libtap_dep, clientlib_inc],
                       c_args : test_c_args,
                       override_options : test_options)
test('shuffle', shuffle_test)

