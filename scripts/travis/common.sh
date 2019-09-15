die() {
    echo "$@" >&2
    exit 1
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
    sudo pip3 install meson || die "couldn't install meson"
}
