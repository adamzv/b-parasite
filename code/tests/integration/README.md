# HA and converter regression tests

Tests import the real `assess.py` and external converter. Python needs `websocket-client`; Node needs the
real `zigbee-herdsman-converters` and `zigbee-herdsman` packages used by Zigbee2MQTT. The converter is copied
into a temporary directory beside those dependencies; production source and dependencies stay unchanged.

```sh
BPARASITE_TEST_PYTHON=/path/to/venv/bin/python \
ZIGBEE_TEST_NODE_MODULES=/path/to/zigbee2mqtt/node_modules \
sh code/tests/integration/run.sh
```

Validated with Python 3.14, websocket-client 1.9.2 and zigbee-herdsman-converters 26.76.0.
The tests cover invalid measurements clearing cached state, diagnostic units/reads/reporting,
unsupported old-firmware attributes, visible configuration failures, calibration writes, and assessments
that suppress historical soil and previous watering notes while the current soil sensor is invalid.
Network/radio calls are replaced at the endpoint boundary; no hardware or HA server is contacted.
