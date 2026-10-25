import { describe, expect, it } from 'vitest';
import { ENTITY_KEYS, boostResultIsBad, calibrationWire, deltaFactor, discoverEntities, findDevice, fmtTime, hoursToMin, periodOf, snap, timeToHours } from '../src/model.js';
import { DEVICE_ID, PREFIX, makeHass } from './mock-hass.js';

describe('discovery', () => {
  it('finds the device by manufacturer and model', () => {
    expect(findDevice(makeHass())).toBe(DEVICE_ID);
    expect(findDevice({ devices: {} })).toBeUndefined();
  });

  it('maps every quirk entity by translation_key', () => {
    const e = discoverEntities(makeHass(), DEVICE_ID);
    expect(Object.keys(e).sort()).toEqual(Object.keys(ENTITY_KEYS).sort());
    expect(e.climate).toBe(`climate.${PREFIX}`);
    expect(e.boost_duration).toBe(`number.${PREFIX}_boost_duration`);
    expect(e.morning_boost_duration).toBe(`number.${PREFIX}_morning_heating_boost_duration`);
  });

  it('falls back to entity_id suffixes without translation_key, longest suffix winning', () => {
    const e = discoverEntities(makeHass({ withTk: false }), DEVICE_ID);
    expect(Object.keys(e).sort()).toEqual(Object.keys(ENTITY_KEYS).sort());
    expect(e.boost_duration).toBe(`number.${PREFIX}_boost_duration`);
    expect(e.morning_boost_duration).toBe(`number.${PREFIX}_morning_heating_boost_duration`);
    expect(e.boost_state).toBe(`sensor.${PREFIX}_boost`);
    expect(e.humidity).toBe(`sensor.${PREFIX}_humidity`);
  });

  it('ignores other devices and applies overrides', () => {
    const e = discoverEntities(makeHass(), DEVICE_ID, { humidity: 'sensor.hall_humidity' });
    expect(e.climate).not.toBe('climate.other');
    expect(e.humidity).toBe('sensor.hall_humidity');
    expect(discoverEntities(makeHass(), undefined)).toEqual({});
  });
});

describe('helpers', () => {
  it('normalises the schedule period', () => {
    expect(periodOf('Day')).toBe('day');
    expect(periodOf('night')).toBe('night');
    expect(periodOf('Day_morning_boost')).toBe('boost');
    expect(periodOf('No_clock_holding_day')).toBe('noclock');
    expect(periodOf('unavailable')).toBeNull();
  });

  it('converts times', () => {
    expect(fmtTime(hoursToMin(6.5))).toBe('06:30');
    expect(fmtTime(hoursToMin(23.75))).toBe('23:45');
    expect(timeToHours('06:40')).toBe(6.75);
    expect(timeToHours('23:59')).toBe(23.75);
    expect(timeToHours('')).toBeNull();
  });

  it('scales °C differences only for °F users', () => {
    expect(deltaFactor('°C', '°F')).toBe(1.8);
    expect(deltaFactor('°C', '°C')).toBe(1);
    expect(deltaFactor('°F', '°F')).toBe(1);
  });

  it('biases calibration writes onto the tenth ZHA will store', () => {
    for (let i = -25; i <= 25; i++) {
      const v = Number((i * 0.1).toFixed(1));
      const w = calibrationWire(v);
      expect(Math.trunc(w / 0.1)).toBe(i);
      expect(Math.abs(w - v)).toBeLessThan(1e-6);
      expect(Math.abs(w)).toBeLessThanOrEqual(2.5);
    }
    expect(calibrationWire(0)).toBe(0);
    expect(calibrationWire(2.5)).toBe(2.5);
    expect(calibrationWire(-2.5)).toBe(-2.5);
  });

  it('snaps and clamps', () => {
    expect(snap(70.3, 50, 90, 0.5)).toBe(70.5);
    expect(snap(95, 50, 90, 1)).toBe(90);
    expect(snap(0.30000000000000004, 0, 3, 0.1)).toBe(0.3);
  });

  it('flags bad boost results', () => {
    expect(boostResultIsBad('Rejected_sensor_fault')).toBe(true);
    expect(boostResultIsBad('Stopped_by_failsafe')).toBe(true);
    expect(boostResultIsBad('Target_reached')).toBe(false);
  });
});
