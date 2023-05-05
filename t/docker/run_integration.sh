#!/bin/sh
cd /ashuffle/t/integration
# The default test timeout is 10m. Need to set the timeout to 1h so the larger
# performance tests can run.
exec go test -v -timeout 1h "$@" ashuffle/integration
