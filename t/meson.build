libtap_inc = include_directories('libtap')
libtap = library('libtap', ['libtap/tap.c'], include_directories : libtap_inc)
libtap_dep = declare_dependency(include_directories : libtap_inc,
                                link_with : libtap)

clientlib_inc_dep = libmpdclient.partial_dependency(includes : true)
clientfake_inc = include_directories('.')
clientfake = static_library(
    'clientfake',
    ['mpdclient_fake.cc'],
    include_directories : [clientfake_inc, root_inc],
    dependencies : absl_all + [clientlib_inc_dep],
)
clientfake_dep = declare_dependency(include_directories : [clientfake_inc, root_inc],
                                    link_with: clientfake,
                                    dependencies: clientlib_inc_dep)

testhelpers_inc = include_directories('.')
testhelpers_dep = declare_dependency(include_directories : testhelpers_inc)

