# Zigbee2MQTT converter tests

```sh
npm ci
sh code/tests/integration/run.sh
```

These tests load the real converter and the pinned Zigbee2MQTT libraries. They cover unknown measurements, diagnostic units and wire IDs, reporting configuration, calibration writes and compatibility with devices that lack the new attributes.

Endpoint calls are controlled by the tests; no coordinator, MQTT broker or Home Assistant server is contacted. Set `ZIGBEE_TEST_NODE_MODULES` to an absolute dependency directory to check another installed version.
