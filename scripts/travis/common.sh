MESON_VERSION="0.54.0"
GO_VERSION="1.14.6"
GIMMIE_URL="https://raw.githubusercontent.com/travis-ci/gimme/master/gimme"

die() {
    echo "$@" >&2
    exit 1
}

install_go() {
    goversion="$1"
    # Gimmie outputs envrionment variables, so we need to eval them here.
    eval "$(curl -sL "${GIMMIE_URL}" | GIMME_GO_VERSION="${goversion}" bash)"
}

build_meta() {
    (cd tools/meta && GO11MODULE=on go build)
}

setup() {
    if test -n "${IN_DEBUG_MODE}"; then
        return 0
    fi
    sudo env DEBIAN_FRONTEND=noninteractive apt-get update -y && \
        sudo env DEBIAN_FRONTEND=noninteractive apt-get install -y \
            clang-9 \
            clang-tidy-9 \
            clang-format-9 \
            cmake \
            libmpdclient-dev \
            ninja-build \
            patchelf \
            python3 python3-pip python3-setuptools python3-wheel \
    || die "couldn't apt-get required packages" 
    sudo pip3 install meson=="${MESON_VERSION}" || die "couldn't install meson"
    install_go "${GO_VERSION}"
    build_meta
}
