#!/bin/sh

BINDIR=/usr/bin
ROOT=/opt/go

die() {
    echo "$@" >&2
    exit 1
}

mkdir -p "${ROOT}"
test -d "${ROOT}" || die "build root '${ROOT}' not a valid directory"
cd "${ROOT}"

VERSION="$1"

test -n "${VERSION}" || die "go version is empty, invalid version"

url="https://dl.google.com/go/go${VERSION}.linux-amd64.tar.gz"
wget -q -O- "${url}" | tar --strip-components=1 -xzv
if test "$?" -ne 0; then
    die "couldn't download and extract go-${VERSION} source"
fi

ln -s "${ROOT}/bin/go" "${BINDIR}/" || die "couldn't symlink go"
ln -s "${ROOT}/bin/gofmt" "${BINDIR}/" || die "couldn't symlink gofmt"

go version
