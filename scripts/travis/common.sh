MESON_VERSION="0.52.0"

die() {
    echo "$@" >&2
    exit 1
}

build_meta() {
    prevwd="${PWD}"
    godir="$(mktemp -d)"
    wget --quiet -O- https://dl.google.com/go/go1.14.2.linux-amd64.tar.gz | tar -C "${godir}" -xz --strip-components=1
    "${godir}/bin/go" version
    (cd tools/meta && GOROOT="${godir}" GO11MODULE=on "${godir}/bin/go" build)
}

setup() {
    if test -n "${IN_DEBUG_MODE}"; then
        return 0
    fi
    sudo apt-get update && \
        sudo apt-get upgrade -y && \
        sudo apt-get install -y \
            clang \
            libmpdclient-dev \
            ninja-build \
            python3 python3-pip python3-setuptools python3-wheel \
    || die "couldn't apt-get required packages" 
    sudo pip3 install meson=="${MESON_VERSION}" || die "couldn't install meson"
    build_meta
}
