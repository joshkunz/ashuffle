#!/bin/sh

set -e

PREFIX=/usr

die() {
    echo $@ >&2
    exit 1
}

do_meson() {
    meson . build --prefix="${PREFIX}" && \
    ninja -C build && \
    ninja -C build install
}

do_legacy() {
    ./configure --prefix="${PREFIX}" --disable-documentation && \
    make && \
    make install
}

VERSION="$1"
MAJOR="$(echo "${VERSION}" | cut -d. -f1)"
MINOR="$(echo "${VERSION}" | cut -d. -f2)"

# Make sure the version matches the format we expected.
if ! test "${MAJOR}.${MINOR}" = "${VERSION}"; then
    die "MAJOR.MINOR (${MAJOR}.${MINOR}) doesn't match ${VERSION}"
fi

if ! test "${MAJOR}" = "2"; then
    die "Only major version == 2 is supported."
fi

if test "${MINOR}" -eq 12; then
    die "$VERSION unsupported. 2.12 doesn't support newer meson installs."
elif test "${MINOR}" -gt 12; then
    # Meson was introduced in 2.12. So for all versions after that, build
    # using meson.
    do_meson
else
    do_legacy
fi

exit 0
