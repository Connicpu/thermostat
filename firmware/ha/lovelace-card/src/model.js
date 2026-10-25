// Pure helpers for the Connie thermostat card: entity discovery, units, schedule maths.
// No DOM, no Lit, so they're unit-tested directly.

export const MANUFACTURER = 'Connie';
export const MODEL = 'thermostat-v3';

/**
 * Card entity keys -> how to find them on the device. `tk` is the quirk's
 * translation_key (connie_thermostat_v3.py), `suffix` is the entity_id tail ZHA
 * derives from the fallback name, used when the registry has no translation_key.
 */
export const ENTITY_KEYS = {
  climate: { domain: 'climate' },
  humidity: { domain: 'sensor', deviceClass: 'humidity', suffix: 'humidity' },
  day_heat_start: { domain: 'number', tk: 'day_heat_start', suffix: 'day_heating_start' },
  day_heat_temp: { domain: 'number', tk: 'day_heat_temp', suffix: 'day_heating_temperature' },
  night_heat_start: { domain: 'number', tk: 'night_heat_start', suffix: 'night_heating_start' },
  night_heat_temp: { domain: 'number', tk: 'night_heat_temp', suffix: 'night_heating_temperature' },
  day_cool_start: { domain: 'number', tk: 'day_cool_start', suffix: 'day_cooling_start' },
  day_cool_temp: { domain: 'number', tk: 'day_cool_temp', suffix: 'day_cooling_temperature' },
  night_cool_start: { domain: 'number', tk: 'night_cool_start', suffix: 'night_cooling_start' },
  night_cool_temp: { domain: 'number', tk: 'night_cool_temp', suffix: 'night_cooling_temperature' },
  morning_boost_duration: { domain: 'number', tk: 'morning_boost_duration', suffix: 'morning_heating_boost_duration' },
  morning_boost_offset: { domain: 'number', tk: 'morning_boost_offset', suffix: 'morning_heating_boost_offset' },
  schedule_state: { domain: 'sensor', tk: 'schedule_state', suffix: 'schedule_period' },
  temperature_swing: { domain: 'number', tk: 'temperature_swing', suffix: 'temperature_swing' },
  // ZHA's own Thermostat number (not a quirk entity): translation_key and entity_id suffix.
  temperature_offset: { domain: 'number', tk: 'local_temperature_calibration', suffix: 'local_temperature_calibration' },
  boost_setpoint: { domain: 'number', tk: 'boost_setpoint', suffix: 'boost_target_temperature' },
  boost_duration: { domain: 'number', tk: 'boost_duration', suffix: 'boost_duration' },
  boost_mode: { domain: 'select', tk: 'boost_mode', suffix: 'boost_mode' },
  boost_start_target: { domain: 'button', tk: 'boost_start_target', suffix: 'start_boost_to_target' },
  boost_start_timed: { domain: 'button', tk: 'boost_start_timed', suffix: 'start_timed_boost' },
  boost_cancel: { domain: 'button', tk: 'boost_cancel', suffix: 'cancel_boost' },
  boost_state: { domain: 'sensor', tk: 'boost_state', suffix: 'boost' },
  boost_active: { domain: 'binary_sensor', tk: 'boost_active', suffix: 'boost_active' },
  boost_remaining: { domain: 'sensor', tk: 'boost_remaining', suffix: 'boost_time_remaining' },
  boost_last_result: { domain: 'sensor', tk: 'boost_last_result', suffix: 'last_boost_result' },
};

const domainOf = (id) => id.split('.', 1)[0];

const endsWithName = (id, suffix) => id.endsWith(`_${suffix}`) || id.endsWith(`.${suffix}`);

/** Returns the ENTITY_KEYS key whose suffix is the longest one `entityId` ends with. */
function bestSuffixKey(entityId, domain) {
  let best = null;
  let len = -1;
  for (const [k, s] of Object.entries(ENTITY_KEYS))
    if (s.domain === domain && s.suffix && s.suffix.length > len && endsWithName(entityId, s.suffix)) {
      best = k;
      len = s.suffix.length;
    }
  return best;
}

/** Returns the id of the first Connie thermostat-v3 device in hass.devices, or undefined. */
export function findDevice(hass) {
  const devs = Object.values(hass?.devices ?? {});
  return devs.find((d) => d.manufacturer === MANUFACTURER && d.model === MODEL)?.id;
}

/**
 * Map every ENTITY_KEYS key to an entity_id on `deviceId`, then apply `overrides`
 * (config.entities). Keys that can't be found are left out.
 */
