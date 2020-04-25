# Contributing to ashuffle

Contributions to ashuffle are always welcome! This document describes the
code standards of ashuffle and the contribution process. The goal of this
document is not to discourage contributions, but to set clear expectations
of what will and won't be merged. If you're unsure of anything in this
document, feel free to still open an issue or pull-request with a change. We
are happy to help with any issues you may have.

## Will you add support for "X"

ashuffle aims to be in the "do one thing and do it well" category of tools.
This means avoiding features that overly complicate the core functionality of
ashuffle: to shuffle the user's MPD library. Generally, this manifests as
avoiding features that could just as easily be achieved via a separate MPD
client. Here are some examples of previously accepted and rejected changes to
give you a better idea of what this means.

**Changes that were accepted:**

* Support for MPD authentication
* The `--queue-buffer` option to let crossfade users more easily use ashuffle.
* Preventing shuffle when MPD's "single" mode is on.

**Changes that would likley be accepted:**

* Shuffle by "group" (e.g., album-based or playlist-based shuffle). This is
  just a modification to ashuffle's core loop. It couldn't easily be achieved
  by another client.

**Changes that were rejected:**

* Having ashuffle prune the MPD queue to a maximum length. This could be just
  as easily achieved by another MPD client. It is orthogonal to ashuffle's
  core function.

## How to contribute

ashuffle uses the standard GitHub ["pull request"][1] model for contributions:

1. Fork the main ashuffle repository.
2. Make the changes you want in your ashuffle fork.
3. Open a pull request against ashuffle for the changes.
4. Respond to feedback by updating your change as needed.
5. Your change will be accepted and merged into ashuffle.

If you're unsure how to do any of those steps, refer to GitHub's help articles
on pull requests, and just try your best. We'll try to help with the
contribution process if you have issues. Just reach out.

## Expectations for contributed code

Beginning with ashuffle v2, ashuffle has adopted a consistent code style,
a set of comprehensive unit tests, and integration testing to ensure
compatibility with older libmpdclient and MPD versions. Your contribution
should be well formatted, tested, and have integration testing where needed.

Try not to worry too much about these requirements. Opening a pull-request
against ashuffle will cause ashuffle's formatting checks and entire automated
test suite to be run against your change. We don't expect new requests to be
perfect. Reviewers will let you know if you need to add more comprehensive
testing to your change, and help to diagnose test failures.

**Formatting:** ashuffle follows the Google formatting style for C/C++ code
with some slight tweaks (e.g., 4 spaces for indentation instead of 2). Go
code follows the canonical Go style as checked by `go fmt`. There is a
[`.clang-format`](.clang-format) in the root of this repository that works with
`clang-format`.  You can use [`scripts/format`](scripts/format) to automatically
format your C and Go code using `clang-format` and `go fmt` respectively. There
is also a format checker, [`scripts/check-format`](scripts/check-format) that
can be used as a pre-commit hook for checking that your code is well formatted.

**Unit Testing:** All code should be tested. New features should have at least
some test coverage for common usage. We will still accept patches for code
that does not test clear error cases, or cases that cannot easily be
unit-tested. Code that regresses ashuffle's existing test suite will not be
accepted. See [`t/readme.md`](t/readme.md) for a more detailed description of
ashuffle's unit tests.

**Integration Testing:** As described in
[ashuffle's compatibility statement](readme.md#mpd-version-support) ashuffle
aims to support the latest versions of libmpdclient and MPD, as well as all
versions that are used in an actively supported Ubuntu release
(including LTS releases). This means that ashuffle must support several
years of MPD and libmpdclient releases. To verify ashuffle's compatibility,
ashuffle has an integration test suite that runs against real versions of
MPD and libmpdclient. These tests are expected to continue working for all
changes. Large features should include integration tests to verify they are
not broken by new MPD/libmpdclient releases.

[1]: https://help.github.com/en/articles/about-pull-requests
