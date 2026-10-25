// Mocked `hass` for tests and the demo page: a Connie thermostat V3 as ZHA + the quirk expose it,
// in a Home Assistant set to °F (the number entities are what HA shows after unit conversion).
const DEV = 'dev_connie_v3';
const P = 'connie_thermostat_v3';

const tempNum = (v) => ({ state: String(v), attributes: { unit_of_measurement: '°F', device_class: 'temperature', min: 50, max: 89.5, step: 0.5, mode: 'box' } });
const hoursNum = (v) => ({ state: String(v), attributes: { unit_of_measurement: 'h', min: 0, max: 23.75, step: 0.25, mode: 'box' } });

/** [entity_id, translation_key, state object] */
export function baseEntities() {
  return [
    [`climate.${P}`, null, {
      state: 'heat',
      attributes: {
        friendly_name: 'Thermostat', hvac_modes: ['off', 'heat', 'cool'], fan_modes: ['auto', 'on'], fan_mode: 'auto',
        hvac_action: 'heating', current_temperature: 68.8, temperature: 70, min_temp: 50, max_temp: 90, target_temp_step: 1,
      },
    }],
    [`sensor.${P}_humidity`, null, { state: '41.2', attributes: { device_class: 'humidity', unit_of_measurement: '%' } }],
    [`number.${P}_day_heating_start`, 'day_heat_start', hoursNum(6.5)],
    [`number.${P}_day_heating_temperature`, 'day_heat_temp', tempNum(70)],
    [`number.${P}_night_heating_start`, 'night_heat_start', hoursNum(22)],
    [`number.${P}_night_heating_temperature`, 'night_heat_temp', tempNum(65)],
    [`number.${P}_day_cooling_start`, 'day_cool_start', hoursNum(8)],
    [`number.${P}_day_cooling_temperature`, 'day_cool_temp', tempNum(76)],
    [`number.${P}_night_cooling_start`, 'night_cool_start', hoursNum(21.5)],
    [`number.${P}_night_cooling_temperature`, 'night_cool_temp', tempNum(72)],
    [`number.${P}_morning_heating_boost_duration`, 'morning_boost_duration', { state: '45', attributes: { unit_of_measurement: 'min', device_class: 'duration', min: 0, max: 240, step: 15 } }],
    [`number.${P}_morning_heating_boost_offset`, 'morning_boost_offset', { state: '1.0', attributes: { unit_of_measurement: '°C', min: 0, max: 3, step: 0.1 } }],
    [`sensor.${P}_schedule_period`, 'schedule_state', { state: 'Day', attributes: { device_class: 'enum', options: ['No_clock_holding_day', 'Day', 'Night', 'Day_morning_boost'] } }],
    [`number.${P}_temperature_swing`, 'temperature_swing', { state: '0.5', attributes: { unit_of_measurement: '°C', min: 0.1, max: 1, step: 0.1, mode: 'slider' } }],
    [`number.${P}_local_temperature_calibration`, 'local_temperature_calibration', { state: '0.0', attributes: { unit_of_measurement: '°C', min: -2.5, max: 2.5, step: 0.1, mode: 'box' } }],
    [`number.${P}_boost_target_temperature`, 'boost_setpoint', tempNum(72)],
    [`number.${P}_boost_duration`, 'boost_duration', { state: '30', attributes: { unit_of_measurement: 'min', device_class: 'duration', min: 1, max: 120, step: 1 } }],
    [`select.${P}_boost_mode`, 'boost_mode', { state: 'Auto', attributes: { options: ['Auto', 'Heat', 'Cool'] } }],
    [`button.${P}_start_boost_to_target`, 'boost_start_target', { state: 'unknown', attributes: {} }],
    [`button.${P}_start_timed_boost`, 'boost_start_timed', { state: 'unknown', attributes: {} }],
    [`button.${P}_cancel_boost`, 'boost_cancel', { state: 'unknown', attributes: {} }],
    [`sensor.${P}_boost`, 'boost_state', { state: 'Off', attributes: { device_class: 'enum' } }],
    [`binary_sensor.${P}_boost_active`, 'boost_active', { state: 'off', attributes: { device_class: 'running' } }],
    [`sensor.${P}_boost_time_remaining`, 'boost_remaining', { state: '0', attributes: { unit_of_measurement: 'min' } }],
    [`sensor.${P}_last_boost_result`, 'boost_last_result', { state: 'Target_reached', attributes: { device_class: 'enum' } }],
  ];
}

/**
 * Build a hass object. `opts.withTk = false` drops translation_key (older HA), `opts.patch`
 * maps entity_id -> partial state to merge. `calls` records callService invocations.
 */
export function makeHass(opts = {}) {
  const { withTk = true, patch = {}, unit = '°F' } = opts;
  const states = {};
  const entities = {};
  for (const [id, tk, st] of baseEntities()) {
    const p = patch[id] ?? {};
    states[id] = { entity_id: id, ...st, ...p, attributes: { ...st.attributes, ...(p.attributes ?? {}) } };
    entities[id] = { entity_id: id, device_id: DEV, platform: 'zha', ...(withTk && tk ? { translation_key: tk } : {}) };
  }
  // An unrelated device that must not be picked up.
  states['climate.other'] = { entity_id: 'climate.other', state: 'cool', attributes: {} };
  entities['climate.other'] = { entity_id: 'climate.other', device_id: 'dev_other', platform: 'zha' };
  const calls = [];
  return {
    calls,
    states,
    entities,
    devices: {
      [DEV]: { id: DEV, name: 'Connie thermostat V3', name_by_user: 'Thermostat', manufacturer: 'Connie', model: 'thermostat-v3' },
      dev_other: { id: 'dev_other', name: 'Other', manufacturer: 'Acme', model: 'x' },
    },
    config: { unit_system: { temperature: unit }, time_zone: 'America/Los_Angeles' },
    themes: { darkMode: false },
    language: 'en',
    locale: { language: 'en' },
    callService(domain, service, data) {
      calls.push({ domain, service, data });
      if (domain === 'number' && service === 'set_value') {
        const st = states[data?.entity_id];
        const min = Number(st?.attributes?.min);
        const max = Number(st?.attributes?.max);
        const v = Number(data?.value);
        // Home Assistant rejects a number past the entity's min or max (out_of_range).
        if ((Number.isFinite(min) && v < min) || (Number.isFinite(max) && v > max))
          return Promise.reject(new Error(`out_of_range: ${v} is outside ${min}..${max}`));
      }
      return Promise.resolve();
    },
  };
}

export const DEVICE_ID = DEV;
export const PREFIX = P;
