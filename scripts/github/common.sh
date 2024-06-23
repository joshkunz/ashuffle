# renovate: datasource=pypi depName=meson
MESON_VERSION="1.4.1"
LLVM_RELEASE="14"

CLANG_CC="clang-${LLVM_RELEASE}"
CLANG_CXX="clang++-${LLVM_RELEASE}"
CLANG_FORMAT="clang-format-${LLVM_RELEASE}"
CLANG_TIDY="clang-tidy-${LLVM_RELEASE}"
LLD="lld-${LLVM_RELEASE}"

die() {
    echo "$@" >&2
    exit 1
}

build_meta() {
    (cd tools/meta && GO11MODULE=on go build -o "${RUNNER_TEMP}/meta")
}

setup() {
    if test -n "${IN_DEBUG_MODE:-}"; then
        return 0
    fi
    sudo env DEBIAN_FRONTEND=noninteractive apt-get update -y && \
        sudo env DEBIAN_FRONTEND=noninteractive apt-get install -y \
            "${CLANG_CC}" \
            "${CLANG_FORMAT}" \
            "${CLANG_TIDY}" \
            cmake \
            libmpdclient-dev \
            "${LLD}" \
            ninja-build \
            patchelf \
            python3 python3-pip python3-setuptools python3-wheel \
    || die "couldn't apt-get required packages"
    sudo pip3 install meson=="${MESON_VERSION}" || die "couldn't install meson"
}
