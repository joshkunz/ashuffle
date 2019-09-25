# ashuffle testing

This directory (`./t`) contains all of the test sources used to test ashuffle.
To validate that ashuffle is working correctly, it has a comprehensive battery
of unit tests, and integration tests. These tests are checked by a continuous
integration system (travis-ci) to ensure that ashuffle stays healthy. New
ashuffle contributions should include comprehensive testing.

This document describes how the various ashuffle tests work.

## unit testing

The first line of defense from software defects is ashuffle's unit test suite.
These tests run against only ashuffle sources (though they do depend on
libmpdclient headers), and attempt to target individual ashuffle subsystems
like "list", or "rule". You can run the unit tests like so:

    meson -Dtests=enabled build
    ninja -C build test 

Unit tests are located in the root of the testing directory. All unit-tests
are written using [libtap](https://github.com/zorgnax/libtap). `libtap` is
is fairly lightweight, and has comprehensive documentation. However, if you're
interested in writing a test, it's probably easiest to copy and modify an
existing test.

To allow for unit-testing of ashuffle independent of libmpdclient (the MPD
client library used by ashuffle), ashuffle has a "fake" libmpdclient library
"mpdclient_fake". This library is linked into subsystems that depend on
libmpdclient (like `ashuffle.c`). It has hooks for controlling how the fake
libmpdclient functions behave defined in `mpdclient_fake.h`. `helpers.{c,h}`
is a test helper library with some routines that are common to several tests.

As part of ashuffle's continuous integration testing, these unit tests are also
run under Clang's AddressSanitizer, and MemorySanitizer to check for leaks,
and other invalid memory accesses. This somewhat non-trivial to run locally,
and will be run automatically when a pull-request is opened against ashuffle.
If you want to run the sanitizers locally, take a look at
`/scripts/travis/unit-test`.

## integration testing 

To ensure that ashuffle is compatible with a range of MPD and libmpdclient
releases, we also have an integration testing system. ashuffle integration
tests use a real libmpdclient, and a real MPD instance (both built from source).
A Go test harness is used to run MPD, and ashuffle, and also verify that
ashuffle takes the appropriate action for the situation. To increase
reproducibility of these tests, we run them entirely within a docker container.

To run the tests, you can use the `run-integration` script:

    scripts/run-integration

which will test your local version of ashuffle against the lastest MPD and
libmpdclient. You can use the `--mpd-version`, and `--libmpdclient-version`
flags to select other MPD or libmpdclient versions.

The integration test harness, and tests themselves are stored in
`./integration`. The Dockerfile, test runner, and other docker helpers are
stored in `./docker`. Some static files used in the docker containers are
stored in `./static`.
