#!/bin/sh
set -eu

cd "$(dirname "$0")/../../.."
ZIGBEE_TEST_NODE_MODULES=${ZIGBEE_TEST_NODE_MODULES:-"$(pwd)/node_modules"}
export ZIGBEE_TEST_NODE_MODULES
node --test code/tests/integration/test_converter.mjs

