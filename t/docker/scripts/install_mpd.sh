#!/bin/sh

set -e

SRC="$( dirname $(readlink -f "$0") )"
. "$SRC/common.sh"

MAKE_PARALLEL_JOBS=16

PATCH_DIR=/patches
PREFIX=/usr
ROOT=/opt/mpd
GIT_URL="https://github.com/MusicPlayerDaemon/MPD.git"

apply_patches() {
    patch_major="$1"
    patch_minor="$2"
    echo "Applying patches from ${patch_major}/${patch_minor}..."
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
    errlog="$(tempfile)"
    test -f "${errlog}" || die "couldn't create error log"

    echo "Configuring mpd..."
    ./configure --quiet --enable-silent-rules --prefix="${PREFIX}" && \
    make -j "${MAKE_PARALLEL_JOBS}" 2>>"${errlog}" && \
    make -j "${MAKE_PARALLEL_JOBS}" install 2>>"${errlog}"

    status="$?"
    if test "${status}" -ne 0; then
        cat config.log
        echo "Error log:"
        cat "${errlog}"
    fi
    make -j clean >/dev/null 2>&1
    rm "${errlog}"
    return "${status}"
}

VERSION="$1"
if test "${VERSION}" = "latest"; then
    latest_version="$(latest_version "${GIT_URL}")"
    diag "VERSION=latest, using VERSION=${latest_version}"
    VERSION="${latest_version}"
fi

MAJOR="$(echo "${VERSION}" | cut -d. -f-2)"
MINOR="$(echo "${VERSION}" | cut -d. -f3)"

# Make sure the version matches the format we expected.
if test -n "${MINOR}" && ! test "${MAJOR}.${MINOR}" = "${VERSION}"; then
    die "MAJOR.MINOR (${MAJOR}.${MINOR}) doesn't match ${VERSION}"
fi

mkdir -p "${ROOT}"
test -d "${ROOT}" || die "build root '${ROOT}' not a valid directory"
cd "${ROOT}"

echo "Fetching mpd-${VERSION}..."
url="http://www.musicpd.org/download/mpd/${MAJOR}/mpd-${VERSION}.tar.xz"
wget -q -O- "${url}" | tar --strip-components=1 -xJ
if test "$?" -ne 0; then
    die "failed to fetch mpd-${VERSION} sources"
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
