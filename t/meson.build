libtap_inc = include_directories('libtap')
libtap = library('libtap', 'libtap/tap.c', include_directories : libtap_inc)
libtap_dep = declare_dependency(include_directories : libtap_inc, link_with : libtap)

testhelpers_inc = include_directories('.')
testhelpers_dep = declare_dependency(include_directories : testhelpers_inc)

clientlib_inc_dep = clientlib.partial_dependency(includes : true)
clientfake_inc = include_directories('.')
clientfake = library('clientfake',
                     ['mpdclient_fake.c', '../src/util.c', '../src/list.c'],
                     include_directories : [clientfake_inc, root_inc],
                     dependencies: clientlib_inc_dep)
clientfake_dep = declare_dependency(include_directories : clientfake_inc, link_with: clientfake, dependencies: clientlib_inc_dep)
