// Zigbee2MQTT 2.x external converter for the b-parasite Zigbee sample with practical soil calibration.
// Overrides the built-in "b-parasite" definition and adds the private 0xFC00 calibration cluster.
import * as fz from 'zigbee-herdsman-converters/converters/fromZigbee';
import * as m from 'zigbee-herdsman-converters/lib/modernExtend';
import * as reporting from 'zigbee-herdsman-converters/lib/reporting';
import {presets as e, access as ea} from 'zigbee-herdsman-converters/lib/exposes';
import {Zcl} from 'zigbee-herdsman';

const ENDPOINT = 10;
const CALIBRATION_CLUSTER = 'bparasiteSoilCalibration';
// The firmware keeps these in units of 0.01% of the factory air (0%) to water (100%) scale.
const CALIBRATION_KEYS = ['soil_raw', 'soil_dry', 'soil_wet'];
const DIAGNOSTIC_KEYS = ['soil_valid', 'sensor_errors', 'battery_mv', 'boot_count', 'reset_cause', 'uptime_sec'];

// Old firmware without the calibration cluster must still configure the standard clusters.
async function ignoreUnsupported(action) {
    try {
        await action();
    } catch (error) {
        const message = String(error?.message ?? error);
        if (!/\bUNSUPPORTED_CLUSTER\b|\bUNSUPPORTED_ATTRIBUTE\b/.test(message)) {
            throw error;
        }
    }
}

const fzCalibration = {
    cluster: CALIBRATION_CLUSTER,
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
        const result = {};
        for (const key of CALIBRATION_KEYS) {
            if (msg.data[key] !== undefined) {
                result[key] = msg.data[key] === null || msg.data[key] === -32768 ? null : msg.data[key] / 100;
            }
        }
        for (const key of DIAGNOSTIC_KEYS) {
            if (msg.data[key] !== undefined) {
                result[key] = key === 'soil_valid' && msg.data[key] !== null ? Boolean(msg.data[key]) : msg.data[key];
            }
        }
        if (soilInvalid({...meta?.state, ...result})) {
            result.soil_moisture = null;
            result.soil_raw = null;
        }
        return result;
    },
};

function soilInvalid(state) {
    return state?.soil_valid === false || (state?.sensor_errors & 2) !== 0;
}

// Explicit nulls clear Zigbee2MQTT's merged state after a sensor failure.
function withValidity(converter, key, unknown) {
    return {
        ...converter,
        convert: (model, msg, publish, options, meta) => {
            if (msg.data.measuredValue === undefined) {
                return;
            }
            if (msg.data.measuredValue === null || msg.data.measuredValue === unknown ||
                (key === 'soil_moisture' && soilInvalid(meta?.state))) {
                return {[key]: null, ...(key === 'illuminance' && options?.illuminance_raw ? {illuminance_raw: null} : {})};
            }
            return converter.convert(model, msg, publish, options, meta);
        },
    };
}

const illuminance = m.illuminance();
illuminance.fromZigbee = illuminance.fromZigbee.map((converter) => withValidity(converter, 'illuminance', 65535));

const tzCalibration = {
    key: ['soil_dry', 'soil_wet'],
    convertSet: async (entity, key, value) => {
        const scaled = Math.round(Number(value) * 100);
        if (!Number.isFinite(scaled) || scaled < -32768 || scaled > 32767) {
            throw new Error(`${key} must be a number between -327.68 and 327.67`);
        }
        await entity.write(CALIBRATION_CLUSTER, {[key]: scaled});
        return {state: {[key]: scaled / 100}};
    },
    convertGet: async (entity, key) => {
        await entity.read(CALIBRATION_CLUSTER, [key]);
    },
};

const tzSoilRaw = {
    key: ['soil_raw', ...DIAGNOSTIC_KEYS],
    convertGet: async (entity, key) => {
        await entity.read(CALIBRATION_CLUSTER, [key]);
    },
};

