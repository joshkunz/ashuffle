#!/bin/sh
cd /ashuffle/t/integration
# The default test timeout is 10m. Need to set the timeout to 1h so the larger
# performance tests can run.
# TODO(jkz): Factor out performance tests so they can run in parallel.
exec go test -v -timeout 1h ashuffle/integration
