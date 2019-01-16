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
