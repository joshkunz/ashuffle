project('ashuffle', ['c', 'cpp'], default_options: ['cpp_std=c++17', 'warning_level=2'])

cmake = import('cmake')
cpp = meson.get_compiler('cpp')

# absl dependencies need to be explicited...
# It might be possible to use cmake dependencies (e.g. "absl:string"
# defined in abslTargets.cmake in the future but that does not seem
# worth the time trying to figure that out.
absl_libs = [
    # Via Base:
    'raw_logging_internal',

    # Via Strings:
    'int128',
    'str_format_internal',
    'strings_internal',
    'strings',

    # Via Hash:
    'hash',
    'city',
]

absl_deps = []
if not get_option('unsupported_use_system_absl')
    absl = cmake.subproject('absl', cmake_options: [
        '-DCMAKE_CXX_STANDARD=17',
    ])

    absl_deps = []
    foreach lib : absl_libs
        absl_deps += absl.dependency(lib)
    endforeach
else
    # note that the system's absl needs to be compiled for C++17 standard
    # or final link will fail.
    foreach lib : absl_libs
        dep = cpp.find_library('absl_' + lib)
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
