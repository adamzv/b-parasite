import assert from 'node:assert/strict';
import {test, after} from 'node:test';
import {copyFile, mkdtemp, symlink, rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {fileURLToPath, pathToFileURL} from 'node:url';
import path from 'node:path';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
const dependencies = process.env.ZIGBEE_TEST_NODE_MODULES;
assert.ok(dependencies, 'Set ZIGBEE_TEST_NODE_MODULES to real Zigbee2MQTT node_modules');
const work = await mkdtemp(path.join(tmpdir(), 'bparasite-converter-'));
after(() => rm(work, {recursive: true, force: true}));
await symlink(dependencies, path.join(work, 'node_modules'), 'dir');
await copyFile(path.join(root, 'code/samples/zigbee/bparasite.mjs'), path.join(work, 'bparasite.mjs'));
const {default: definition} = await import(pathToFileURL(path.join(work, 'bparasite.mjs')));
const {Zcl} = await import(pathToFileURL(path.join(dependencies, 'zigbee-herdsman/dist/index.js')));
const {prepareDefinition} = await import(pathToFileURL(path.join(dependencies, 'zigbee-herdsman-converters/dist/index.js')));
const expandedDefinition = prepareDefinition(definition);
const cluster = 'bparasiteSoilCalibration';
const convert = (name, data, state = {}) => definition.fromZigbee.find((c) => c.cluster === name)
    .convert({}, {data}, () => {}, {}, {state});
const diagnostics = ['soil_valid', 'sensor_errors', 'battery_mv', 'boot_count', 'reset_cause', 'uptime_sec'];

function convertExpanded(name, data, state = {}, options = {}) {
    const result = {};
    for (const converter of expandedDefinition.fromZigbee.filter((item) => item.cluster === name)) {
        Object.assign(result, converter.convert(expandedDefinition, {data}, () => {}, options, {state}));
    }
    return result;
}

for (const value of [null, 65535]) {
    test(`expanded illuminance converters clear ${value} and recover with lux values`, () => {
        const state = {illuminance: 100, illuminance_raw: 20001};
        Object.assign(state, convertExpanded('msIlluminanceMeasurement', {measuredValue: value}, state, {illuminance_raw: true}));
        assert.equal(state.illuminance, null);
        assert.equal(state.illuminance_raw, null);
        Object.assign(state, convertExpanded('msIlluminanceMeasurement', {measuredValue: 20001}, state, {illuminance_raw: true}));
        assert.equal(state.illuminance, 100);
        assert.equal(state.illuminance_raw, 20001);
        const exposes = expandedDefinition.exposes(undefined, {});
        assert.ok(exposes.some((item) => item.property === 'illuminance' && item.unit === 'lx'));
        assert.ok(expandedDefinition.toZigbee.some((item) => item.key.includes('illuminance')));
    });
}

test('separate diagnostic reports keep soil invalid until both guards clear in either order', () => {
    for (const updates of [[{sensor_errors: 0}, {soil_valid: true}], [{soil_valid: true}, {sensor_errors: 0}]]) {
        const state = {soil_valid: false, sensor_errors: 2, soil_moisture: null, soil_raw: null};
        Object.assign(state, convertExpanded(cluster, updates[0], state));
        Object.assign(state, convertExpanded('msSoilMoisture', {measuredValue: 4500}, state));
        Object.assign(state, convertExpanded(cluster, {soil_raw: 5075}, state));
        assert.equal(state.soil_moisture, null);
        assert.equal(state.soil_raw, null);
        Object.assign(state, convertExpanded(cluster, updates[1], state));
        assert.equal(state.soil_moisture, null);
        assert.equal(state.soil_raw, null);
        Object.assign(state, convertExpanded('msSoilMoisture', {measuredValue: 4500}, state));
        Object.assign(state, convertExpanded(cluster, {soil_raw: 5075}, state));
        assert.equal(state.soil_moisture, 45);
        assert.equal(state.soil_raw, 50.75);
    }
});

function configureDevice(fail = () => {}) {
    const calls = [];
    class Endpoint {}
    const endpoint = Object.assign(new Endpoint(), {
        ID: 10,
        getInputClusters: () => [{name: 'msIlluminanceMeasurement', ID: 0x0400}],
        bind: async (name) => { calls.push(['bind', name]); fail('bind', name); },
        configureReporting: async (name, records) => { calls.push(['report', name, records]); fail('report', name, records); },
        read: async (name, attrs) => { calls.push(['read', name, attrs]); fail('read', name, attrs); },
    });
    const device = {getEndpoint: () => endpoint, endpoints: [endpoint], customClusters: {},
        addCustomCluster(name, value) {this.customClusters[name] = value;}};
    return {calls, device, endpoint};
}

test('expanded definition retains illuminance binding reporting and initial read', async () => {
    const fixture = configureDevice();
    await expandedDefinition.configure(fixture.device, {}, expandedDefinition);
    assert.ok(fixture.calls.some(([op, name]) => op === 'bind' && name === 'msIlluminanceMeasurement'));
    assert.ok(fixture.calls.some(([op, name, records]) => op === 'report' && name === 'msIlluminanceMeasurement' &&
        records.some((record) => record.attribute === 'measuredValue' && record.maximumReportInterval === 3600)));
    assert.ok(fixture.calls.some(([op, name, attrs]) => op === 'read' && name === 'msIlluminanceMeasurement' &&
        attrs.includes('measuredValue')));
});

test('unknown measurement clears previously cached numeric state', () => {
    for (const [name, key, invalid] of [['msTemperatureMeasurement', 'temperature', -32768],
        ['msRelativeHumidity', 'humidity', 65535], ['msSoilMoisture', 'soil_moisture', 65535]]) {
        for (const value of [null, invalid]) {
            const result = convert(name, {measuredValue: value}, {[key]: 42});
            assert.equal({...{[key]: 42}, ...result}[key], null);
        }
    }
});

test('standard measurements keep zero and their correct units', () => {
    assert.deepEqual(convert('msTemperatureMeasurement', {measuredValue: 2134}), {temperature: 21.34});
    assert.deepEqual(convert('msRelativeHumidity', {measuredValue: 5032}), {humidity: 50.32});
    assert.deepEqual(convert('msSoilMoisture', {measuredValue: 3210}), {soil_moisture: 32.1});
    assert.deepEqual(convert('msSoilMoisture', {measuredValue: 0}), {soil_moisture: 0});
});

test('diagnostics are unscaled while calibration remains hundredths', () => {
    assert.deepEqual(convert(cluster, {soil_raw: -1234, soil_dry: 2034, soil_wet: 8123,
        soil_valid: 1, sensor_errors: 8, battery_mv: 2891, boot_count: 1234, reset_cause: 0x10001, uptime_sec: 86432}),
    {soil_raw: -12.34, soil_dry: 20.34, soil_wet: 81.23, soil_valid: true,
        sensor_errors: 8, battery_mv: 2891, boot_count: 1234, reset_cause: 0x10001, uptime_sec: 86432});
});

test('private cluster reports preserve the firmware wire IDs and scalar widths', async () => {
    const customClusters = {};
    await definition.extend[0].configure[0]({addCustomCluster: (name, value) => {customClusters[name] = value;}});
    const registered = Zcl.Utils.getCluster(cluster, undefined, customClusters);
    const payload = Object.entries({soil_raw: -1234, soil_valid: 1, sensor_errors: 2,
        battery_mv: 2891, boot_count: 1234, reset_cause: 65537, uptime_sec: 86432}).map(([name, attrData]) => {
        const attr = Zcl.Utils.getClusterAttribute(registered, name);
        return {attrId: attr.ID, dataType: attr.type, attrData};
    });
    const frame = Zcl.Frame.create(Zcl.FrameType.GLOBAL, Zcl.Direction.SERVER_TO_CLIENT, true,
        undefined, 1, 'report', cluster, payload, customClusters);
    assert.equal(registered.ID, 0xfc00);
    assert.equal(frame.toBuffer().toString('hex'),
        '18010a0000292efb10001001110020021200214b0b130023d204000014002301000100150023a0510100');
});

test('invalid soil diagnostic clears merged soil and blocks later stale reports until recovery', () => {
    for (const data of [{soil_valid: false}, {sensor_errors: 2}]) {
        const state = {...{soil_moisture: 10, soil_raw: 30}, ...convert(cluster, data)};
        assert.equal(state.soil_moisture, null);
        assert.equal(state.soil_raw, null);
        assert.equal(convert('msSoilMoisture', {measuredValue: 5000}, state).soil_moisture, null);
        assert.equal(convert(cluster, {soil_raw: 5000}, state).soil_raw, null);
        const recovered = {...state, ...convert(cluster, {soil_valid: true, sensor_errors: 0}, state)};
        assert.equal(convert('msSoilMoisture', {measuredValue: 4500}, recovered).soil_moisture, 45);
        assert.equal(convert(cluster, {soil_raw: 5075}, recovered).soil_raw, 50.75);
    }
});

test('null calibration raw is unknown instead of zero', () => {
    assert.deepEqual(convert(cluster, {soil_raw: null}), {soil_raw: null});
});

test('diagnostics are readable and exposed as diagnostics', async () => {
    const fixture = configureDevice();
    for (const key of diagnostics) {
        const exposed = definition.exposes.find((item) => item.property === key);
        assert.ok(exposed, key);
        assert.equal(exposed.category, 'diagnostic');
        const converter = definition.toZigbee.find((item) => item.key.includes(key));
        assert.ok(converter, key);
        await converter.convertGet(fixture.endpoint, key);
        assert.deepEqual(fixture.calls.at(-1), ['read', cluster, [key]]);
    }
});

test('configuration requests build identity and each diagnostic reporting', async () => {
    const fixture = configureDevice();
    await definition.configure(fixture.device, {});
    assert.ok(fixture.calls.some(([op, name, attrs]) => op === 'read' && name === 'genBasic' && attrs.includes('swBuildId')));
    for (const key of diagnostics) assert.ok(fixture.calls.some(([op, name, records]) =>
        op === 'report' && name === cluster && records.some((record) => record.attribute === key)), key);
});

test('unsupported new attributes leave calibration and standard reporting available on older boards', async () => {
    const fixture = configureDevice((op, name, payload) => {
        if (name === cluster && payload?.some((item) => diagnostics.includes(item.attribute ?? item))) {
            throw new Error("Status 'UNSUPPORTED_ATTRIBUTE'");
        }
    });
    await definition.configure(fixture.device, {});
    assert.ok(fixture.calls.some(([op, name, attrs]) => op === 'read' && name === cluster && attrs.includes('soil_dry')));
    assert.ok(fixture.calls.some(([op, name]) => op === 'report' && name === 'msSoilMoisture'));
});

test('unsupported private cluster is compatible with the original standard-only firmware', async () => {
    const fixture = configureDevice((op, name) => {
        if (name === cluster) throw new Error("Status 'UNSUPPORTED_CLUSTER'");
    });
    await definition.configure(fixture.device, {});
});

test('unexpected configure failures remain visible', async () => {
    for (const operation of ['bind', 'report', 'read']) {
        for (const message of ['NO_RESPONSE', 'Timeout', "Status 'FAILURE'", 'INSUFFICIENT_SPACE']) {
            const fixture = configureDevice((op, name) => {
                if (name === cluster && op === operation) throw new Error(message);
            });
            await assert.rejects(() => definition.configure(fixture.device, {}), {message});
        }
    }
});

test('calibration writes retain hundredths and reject out-of-range values', async () => {
    const calls = [];
    const endpoint = {write: async (name, data) => {calls.push([name, data]);}};
    const converter = definition.toZigbee.find((item) => item.key.includes('soil_dry'));
    assert.deepEqual(await converter.convertSet(endpoint, 'soil_dry', 20.125), {state: {soil_dry: 20.13}});
    assert.deepEqual(calls, [[cluster, {soil_dry: 2013}]]);
    await assert.rejects(() => converter.convertSet(endpoint, 'soil_wet', 1000));
    await assert.rejects(() => converter.convertSet(endpoint, 'soil_wet', 'bad'));
});