export default {
    zigbeeModel: ['b-parasite'],
    model: 'b-parasite',
    vendor: 'Custom devices (DiY)',
    description: 'b-parasite open source soil moisture sensor',
    extend: [
        m.deviceAddCustomCluster(CALIBRATION_CLUSTER, {
            name: CALIBRATION_CLUSTER,
            ID: 0xfc00,
            attributes: {
                soil_raw: {name: 'soil_raw', ID: 0x0000, type: Zcl.DataType.INT16},
                soil_dry: {name: 'soil_dry', ID: 0x0001, type: Zcl.DataType.INT16, write: true},
                soil_wet: {name: 'soil_wet', ID: 0x0002, type: Zcl.DataType.INT16, write: true},
                soil_valid: {name: 'soil_valid', ID: 0x0010, type: Zcl.DataType.BOOLEAN},
                sensor_errors: {name: 'sensor_errors', ID: 0x0011, type: Zcl.DataType.UINT8},
                battery_mv: {name: 'battery_mv', ID: 0x0012, type: Zcl.DataType.UINT16},
                boot_count: {name: 'boot_count', ID: 0x0013, type: Zcl.DataType.UINT32},
                reset_cause: {name: 'reset_cause', ID: 0x0014, type: Zcl.DataType.UINT32},
                uptime_sec: {name: 'uptime_sec', ID: 0x0015, type: Zcl.DataType.UINT32},
            },
            commands: {},
            commandsResponse: {},
        }),
        illuminance,
        m.identify(),
    ],
    fromZigbee: [withValidity(fz.temperature, 'temperature', -32768), withValidity(fz.humidity, 'humidity', 65535),
        fz.battery, withValidity(fz.soil_moisture, 'soil_moisture', 65535), fzCalibration],
    toZigbee: [tzCalibration, tzSoilRaw],
    exposes: [
        e.temperature(),
        e.humidity(),
        e.battery(),
        e.soil_moisture(),
        e.binary('soil_valid', ea.STATE_GET, true, false).withCategory('diagnostic')
            .withDescription('Whether the latest soil measurement succeeded.'),
        e.numeric('sensor_errors', ea.STATE_GET).withCategory('diagnostic')
            .withDescription('Latest measurement error mask: battery=1, soil=2, light=4, SHTC3=8.'),
        e.numeric('battery_mv', ea.STATE_GET).withUnit('mV').withCategory('diagnostic')
            .withDescription('Latest battery voltage in exact millivolts.'),
        e.numeric('boot_count', ea.STATE_GET).withCategory('diagnostic')
            .withDescription('Persistent boot counter.'),
        e.numeric('reset_cause', ea.STATE_GET).withCategory('diagnostic')
            .withDescription('Reset cause bitmask from the latest boot.'),
        e.numeric('uptime_sec', ea.STATE_GET).withUnit('s').withCategory('diagnostic')
            .withDescription('Seconds since the latest boot.'),
        e.numeric('soil_raw', ea.STATE_GET).withUnit('%').withCategory('diagnostic')
            .withDescription('Battery-compensated reading on the factory scale (0% = in air, 100% = in a glass of water). Press SW1 twice in dry soil or three times in watered soil to calibrate on the device.'),
        e.numeric('soil_dry', ea.ALL).withUnit('%').withCategory('config').withValueMin(-100).withValueMax(200).withValueStep(0.01)
            .withDescription('soil_raw that reads as 0% soil moisture. Set by pressing SW1 twice in dry soil; can be overridden here.'),
        e.numeric('soil_wet', ea.ALL).withUnit('%').withCategory('config').withValueMin(-100).withValueMax(200).withValueStep(0.01)
            .withDescription('soil_raw that reads as 100% soil moisture. Set by pressing SW1 three times in watered soil; can be overridden here.'),
    ],
    configure: async (device, coordinatorEndpoint) => {
        const endpoint = device.getEndpoint(ENDPOINT);
        await reporting.bind(endpoint, coordinatorEndpoint, ['genPowerCfg', 'msTemperatureMeasurement', 'msRelativeHumidity', 'msSoilMoisture']);
        await reporting.batteryPercentageRemaining(endpoint);
        await reporting.temperature(endpoint);
        await reporting.humidity(endpoint);
        await reporting.soil_moisture(endpoint);
        await ignoreUnsupported(() => endpoint.read('genBasic', ['swBuildId']));
        await ignoreUnsupported(() => reporting.bind(endpoint, coordinatorEndpoint, [CALIBRATION_CLUSTER]));
        // One attribute per request lets older firmware reject new diagnostics without losing calibration.
        for (const key of [...CALIBRATION_KEYS, ...DIAGNOSTIC_KEYS]) {
            const record = {attribute: key, minimumReportInterval: key === 'soil_raw' ? 10 : 1,
                maximumReportInterval: 3600};
            if (key !== 'soil_valid') record.reportableChange = key === 'soil_raw' ? 50 : 1;
            await ignoreUnsupported(() => endpoint.configureReporting(CALIBRATION_CLUSTER, [record]));
            await ignoreUnsupported(() => endpoint.read(CALIBRATION_CLUSTER, [key]));
        }
    },
};
