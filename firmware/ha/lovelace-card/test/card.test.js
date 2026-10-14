import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import '../src/connie-thermostat-card.js';
import { editorSchema } from '../src/editor.js';
import { DEVICE_ID, PREFIX, makeHass } from './mock-hass.js';

async function mount(config = {}, hassOpts = {}) {
  const el = document.createElement('connie-thermostat-card');
  el.setConfig({ type: 'custom:connie-thermostat-card', ...config });
  el.hass = makeHass(hassOpts);
  el.nowMinutes = 10 * 60;
  document.body.appendChild(el);
  await el.updateComplete;
  return el;
}

const $ = (el, sel) => el.shadowRoot.querySelector(sel);
const $$ = (el, sel) => [...el.shadowRoot.querySelectorAll(sel)];
const text = (el) => el.shadowRoot.textContent.replace(/\s+/g, ' ');
const button = (el, label) => $$(el, 'button').find((b) => b.getAttribute('aria-label') === label || b.textContent.trim() === label);

describe('card', () => {
  beforeEach(() => vi.useFakeTimers());
  afterEach(() => {
    vi.useRealTimers();
    document.body.innerHTML = '';
  });

  it('registers in the card picker and offers a stub config', () => {
    expect(window.customCards.find((c) => c.type === 'connie-thermostat-card')).toBeTruthy();
    const C = customElements.get('connie-thermostat-card');
    expect(C.getStubConfig(makeHass())).toEqual({ device_id: DEVICE_ID });
    expect(C.getConfigElement().tagName.toLowerCase()).toBe('connie-thermostat-card-editor');
  });

  it('auto-discovers without a device_id and shows temps in °F', async () => {
    const el = await mount();
    const t = text(el);
    expect(t).toContain('68.8°F');
    expect(t).toContain('41%');
    expect($(el, '.sp-val').textContent).toContain('70');
    expect(t).toContain('Changes the Day heating program setpoint');
    expect($(el, '.period').textContent).toContain('06:30–22:00');
  });

  it('shows only Off / Heat / Cool and the running state', async () => {
    const el = await mount({}, { patch: { [`climate.${PREFIX}`]: { attributes: { hvac_modes: ['off', 'heat_cool', 'heat', 'cool', 'auto'] } } } });
    expect($$(el, '.seg button').map((b) => b.textContent.trim())).toEqual(['Off', 'Heat', 'Cool']);
    expect($(el, '.chip.heat').classList.contains('on')).toBe(true);
    expect($(el, '.chip.cool').classList.contains('on')).toBe(false);
    expect($(el, '.seg .heat').getAttribute('aria-pressed')).toBe('true');
  });

  it('debounces setpoint steps into one set_temperature', async () => {
    const el = await mount();
    button(el, 'Raise setpoint').click();
    await el.updateComplete;
    button(el, 'Raise setpoint').click();
    await el.updateComplete;
    expect($(el, '.sp-val').textContent).toContain('72');
    expect(el.hass.calls).toEqual([]);
    vi.advanceTimersByTime(1000);
    expect(el.hass.calls).toEqual([{ domain: 'climate', service: 'set_temperature', data: { entity_id: `climate.${PREFIX}`, temperature: 72 } }]);
  });

  it('setpoint is a keyboard slider routed through _stepSetpoint', async () => {
    const el = await mount();
    const sl = $(el, '[role="slider"]');
    expect(sl.getAttribute('tabindex')).toBe('0');
    expect(sl.getAttribute('aria-valuenow')).toBe('70');
    expect(sl.getAttribute('aria-valuemin')).toBe('50');
    expect(sl.getAttribute('aria-valuemax')).toBe('90');
    expect(sl.getAttribute('aria-valuetext')).toBe('70 °F, Day heating');
    const spy = vi.spyOn(el, '_stepSetpoint');
    const key = async (k) => {
      const ev = new KeyboardEvent('keydown', { key: k, bubbles: true, cancelable: true });
      $(el, '[role="slider"]').dispatchEvent(ev);
      await el.updateComplete;
      return ev.defaultPrevented;
    };
    expect(await key('ArrowUp')).toBe(true);
    expect($(el, '[role="slider"]').getAttribute('aria-valuenow')).toBe('71');
    expect($(el, '[role="slider"]').getAttribute('aria-valuetext')).toBe('71 °F, Day heating, sending…');
    await key('ArrowLeft');
    await key('PageUp');
    expect($(el, '[role="slider"]').getAttribute('aria-valuenow')).toBe('75');
    await key('PageDown');
    await key('ArrowRight');
    await key('End');
    expect($(el, '[role="slider"]').getAttribute('aria-valuenow')).toBe('90');
    await key('Home');
    expect($(el, '[role="slider"]').getAttribute('aria-valuenow')).toBe('50');
    expect(await key('a')).toBe(false);
    expect(spy.mock.calls).toEqual([[1, 1], [-1, 1], [1, 5], [-1, 5], [1, 1], [1, 19], [-1, 40]]);
    vi.advanceTimersByTime(1000);
    expect(el.hass.calls).toEqual([{ domain: 'climate', service: 'set_temperature', data: { entity_id: `climate.${PREFIX}`, temperature: 50 } }]);
  });

  it('labels the night cooling period when cooling at night', async () => {
    const el = await mount({}, { patch: { [`climate.${PREFIX}`]: { state: 'cool', attributes: { hvac_action: 'idle', temperature: 72 } }, [`sensor.${PREFIX}_schedule_period`]: { state: 'Night' } } });
    expect(text(el)).toContain('Changes the Night cooling program setpoint');
    const active = $$(el, '.slot.active');
    expect(active).toHaveLength(1);
    expect(active[0].classList.contains('cool')).toBe(true);
    expect(active[0].textContent).toContain('Night');
  });

  it('marks morning boost and missing clock', async () => {
    let el = await mount({}, { patch: { [`sensor.${PREFIX}_schedule_period`]: { state: 'Day_morning_boost' } } });
    expect($(el, '.period').textContent).toContain('Morning boost');
    el = await mount({}, { patch: { [`sensor.${PREFIX}_schedule_period`]: { state: 'No_clock_holding_day' } } });
    expect(text(el)).toContain('holding the day program');
  });

  it('sets modes and toggles the fan', async () => {
    const el = await mount({ device_id: DEVICE_ID });
    button(el, 'Cool').click();
    button(el, 'Fan always on').click();
    expect(el.hass.calls).toEqual([
      { domain: 'climate', service: 'set_hvac_mode', data: { entity_id: `climate.${PREFIX}`, hvac_mode: 'cool' } },
      { domain: 'climate', service: 'set_fan_mode', data: { entity_id: `climate.${PREFIX}`, fan_mode: 'on' } },
    ]);
  });

  it('writes schedule times in hours and temps via number.set_value', async () => {
    const el = await mount();
    const input = $(el, 'input[aria-label="day heating start"]');
    expect(input.value).toBe('06:30');
    input.value = '07:10';
    input.dispatchEvent(new Event('change'));
    button(el, 'Increase night heating temperature').click();
    vi.advanceTimersByTime(1000);
    expect(el.hass.calls).toEqual(expect.arrayContaining([
      { domain: 'number', service: 'set_value', data: { entity_id: `number.${PREFIX}_day_heating_start`, value: 7.25 } },
      { domain: 'number', service: 'set_value', data: { entity_id: `number.${PREFIX}_night_heating_temperature`, value: 65.5 } },
    ]));
  });

  it('shows °C differences as °F and writes °C back', async () => {
    const el = await mount();
    const row = $$(el, '.row').find((r) => r.textContent.includes('Offset'));
    expect(row.textContent).toContain('1.8°F');
    button(el, 'Increase morning boost offset').click();
    vi.advanceTimersByTime(1000);
    expect(el.hass.calls.at(-1)).toEqual({ domain: 'number', service: 'set_value', data: { entity_id: `number.${PREFIX}_morning_heating_boost_offset`, value: 1.1 } });
  });

  it('starts and cancels boosts, sets the direction', async () => {
    const el = await mount({}, { patch: { [`binary_sensor.${PREFIX}_boost_active`]: { state: 'on' }, [`sensor.${PREFIX}_boost`]: { state: 'Timed_heat' }, [`sensor.${PREFIX}_boost_time_remaining`]: { state: '12' } } });
    expect(text(el)).toContain('Timed heat · 12 min left');
    $$(el, '.sec-h').find((b) => b.querySelector('h3').textContent === 'Boost').click();
    await el.updateComplete;
    button(el, 'Timed').click();
    button(el, 'Cancel').click();
    const sel = $(el, 'select[aria-label="Boost direction"]');
    sel.value = 'Heat';
    sel.dispatchEvent(new Event('change'));
    expect(el.hass.calls).toEqual([
      { domain: 'button', service: 'press', data: { entity_id: `button.${PREFIX}_start_timed_boost` } },
      { domain: 'button', service: 'press', data: { entity_id: `button.${PREFIX}_cancel_boost` } },
      { domain: 'select', service: 'select_option', data: { entity_id: `select.${PREFIX}_boost_mode`, option: 'Heat' } },
    ]);
  });

  it('warns on faults and unavailability', async () => {
    const el = await mount({}, { patch: { [`climate.${PREFIX}`]: { state: 'unavailable' }, [`sensor.${PREFIX}_last_boost_result`]: { state: 'Stopped_by_failsafe' } } });
    expect(text(el)).toContain('Thermostat is unavailable');
    expect(text(el)).toContain('Last boost: Stopped by failsafe');
    expect($$(el, '.seg button').every((b) => b.disabled)).toBe(true);
  });

  it('explains when no device is found', async () => {
    const el = document.createElement('connie-thermostat-card');
    el.setConfig({});
    el.hass = { ...makeHass(), devices: {}, entities: {} };
    document.body.appendChild(el);
    await el.updateComplete;
    expect(text(el)).toContain('No Connie thermostat V3 found');
  });
});

describe('editor', () => {
  it('has a device selector filtered to the thermostat and an override per entity', () => {
    const s = editorSchema();
    expect(s[0].selector.device.filter[0]).toEqual({ manufacturer: 'Connie', model: 'thermostat-v3' });
    expect(s.find((x) => x.name === 'entities').schema.length).toBeGreaterThan(20);
  });

  it('emits config-changed without defaults or empty overrides', async () => {
    const ed = document.createElement('connie-thermostat-card-editor');
    ed.hass = makeHass();
    ed.setConfig({ type: 'custom:connie-thermostat-card' });
    document.body.appendChild(ed);
    await ed.updateComplete;
    expect(ed.shadowRoot.textContent).toContain('Found 24 of 24 entities');
    const got = [];
    ed.addEventListener('config-changed', (e) => got.push(e.detail.config));
    ed._changed({ device_id: DEVICE_ID, show_status: false, show_schedule: true, entities: { humidity: '', climate: 'climate.x' } });
    expect(got[0]).toEqual({ type: 'custom:connie-thermostat-card', device_id: DEVICE_ID, show_status: false, entities: { climate: 'climate.x' } });
  });
});
