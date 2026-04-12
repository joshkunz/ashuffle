# vim: set sw=2 ts=2:
project(
  'ashuffle',
  ['c', 'cpp'],
  version: 'v3.14.10',
  default_options: ['cpp_std=c++17', 'warning_level=2']
)

add_global_arguments(
  [
    '-Werror=switch',
    '-fno-omit-frame-pointer',
  ],
  language : ['c', 'cpp'],
)

# Figure out if a linker option for std::filesystem is needed. Older versions
# of clang (before Clang 9.0) and GCC (before 9.1) required linking a special
# "c++fs" library.
stdfs_deps = []

cxx_compiler = meson.get_compiler('cpp')

stdfs_test = '''
#include <filesystem>
int main(void) { return std::filesystem::exists("/foo"); }
'''

if cxx_compiler.links(stdfs_test, name: 'std::fs links with only stdlib')
  # then we don't need to do anything.
elif cxx_compiler.links(stdfs_test, args: ['-lc++fs'], name: 'std::fs links with -lc++fs')
  stdfs_deps += [declare_dependency(link_args: ['-lc++fs'])]
elif cxx_compiler.links(stdfs_test, args: ['-lstdc++fs'], name: 'std::fs links with -lstdc++fs')
  stdfs_deps += [declare_dependency(link_args: ['-lstdc++fs'])]
else
  error('Could not auto-detect how to link std::filesystem. Build configuration unsupported')
endif

cmake = import('cmake')

absl_deps = []
if not get_option('unsupported_use_system_absl')
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

  absl_opts = cmake.subproject_options()
  absl_opts.add_cmake_defines({
    'CMAKE_CXX_STANDARD': '17',
    # See above.
    'CMAKE_PROJECT_absl_INCLUDE': absl_project_inc,
    # Absl's CMAKE build feels like it is MinGW for some reason when building
    # under Meson. Explicitly tell CMAKE it is not MinGW.
    'MINGW': 'false',
    # Use new linker policy to avoid warnings.
    'CMAKE_POLICY_DEFAULT_CMP0156': 'NEW',
    'CMAKE_POLICY_DEFAULT_CMP0179': 'NEW',
    # Use new language version policy to avoid warnings.
    'CMAKE_POLICY_DEFAULT_CMP0128': 'NEW',
  })

  absl = cmake.subproject('absl', options: absl_opts)

  # Use target_list() instead of a hand-maintained list so this doesn't need
  # updating when absl adds or removes internal targets. Static linking means
  # the linker only pulls in symbols that are actually referenced, so unused
  # targets don't affect binary size.
  foreach t : absl.target_list()
    # cmake auto-generates CTest dashboard targets (Experimental, Nightly,
    # Continuous, etc.) that appear in target_list() as custom_targets. These
    # don't support dependency(), so filter to absl_ prefixed targets only.
    if not t.startswith('absl_')
      continue
    endif
    # absl defines some test-only libraries (e.g. absl_*_test_util) that are
    # not needed at runtime and may not build correctly outside absl's own
    # test suite.
    if t.contains('_test')
      continue
    endif
    absl_deps += absl.dependency(t)
  endforeach
else
  cpp = meson.get_compiler('cpp')

  # note that the system's absl needs to be compiled for C++17 standard
  # or final link will fail.
  foreach lib : absl_libs
    dep = dependency(lib)
    if dep.found()
      absl_deps += dep
    endif
  endforeach
endif

if not get_option('unsupported_use_system_yamlcpp')
  yaml_cpp_opts = cmake.subproject_options()
  yaml_cpp_opts.add_cmake_defines({
    'CMAKE_CXX_STANDARD': '17',
    'YAML_BUILD_SHARED_LIBS': 'OFF',
    # Use new linker policy to avoid warnings.
    'CMAKE_POLICY_DEFAULT_CMP0156': 'NEW',
    'CMAKE_POLICY_DEFAULT_CMP0179': 'NEW',
  })

  yaml_cpp = cmake.subproject('yaml-cpp', options: yaml_cpp_opts).dependency('yaml-cpp')
else
  yaml_cpp = dependency('yaml-cpp')
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
  'src/args.cc',
  'src/ashuffle.cc',
  'src/getpass.cc',
  'src/load.cc',
  'src/log.cc',
  'src/rule.cc',
  'src/shuffle.cc',
)

executable_sources = files('src/mpd_client.cc', 'src/main.cc')

libashuffle = static_library(
  'ashuffle',
  sources,
  include_directories: src_inc,
  dependencies: absl_deps + [yaml_cpp, libmpdclient],
)

ashuffle = executable(
  'ashuffle',
  executable_sources,
  dependencies: stdfs_deps + absl_deps + [libmpdclient],
  link_with: [libashuffle, libversion],
  install: true,
)

fs = import('fs')

if fs.exists('scripts/run-clang-tidy')
  # In the integration test container, we don't supply this script for caching
  # reasons, but this fails the build. So we make this target conditional
  # on the script being present.
  clang_tidy = run_target('ashuffle-clang-tidy',
    command : files('scripts/run-clang-tidy') + sources + executable_sources
  )
endif # clang-tidy

if get_option('tests').enabled()

  if not get_option('unsupported_use_system_gtest')
    googletest_opts = cmake.subproject_options()
    googletest_opts.add_cmake_defines({
      'BUILD_GMOCK': 'ON',
      'CMAKE_CXX_STANDARD': '17',
      'INSTALL_GTEST': 'OFF',
      # Use new linker policy to avoid warnings.
      'CMAKE_POLICY_DEFAULT_CMP0156': 'NEW',
      'CMAKE_POLICY_DEFAULT_CMP0179': 'NEW',
    })

    googletest = cmake.subproject('googletest', options: googletest_opts)

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
    'args': ['t/args_test.cc'],
    'ashuffle': ['t/ashuffle_test.cc'],
    'load': ['t/load_test.cc'],
    'log': ['t/log_test.cc'],
    'mpd_fake': ['t/mpd_fake_test.cc'],
    'rule': ['t/rule_test.cc'],
    'shuffle': ['t/shuffle_test.cc'],
  }

  foreach test_name, test_sources : tests
    test_exe = executable(
      test_name + '_test',
      test_sources,
      include_directories : src_inc,
      link_with: libashuffle,
      dependencies : stdfs_deps + absl_deps + gtest_deps + [mpdfake_dep],
      override_options : test_options,
    )
    test(test_name, test_exe)
  endforeach

endif # tests feature
