// Zigbee2MQTT external converter for Connie's Thermostat V3 (UNTESTED prototype).
// Copy to <z2m data dir>/external_converters/ (Z2M 2.x) and restart Z2M.
// Basic cluster: manufacturerName "Connie", modelId "thermostat-v3".
const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const e = exposes.presets;

const definition = {
    zigbeeModel: ['thermostat-v3'],
    model: 'thermostat-v3',
    vendor: 'Connie',
    description: '24VAC single-stage heat/cool thermostat (ESP32-C6)',
    fromZigbee: [fz.thermostat, fz.fan, fz.humidity],
    toZigbee: [
        tz.thermostat_local_temperature,
        tz.thermostat_occupied_heating_setpoint,
        tz.thermostat_occupied_cooling_setpoint,
        tz.thermostat_system_mode,
        tz.thermostat_running_state,
        tz.thermostat_local_temperature_calibration,
        tz.fan_mode,
    ],
    exposes: [
        e.climate()
            .withLocalTemperature()
            .withSetpoint('occupied_heating_setpoint', 10, 30.5, 0.5)
            .withSetpoint('occupied_cooling_setpoint', 11.5, 32, 0.5)
            .withSystemMode(['off', 'heat', 'cool', 'auto'])
            .withRunningState(['idle', 'heat', 'cool', 'fan_only'])
            .withFanMode(['on', 'auto'])
            .withLocalTemperatureCalibration(-2.5, 2.5, 0.1),
        e.humidity(),
    ],
    configure: async (device, coordinatorEndpoint) => {
        const ep = device.getEndpoint(1);
        await reporting.bind(ep, coordinatorEndpoint, ['hvacThermostat', 'hvacFanCtrl', 'msRelativeHumidity']);
        await reporting.thermostatTemperature(ep);
        await reporting.thermostatOccupiedHeatingSetpoint(ep);
        await reporting.thermostatOccupiedCoolingSetpoint(ep);
        await reporting.thermostatSystemMode(ep);
        await reporting.thermostatRunningState(ep);
        await reporting.humidity(ep);
        await ep.configureReporting('hvacFanCtrl', [
            {attribute: 'fanMode', minimumReportInterval: 0, maximumReportInterval: 3600, reportableChange: 0},
        ]);
    },
    ota: true,
};

module.exports = definition;
