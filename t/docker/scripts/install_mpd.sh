#!/bin/sh

set -e

PATCH_DIR=/patches
PREFIX=/usr

die() {
    echo $@ >&2
    exit 1
}

apply_patches() {
    patch_major="$1"
    patch_minor="$2"
    find "${PATCH_DIR}/${patch_major}/${patch_minor}" \
        -maxdepth 1 -type f -name '*.patch' | sort -u | while read P; do
        cat "$P" | patch -p1 || die "failed to apply patch $P"
    done
}

do_meson() {
    meson . build/release --prefix=/usr --buildtype=debugoptimized -Db_ndebug=true && \
    ninja -C build/release && \
    ninja -C build/release install
}

do_legacy() {
    ./configure --prefix="${PREFIX}" && \
    make -j && \
    make -j install
}

VERSION="$1"
MAJOR="$(echo "${VERSION}" | cut -d. -f-2)"
MINOR="$(echo "${VERSION}" | cut -d. -f3)"

# Make sure the version matches the format we expected.
if test -n "${MINOR}" && ! test "${MAJOR}.${MINOR}" = "${VERSION}"; then
    die "MAJOR.MINOR (${MAJOR}.${MINOR}) doesn't match ${VERSION}"
fi

case "$MAJOR" in
    0.19)
        # Versions before 0.19.15 don't work natively with the compiler in
        # this container. So we need to add a trivial patch.
        if test -n "${MINOR}" && test "${MINOR}" -lt 15; then
            apply_patches 0.19 pre-15
        fi
        do_legacy
        ;;
    0.20)
        do_legacy
        ;;
    0.21)
        do_meson
        ;;
    *)
        die "unrecognized major version ${MAJOR}"
esac

exit 0
