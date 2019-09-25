diag() {
    echo $@ >&2
}

die() {
    echo $@ >&2
    exit 1
}

latest_version() {
    stage="$(mktemp -d)"
    test -d "${stage}" || die "${stage} is not a valid directory"

    opwd="${PWD}"
    cd "${stage}"

    URL="$1"
    # Pattern to match version-like strings.
    VMATCH='v\d+\.\d+(\.\d+)?'

    git init 1>/dev/null 2>/dev/null || die "couldn't init git repo"
    git fetch --tags --depth=1 "${URL}" 2>/dev/null || die "couldn't fetch tags"
    git tag | grep -P "${VMATCH}" | sed -E 's/^[^0-9]*//' | sort -V --reverse | head -n1
    exitcode="$?"

    cd "${opwd}"
    rm -rf "${stage}"
    exit "${exitcode}"
}
