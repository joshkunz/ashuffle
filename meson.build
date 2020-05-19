project('ashuffle', ['c', 'cpp'], default_options: ['cpp_std=c++17', 'warning_level=2'])

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
    'absl_city',
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

sources = files(
    'src/ashuffle.cc',
    'src/load.cc',
    'src/args.cc',
    'src/getpass.cc',
    'src/rule.cc',
    'src/shuffle.cc',
)

executable_sources = sources + files('src/mpd_client.cc', 'src/main.cc')

src_inc = include_directories('src')
root_inc = include_directories('.')

ashuffle = executable(
    'ashuffle',
    executable_sources,
    dependencies: absl_deps + [libmpdclient],
    install: true
)

clang_tidy = run_target('ashuffle-clang-tidy',
    command : files('scripts/run-clang-tidy') + executable_sources
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
            sources + test_sources,
            include_directories : src_inc,
            dependencies : absl_deps + gtest_deps + [mpdfake_dep],
            override_options : test_options,
        )
        test(test_name, test_exe)
    endforeach

endif # tests feature
