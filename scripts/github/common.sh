# renovate: datasource=pypi depName=meson
MESON_VERSION="1.8.2"
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
    target_arch="$1"

    declare -l -a deb_packages
    deb_packages=(
        "${CLANG_CC}"
        "${CLANG_FORMAT}"
        "${CLANG_TIDY}"
        cmake
        "${LLD}"
        ninja-build
        patchelf
        python3
        python3-pip
        python3-setuptools
        python3-wheel
    )
    if [[ "${target_arch}" = "x86_64" ]]; then
        deb_packages+=( libmpdclient-dev )
    fi

    if test -n "${IN_DEBUG_MODE:-}"; then
        echo apt install "${deb_packages[@]}"
        return 0
    fi

    sudo env DEBIAN_FRONTEND=noninteractive apt-get update -y && \
        sudo env DEBIAN_FRONTEND=noninteractive apt-get install -y \
        "${deb_packages[@]}" \
    || die "couldn't apt-get required packages"
    sudo pip3 install meson=="${MESON_VERSION}" || die "couldn't install meson"
}
