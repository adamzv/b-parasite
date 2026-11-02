#!/bin/sh
set -eu
sensor_tests=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project=$(CDPATH= cd -- "$sensor_tests/../../.." && pwd)
build=$(mktemp -d "${TMPDIR:-/tmp}/bparasite-sensors.XXXXXX")
trap 'rm -rf "$build"' EXIT HUP INT TERM
cc=${CC:-cc}
"$cc" -std=c11 -Wall -Wextra -Werror -I"$sensor_tests/mocks" -I"$project/code/prstlib/include" \
  "$project/code/prstlib/src/shtc3.c" "$sensor_tests/test_shtc3.c" -lm -o "$build/shtc3"
for variant in 1 0; do
  "$cc" -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare \
    -DTEST_PHOTOTRANSISTOR="$variant" -DCONFIG_PRSTLIB_PHOTO_NA_PER_LUX=10 \
    -I"$sensor_tests/mocks" -I"$project/code/prstlib/include" \
    "$project/code/prstlib/src/adc.c" "$sensor_tests/test_adc.c" -lm -o "$build/adc-$variant"
done
status=0
"$build/shtc3" || status=1
"$build/adc-1" || status=1
"$build/adc-0" || status=1
exit "$status"
