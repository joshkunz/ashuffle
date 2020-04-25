# ashuffle testing

This directory (`./t`) contains all of the test sources used to test ashuffle.
To validate that ashuffle is working correctly, it has a comprehensive battery
of unit tests, and integration tests. These tests are checked by a continuous
integration system (travis-ci) to ensure that ashuffle stays healthy. New
ashuffle contributions should include comprehensive testing.

This document describes how the various ashuffle tests work.

## unit testing

The first line of defense from software defects is ashuffle's unit test suite.
These tests run against only ashuffle, not libmpdclient. They attempt to
target individual ashuffle subsystems like "shuffle", or "rule". You can run the
unit tests like so:

    meson -Dtests=enabled build
    ninja -C build test 

Unit tests are located in the root of the testing directory. All unit-tests
are writing using googletest (sometimes know as gtest) and googlemock. These
libraries are fairly popular for C++ code, so you may already be familiar
with them. If you're not, you can find documentation for them on the
[googletest github page](https://github.com/google/googletest). If you're
not familiar with the framework, you can also try copying and tweaking an
existing test.

ashuffle uses a C++ wrapper (`src/mpd.h`) to interact with `libmpdclient`.
This wrapper has a "real" implementation (`src/mpd_client.{h,cc}`) that
proxies to the libmpdclient API, and a "fake" implementation (`t/mpdfake.h`).
ashuffle itself is written against the "generic" API exposed by `src/mpd.h`, so
it's easy to inject fake dependencies (MPD connection, songs, etc.)
from `t/mpdfake.h` when needed.

As part of ashuffle's continuous integration testing, these unit tests are also
run under Clang's AddressSanitizer, and MemorySanitizer to check for leaks,
and other invalid memory accesses. This somewhat non-trivial to run locally,
and will be run automatically when a pull-request is opened against ashuffle.
If you want to run the sanitizers locally, take a look at
`/scripts/travis/unit-test`.

## integration testing 

Since ashuffle's unit-tests are run against fake implementations, additional
work is needed to verify that `ashuffle` works with real libmpdclient and MPD
implementations. ashuffle integration
tests use a real libmpdclient, and a real MPD instance (both built from source).
A Go test harness is used to run MPD, and ashuffle, and also verify that
ashuffle takes the appropriate action for the situation. To increase
reproducibility of these tests, we run them entirely within a docker container.
Since we build MPD and libmpdclient from source for each run, it's easy to
test against different combinations of libmpdclient/MPD version. This allows
us to make sure that ashuffle stays compatible with older libmpdclient and
MPD releases.

To run the tests, you can use the `run-integration` script:

    scripts/run-integration

which will test your local version of ashuffle against the lastest MPD and
libmpdclient. You can use the `--mpd-version`, and `--libmpdclient-version`
flags to select other MPD or libmpdclient versions.

The integration test harness, and tests themselves are stored in
`./integration`. The Dockerfile, test runner, and other docker helpers are
stored in `./docker`. Some static files used in the docker containers are
stored in `./static`.

A substantial portion of the integration test time comes from building the
"root" docker image. The "root" image contains the buildtools needed to build
ashuffle/libmpdclient/MPD, as well as a substantial music library
(20,000 tracks) that can be used for basic load-testing. This root image is
built separately, and stored on docker-hub: [jkz0/ashuffle-integration-root
](https://hub.docker.com/r/jkz0/ashuffle-integration-root). This is purely
for convenience, the Dockerfile used to build the image is developed in
[joshkunz/ashuffle-integration-root
](https://github.com/joshkunz/ashuffle-integration-root), and can be built
independently if so desired.

On my machine, using a pre-built root image, and a fully cached integration
test container, the integration tests run in ~30s. An uncached run, with
a pre-built root image takes ~60s.
