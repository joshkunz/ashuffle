# vim: set sw=2 ts=2:
project(
  'ashuffle',
  ['c', 'cpp'],
  version: 'v3.11.1',
  default_options: ['cpp_std=c++17', 'warning_level=2']
)

add_global_arguments(
  [
    '-Werror=switch',
    '-fno-omit-frame-pointer',
  ],
  language : ['c', 'cpp'],
)

# absl dependencies need to be explicited...
# It might be possible to use cmake dependencies (e.g. "absl:string"
# defined in abslTargets.cmake in the future but that does not seem
# worth the time trying to figure that out.
absl_libs = [
  # Via Base:
  'absl_raw_logging_internal',

  # Via Strings:
  'absl_int128',
  'absl_str_format_internal',
  'absl_strings_internal',
  'absl_strings',

  # Via Hash:
  'absl_hash',
  'absl_wyhash',
  'absl_city',

  # Via Time:
  'absl_time',
  'absl_base',
  'absl_spinlock_wait',
]

absl_deps = []
if not get_option('unsupported_use_system_absl')
  cmake = import('cmake')

  # HACK: absl detects if it's being built in "system" mode, or "subproject"
  # mode depending on the cmake PROJECT_SOURCE_DIR variable. Since meson
  # parses the cmake package info in isolation, absl assumes that it is in
  # "system" mode and generates install rules that meson propogates to the
  # library targets by setting the `install` attribute. Since we want absl
  # to remain internal, we hack this check by forcing the PROJECT_SOURCE_DIR
  # to match the true source root. This is done by using
  # CMAKE_PROJECT_..._INCLUDE to inject a cmake snippet after absl's
  # invocation of `project()` to update PROJECT_SOURCE_DIR.
  absl_project_inc = join_paths(meson.current_source_dir(), 'tools/cmake/inject_project_source_dir.cmake')

  absl = cmake.subproject('absl', cmake_options: [
    '-DCMAKE_CXX_STANDARD=17',
    '-DCMAKE_PROJECT_absl_INCLUDE=' + absl_project_inc,
  ])

  absl_deps = []
  foreach lib : absl_libs
    absl_deps += absl.dependency(lib)
  endforeach
else
  cpp = meson.get_compiler('cpp')

  # note that the system's absl needs to be compiled for C++17 standard
  # or final link will fail.
  foreach lib : absl_libs
    dep = cpp.find_library(lib)
    if dep.found()
      absl_deps += dep
    endif
  endforeach
endif
libmpdclient = dependency('libmpdclient')

src_inc = include_directories('src')

version_cc = configure_file(
  configuration: {
    'VERSION': meson.project_version(),
  },
  input: 'src/version.cc.in',
  output: 'version.cc',
)

libversion = static_library(
  'version',
  [version_cc],
  include_directories: src_inc,
)

sources = files(
  'src/ashuffle.cc',
  'src/load.cc',
  'src/args.cc',
  'src/getpass.cc',
  'src/rule.cc',
  'src/shuffle.cc',
)

executable_sources = files('src/mpd_client.cc', 'src/main.cc')

libashuffle = static_library(
  'ashuffle',
  sources,
  include_directories: src_inc,
  dependencies: absl_deps,
)

ashuffle = executable(
  'ashuffle',
  executable_sources,
  dependencies: absl_deps + [libmpdclient],
  link_with: [libashuffle, libversion],
  install: true,
)

clang_tidy = run_target('ashuffle-clang-tidy',
  command : files('scripts/run-clang-tidy') + sources + executable_sources
)

if get_option('tests').enabled()

  if not get_option('unsupported_use_system_gtest')
    googletest = cmake.subproject('googletest', cmake_options: [
      '-DBUILD_GMOCK=ON',
      '-DCMAKE_CXX_STANDARD=17',
      '-DINSTALL_GTEST=OFF',
    ])

    gtest_deps = [
      dependency('threads'),
      googletest.dependency('gtest'),
      googletest.dependency('gmock'),
      googletest.dependency('gmock_main'),
    ]
  else
    gtest_deps = [
      dependency('threads'),
      dependency('gtest', version: '>=1.10'),
      dependency('gmock', version: '>=1.10'),
      dependency('gmock_main', version: '>=1.10'),
    ]
  endif

  mpdfake_inc = include_directories('t')
  mpdfake_dep = declare_dependency(include_directories : mpdfake_inc)

  test_options = [
    'werror=true',
  ]

  tests = {
    'rule': ['t/rule_test.cc'],
    'shuffle': ['t/shuffle_test.cc'],
    'load': ['t/load_test.cc'],
    'args': ['t/args_test.cc'],
    'ashuffle': ['t/ashuffle_test.cc'],
  }

  foreach test_name, test_sources : tests
    test_exe = executable(
      test_name + '_test',
      test_sources,
      include_directories : src_inc,
      link_with: libashuffle,
      dependencies : absl_deps + gtest_deps + [mpdfake_dep],
      override_options : test_options,
    )
    test(test_name, test_exe)
  endforeach

endif # tests feature
