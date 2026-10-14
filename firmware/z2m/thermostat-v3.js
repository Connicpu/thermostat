// Zigbee2MQTT external converter for Connie's Thermostat V3 (UNTESTED prototype).
// Copy to <z2m data dir>/external_converters/ (Z2M 2.x) and restart Z2M.
// Basic cluster: manufacturerName "Connie", modelId "thermostat-v3".
const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const m = require('zigbee-herdsman-converters/lib/modernExtend');
const {Zcl} = require('zigbee-herdsman');
const e = exposes.presets;

// Manufacturer-specific Boost cluster, see firmware/components/zigbee/include/zb_boost.h
const boostCluster = 'connieBoost';
const boostOpts = {cluster: boostCluster};
const boostExtend = [
    m.deviceAddCustomCluster(boostCluster, {
        ID: 0xfc57,
        manufacturerCode: 0x131b,
        attributes: {
            boostSetpoint: {ID: 0x0000, type: Zcl.DataType.INT16},
            boostDuration: {ID: 0x0001, type: Zcl.DataType.UINT16},
            boostMode: {ID: 0x0002, type: Zcl.DataType.ENUM8},
            boostCommand: {ID: 0x0003, type: Zcl.DataType.ENUM8},
            boostState: {ID: 0x0004, type: Zcl.DataType.ENUM8},
            boostRemaining: {ID: 0x0005, type: Zcl.DataType.UINT16},
            boostLastResult: {ID: 0x0006, type: Zcl.DataType.ENUM8},
            swing: {ID: 0x0010, type: Zcl.DataType.UINT16},
            dayCoolStart: {ID: 0x0020, type: Zcl.DataType.UINT16},
            dayCoolTemp: {ID: 0x0021, type: Zcl.DataType.INT16},
            nightCoolStart: {ID: 0x0022, type: Zcl.DataType.UINT16},
            nightCoolTemp: {ID: 0x0023, type: Zcl.DataType.INT16},
            dayHeatStart: {ID: 0x0024, type: Zcl.DataType.UINT16},
            dayHeatTemp: {ID: 0x0025, type: Zcl.DataType.INT16},
            nightHeatStart: {ID: 0x0026, type: Zcl.DataType.UINT16},
            nightHeatTemp: {ID: 0x0027, type: Zcl.DataType.INT16},
            morningBoostDuration: {ID: 0x0028, type: Zcl.DataType.UINT16},
            morningBoostOffset: {ID: 0x0029, type: Zcl.DataType.INT16},
            scheduleState: {ID: 0x002a, type: Zcl.DataType.ENUM8},
        },
        commands: {},
        commandsResponse: {},
    }),
    m.numeric({name: 'boost_setpoint', ...boostOpts, attribute: 'boostSetpoint', scale: 100, unit: '°C',
        valueMin: 10, valueMax: 32, valueStep: 0.5, description: 'Boost-to-target temperature', access: 'ALL'}),
    m.numeric({name: 'boost_duration', ...boostOpts, attribute: 'boostDuration', unit: 'min',
        valueMin: 1, valueMax: 120, valueStep: 1, description: 'Timed boost length', access: 'ALL'}),
    m.enumLookup({name: 'boost_mode', ...boostOpts, attribute: 'boostMode',
        lookup: {auto: 0, heat: 1, cool: 2}, description: 'Boost direction', access: 'ALL'}),
    m.enumLookup({name: 'boost_command', ...boostOpts, attribute: 'boostCommand',
        lookup: {cancel: 0, start_to_target: 1, start_timed: 2}, description: 'Start or cancel a boost', access: 'SET'}),
    m.enumLookup({name: 'boost_state', ...boostOpts, attribute: 'boostState',
        lookup: {off: 0, heating_to_target: 1, cooling_to_target: 2, timed_heat: 3, timed_cool: 4},
        description: 'Current boost', access: 'STATE_GET'}),
    m.numeric({name: 'boost_remaining', ...boostOpts, attribute: 'boostRemaining', unit: 'min',
        description: 'Boost time remaining', access: 'STATE_GET'}),
    m.enumLookup({name: 'boost_last_result', ...boostOpts, attribute: 'boostLastResult',
        lookup: {none: 0, started: 1, target_reached: 2, time_expired: 3, cancelled: 4, mode_changed: 5,
            stopped_by_failsafe: 6, temperature_limit: 7, rejected_invalid: 16, rejected_sensor_fault: 17,
            rejected_already_at_target: 18, rejected_no_direction: 19},
        description: 'Last boost outcome', access: 'STATE_GET'}),
    // swing is a temperature *difference* in °C (do not convert it like an absolute temperature)
    m.numeric({name: 'temperature_swing', ...boostOpts, attribute: 'swing', scale: 100, unit: '°C',
        valueMin: 0.1, valueMax: 1, valueStep: 0.1, entityCategory: 'config',
        description: 'Hysteresis each side of the setpoint: heat on at setpoint - swing, off at setpoint + swing (cooling mirrored)',
        access: 'ALL'}),
    // program schedule (start times in minutes after local midnight)
    ...[['day_heat', 'dayHeat'], ['night_heat', 'nightHeat'], ['day_cool', 'dayCool'], ['night_cool', 'nightCool']].flatMap(([n, a]) => [
        m.numeric({name: `${n}_start`, ...boostOpts, attribute: `${a}Start`, unit: 'min', valueMin: 0, valueMax: 1439,
            valueStep: 15, entityCategory: 'config', description: `${n.replace('_', ' ')} start, minutes after midnight`, access: 'ALL'}),
        m.numeric({name: `${n}_temperature`, ...boostOpts, attribute: `${a}Temp`, scale: 100, unit: '°C', valueMin: 10,
            valueMax: 32, valueStep: 0.5, entityCategory: 'config', description: `${n.replace('_', ' ')} setpoint`, access: 'ALL'}),
    ]),
    m.numeric({name: 'morning_boost_duration', ...boostOpts, attribute: 'morningBoostDuration', unit: 'min', valueMin: 0,
        valueMax: 240, valueStep: 15, entityCategory: 'config', description: 'Heat-mode boost length from day heating start (0 = off)', access: 'ALL'}),
    m.numeric({name: 'morning_boost_offset', ...boostOpts, attribute: 'morningBoostOffset', scale: 100, unit: '°C', valueMin: 0,
        valueMax: 3, valueStep: 0.1, entityCategory: 'config', description: 'Added to the heating setpoint during the morning boost', access: 'ALL'}),
    m.enumLookup({name: 'schedule_state', ...boostOpts, attribute: 'scheduleState',
        lookup: {no_clock_holding_day: 0, day: 1, night: 2, day_morning_boost: 3}, description: 'Active program period', access: 'STATE_GET'}),
];

const definition = {
    zigbeeModel: ['thermostat-v3'],
    model: 'thermostat-v3',
    vendor: 'Connie',
    description: '24VAC single-stage heat/cool thermostat (ESP32-C6)',
    extend: boostExtend,
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
            .withSystemMode(['off', 'heat', 'cool'])
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
