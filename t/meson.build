libtap_inc = include_directories('libtap')
libtap = library('libtap', ['libtap/tap.c'], include_directories : libtap_inc)
libtap_dep = declare_dependency(include_directories : libtap_inc,
                                link_with : libtap)

mpdfake_inc = include_directories('.')
mpdfake_dep = declare_dependency(include_directories : mpdfake_inc)
