#!/bin/sh

die() {
    echo "$@" >&2
    exit 1
}

export GOPATH=/ashuffle/t/integration
cd "$GOPATH"
go get ./... || die "failed to fetch test dependencies"
exec go test integration
