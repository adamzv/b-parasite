# Host regression tests

Run from the b-parasite project root:

```sh
BPARASITE_TEST_PYTHON=/path/to/python-with-websocket-client \
ZIGBEE_TEST_NODE_MODULES=/path/to/node_modules \
bash code/tests/run.sh
```

The Node modules must contain `zigbee-herdsman-converters` and `zigbee-herdsman`. Converter tests were checked with Zigbee2MQTT 2.12.1's converter dependency, 26.76.0. The Python integration interpreter needs `websocket-client`; other host suites use Python's standard library. A C11 compiler, Bash and Node are required.

Sensor, application, boot guard and recovery tests compile production C against controlled Zephyr/hardware boundaries. Integration tests use the actual converter dependencies and Python assistant. Identity/package tests run the scripts on temporary fixtures. Per-suite verification notes record the regressions seen before fixes.

These tests do not measure sleep current or prove radio recovery on hardware. Build the nRF52840 firmware separately before packaging.