export function discoverEntities(hass, deviceId, overrides = {}) {
  const out = {};
  const mine = Object.values(hass?.entities ?? {}).filter((e) => deviceId && e.device_id === deviceId);
  for (const [key, spec] of Object.entries(ENTITY_KEYS)) {
    const cands = mine.filter((e) => domainOf(e.entity_id) === spec.domain);
    let hit = spec.tk && cands.find((e) => e.translation_key === spec.tk);
    if (!hit && spec.deviceClass)
      hit = cands.find((e) => hass.states[e.entity_id]?.attributes?.device_class === spec.deviceClass);
    // An entity belongs to the key with the longest matching suffix, so "boost_duration"
    // doesn't take "morning_heating_boost_duration".
    if (!hit && spec.suffix)
      hit = cands.find((e) => bestSuffixKey(e.entity_id, spec.domain) === key);
    if (!hit && !spec.tk && !spec.suffix) hit = cands[0];
    if (hit) out[key] = hit.entity_id;
  }
  for (const [k, v] of Object.entries(overrides ?? {})) if (v) out[k] = v;
  return out;
}

/** Normalised program period: 'day', 'night', 'boost' (day + morning boost), 'noclock' or null. */
export function periodOf(state) {
  const s = String(state ?? '').toLowerCase();
  if (s === 'day') return 'day';
  if (s === 'night') return 'night';
  if (s.includes('morning') || s.includes('boost')) return 'boost';
  if (s.includes('clock')) return 'noclock';
  return null;
}

/** 'boost' and 'noclock' both run the day setpoint. */
export const editsDay = (p) => p !== 'night';

/** Hours (6.5) -> minutes after midnight (390). */
export const hoursToMin = (h) => Math.round(Number(h) * 60);

/** Minutes after midnight -> "HH:MM". */
export function fmtTime(min) {
  if (!Number.isFinite(min)) return '--:--';
  const m = ((Math.round(min) % 1440) + 1440) % 1440;
  return `${String(Math.floor(m / 60)).padStart(2, '0')}:${String(m % 60).padStart(2, '0')}`;
}

/** "HH:MM" -> hours rounded to the entity step (default 0.25 h). */
export function timeToHours(str, step = 0.25) {
  const m = /^(\d{1,2}):(\d{2})/.exec(str ?? '');
  if (!m) return null;
  const h = Number(m[1]) + Number(m[2]) / 60;
  return Math.min(23.75, Math.round(h / step) * step);
}

/**
 * Temperature differences (swing, morning-boost offset, temperature offset) have no
 * device_class, so HA leaves them in °C. Returns a display factor: 1.8 when the entity is °C
 * and HA shows °F.
 */
export function deltaFactor(entityUnit, haUnit) {
  return entityUnit === '°C' && haUnit === '°F' ? 1.8 : 1;
}

/**
 * Number sent for Local temperature calibration. ZHA stores int(celsius / 0.1), and several
 * tenths (0.3, 0.6, 1.2, ...) sit just under the next integer, so the raw value would be one
 * step low. A 1e-8 bias away from zero lands on the tenth that was pressed. The card still
 * shows and remembers the unbiased 0.1 °C grid.
 */
export function calibrationWire(v) {
  if (!Number.isFinite(v) || v === 0) return v;
  return v + Math.sign(v) * 1e-8;
}

/** Round to the precision implied by `step` (0.5 -> 1 decimal, 1 -> 0). */
export function decimalsOf(step) {
  const s = String(step ?? 1);
  return s.includes('.') ? s.split('.')[1].length : 0;
}

/** Clamp v to [min, max] and snap to step (relative to min). */
export function snap(v, min, max, step) {
  const s = step > 0 ? step : 1;
  const base = Number.isFinite(min) ? min : 0;
  let r = base + Math.round((v - base) / s) * s;
  if (Number.isFinite(min)) r = Math.max(min, r);
  if (Number.isFinite(max)) r = Math.min(max, r);
  return Number(r.toFixed(decimalsOf(s) + 2));
}

/** ZHA enum state "Rejected_sensor_fault" -> "Rejected sensor fault". */
export const humanize = (s) => String(s ?? '').replace(/_+$/, '').replace(/_/g, ' ');

/** Last-boost results that deserve a warning. */
export function boostResultIsBad(s) {
  const v = String(s ?? '').toLowerCase();
  return v.startsWith('rejected') || v.includes('failsafe') || v.includes('limit');
}
