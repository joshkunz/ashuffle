#!/bin/sh

set -e

SRC="$( dirname $(readlink -f "$0") )"
. "$SRC/common.sh"

MAKE_PARALLEL_JOBS=16

PREFIX=/usr
ROOT="/opt/libmpdclient"
GIT_URL="https://github.com/MusicPlayerDaemon/libmpdclient.git"

do_meson() {
    meson . build --prefix="${PREFIX}" && \
    ninja -C build && \
    ninja -C build install
}

do_legacy() {
    errlog="$(tempfile)"
    test -f "${errlog}" || die "couldn't create error log"

    echo "Configuring libmpdclient..."
    # Actual build
    ./configure --quiet --enable-silent-rules \
        --prefix="${PREFIX}" --disable-documentation && \
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

MAJOR="$(echo "${VERSION}" | cut -d. -f1)"
MINOR="$(echo "${VERSION}" | cut -d. -f2)"

# Make sure the version matches the format we expected.
if ! test "${MAJOR}.${MINOR}" = "${VERSION}"; then
    die "MAJOR.MINOR (${MAJOR}.${MINOR}) doesn't match ${VERSION}"
fi

if ! test "${MAJOR}" = "2"; then
    die "Only major version == 2 is supported."
fi

mkdir -p "${ROOT}"
test -d "${ROOT}" || die "build root '${ROOT}' not a valid directory"
cd "${ROOT}"

echo "Fetching libmpdclient-${VERSION}..."
url="https://www.musicpd.org/download/libmpdclient/${MAJOR}/libmpdclient-${VERSION}.tar.xz"
wget -q -O- "${url}" | tar --strip-components=1 -xJ
if test "$?" -ne 0; then
    die "failed to download and extract libmdclient-${VERSION}"
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
