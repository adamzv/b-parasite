# Host regression tests

From the repository root:

```sh
npm ci
npm test
```

Requirements: Node 22 or newer, Python 3, Bash and a C11 compiler (`cc`, or set `CC`). The Node dependency versions are pinned in `package-lock.json`. There are no Python package dependencies and no Home Assistant connection is needed.

The suites compile the sensor, application, recovery, boot guard and counter code against controlled Zephyr interfaces. They also check firmware identity, release packaging, Docker build paths and the external converter against real `zigbee-herdsman` dependencies. The per-suite notes describe the original regressions.

These are host tests. A successful run doesn't measure battery current, transmit radio packets or prove that a board rejoins a real coordinator. Build the firmware and check those behaviors on hardware separately.

To test against another installed converter dependency tree:

```sh
ZIGBEE_TEST_NODE_MODULES=/absolute/path/to/node_modules npm test
```
