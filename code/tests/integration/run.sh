#!/bin/sh
set -eu

cd "$(dirname "$0")/../../.."
: "${ZIGBEE_TEST_NODE_MODULES:?Point to node_modules containing zigbee-herdsman-converters and zigbee-herdsman}"
test_python=${BPARASITE_TEST_PYTHON:-python3}
"$test_python" -m unittest discover -s code/tests/integration -p 'test_*.py' -v
node --test code/tests/integration/test_converter.mjs

