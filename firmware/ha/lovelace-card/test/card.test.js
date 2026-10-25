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
const cardCss = () => [customElements.get('connie-thermostat-card').styles].flat().map((s) => s.cssText ?? String(s)).join('\n');
const shadowCss = (el) => [...el.shadowRoot.adoptedStyleSheets].flatMap((sheet) => [...sheet.cssRules].map((rule) => rule.cssText)).join('\n');
const sectionOpen = (el) => Object.fromEntries($$(el, 'section').map((s) => [s.querySelector('h3').textContent, !s.classList.contains('closed')]));
const sectionHeader = (el, title) => $$(el, '.sec-h').find((b) => b.querySelector('h3').textContent === title);
/** localStorage key for one card instance, namespaced and scoped by its climate entity. */
const sectionStorageKey = (entityId) => `connie-thermostat-card:sections:${entityId}`;
const flushAsync = async () => {
  for (let i = 0; i < 5; i++) await Promise.resolve();
};
const button = (el, label) => $$(el, 'button').find((b) => b.getAttribute('aria-label') === label || b.textContent.trim() === label);

describe('card', () => {
  beforeEach(() => {
    vi.useFakeTimers();
    localStorage.clear();
  });
  afterEach(() => {
    vi.useRealTimers();
    vi.restoreAllMocks();
    document.body.innerHTML = '';
    document.documentElement.className = '';
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
    expect($$(el, '.controls .seg button').map((b) => b.textContent.trim())).toEqual(['Off', 'Heat', 'Cool']);
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
    vi.advanceTimersByTime(450);
    expect(el.hass.calls).toEqual([]);
    vi.advanceTimersByTime(50);
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

  it('puts help text in tooltips: hover, focus, click to pin, Esc and outside click close', async () => {
    const el = await mount();
    expect($(el, '.caption')).toBeNull();
    const w = $(el, '.info-b[data-tip="setpoint"]');
    const btn = w.querySelector('button.info');
    const tip = $(el, '#tip-setpoint');
    expect(tip.getAttribute('role')).toBe('tooltip');
    expect(tip.textContent).toContain('Changes the Day heating program setpoint');
    expect(btn.getAttribute('aria-describedby')).toBe('tip-setpoint');
    expect($(el, '[role="slider"]').getAttribute('aria-describedby')).toBe('tip-setpoint');
    const isOpen = () => $(el, '#tip-setpoint').classList.contains('open');
    expect(isOpen()).toBe(false);

    // The in-flow setpoint tip is click/tap only: hover or focus must not shift the −/+ row.
    w.dispatchEvent(new Event('mouseenter'));
    btn.dispatchEvent(new Event('focus'));
    await el.updateComplete;
    expect(isOpen()).toBe(false);

    // Floating tips still peek on hover and focus.
    const fw = $(el, '.info-b[data-tip="tile-mb-offset"]');
    const fOpen = () => $(el, '#tip-tile-mb-offset').classList.contains('open');
    fw.dispatchEvent(new Event('mouseenter'));
    await el.updateComplete;
    expect(fOpen()).toBe(true);
    fw.dispatchEvent(new Event('mouseleave'));
    await el.updateComplete;
    expect(fOpen()).toBe(false);
    fw.querySelector('button').dispatchEvent(new Event('focus'));
    await el.updateComplete;
    expect(fOpen()).toBe(true);
    fw.querySelector('button').dispatchEvent(new Event('blur'));
    await el.updateComplete;
    expect(fOpen()).toBe(false);

    btn.click(); // pinned: survives mouseleave
    await el.updateComplete;
    expect(isOpen()).toBe(true);
    expect(btn.getAttribute('aria-expanded')).toBe('true');
    w.dispatchEvent(new Event('mouseleave'));
    await el.updateComplete;
    expect(isOpen()).toBe(true);
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    await el.updateComplete;
    expect(isOpen()).toBe(false);

    btn.click();
    await el.updateComplete;
    expect(isOpen()).toBe(true);
    tip.click(); // inside the tooltip: stays open
    await el.updateComplete;
    expect(isOpen()).toBe(true);
    document.body.click(); // outside
    await el.updateComplete;
    expect(isOpen()).toBe(false);

    btn.click();
    await el.updateComplete;
    $(el, '.info-w[data-tip="tile-mb-offset"] button.info').click(); // opening another closes the first
    await el.updateComplete;
    expect(isOpen()).toBe(false);
    expect($(el, '#tip-tile-mb-offset').classList.contains('open')).toBe(true);
  });

  it('Esc stops propagation only when it closed a tooltip', async () => {
    const el = await mount();
    let seen = 0;
    const later = () => seen++;
    window.addEventListener('keydown', later);
    const esc = () => document.body.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
    esc();
    expect(seen).toBe(1); // nothing open: HA still gets Esc
    $(el, '.info-b[data-tip="setpoint"] button.info').click();
    await el.updateComplete;
    expect(el._tip).not.toBeNull();
    esc();
    expect(seen).toBe(1); // consumed by the tooltip
    expect(el._tip).toBeNull();
    esc();
    expect(seen).toBe(2);
    window.removeEventListener('keydown', later);
  });

  it('the setpoint tooltip is in the flow; the others sit in a row or tile', async () => {
    const el = await mount();
    const tip = $(el, '#tip-setpoint');
    expect(tip.classList.contains('inline')).toBe(true);
    expect(tip.parentElement.classList.contains('setpoint')).toBe(true);
    // sits between the badge and the value, so opening it pushes the value down
    expect(tip.previousElementSibling.classList.contains('period-row')).toBe(true);
    expect(tip.nextElementSibling.classList.contains('sp-row')).toBe(true);
    for (const w of $$(el, '.info-w')) expect(w.closest('.row, .tile, .legend')).not.toBeNull();
  });

  it('keeps warnings visible, not in tooltips', async () => {
    const el = await mount({}, { patch: { [`sensor.${PREFIX}_schedule_period`]: { state: 'No_clock_holding_day' } } });
    const b = $(el, '.banner');
    expect(b.textContent).toContain('holding the day program');
    expect(b.closest('.tip')).toBeNull();
    expect($$(el, '.tip').some((t) => t.textContent.includes('holding'))).toBe(false);
  });

  it('puts boost controls above the schedule', async () => {
    const el = await mount();
    expect($$(el, 'section h3').map((h) => h.textContent)).toEqual(['Boost', 'Morning heating boost', 'Program schedule', 'Tuning & status']);
    expect($(el, '.tile[aria-labelledby="tile-boost-target"]')).not.toBeNull(); // Boost is open by default
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
    expect($(el, '.period').textContent.replace(/\s+/g, ' ')).toContain('Morning boost +1.8°F');
    el.hass = makeHass({ unit: '°C', patch: { [`sensor.${PREFIX}_schedule_period`]: { state: 'Day_morning_boost' } } });
    await el.updateComplete;
    expect($(el, '.period').textContent).toContain('Morning boost +1°C');
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
    await el.updateComplete;
    const v = button(el, 'Increase night heating temperature').previousElementSibling;
    expect(v.classList.contains('pending')).toBe(true);
    expect(v.getAttribute('aria-label')).toBe('night heating temperature: 65.5°F, sending…');
    vi.advanceTimersByTime(1000);
    expect(el.hass.calls).toEqual(expect.arrayContaining([
      { domain: 'number', service: 'set_value', data: { entity_id: `number.${PREFIX}_day_heating_start`, value: 7.25 } },
      { domain: 'number', service: 'set_value', data: { entity_id: `number.${PREFIX}_night_heating_temperature`, value: 65.5 } },
    ]));
  });

  it('shows °C differences as °F and writes °C back', async () => {
    const el = await mount();
    const row = $$(el, '.tile').find((r) => r.textContent.includes('Offset'));
    expect(row.textContent).toContain('1.8°F');
    button(el, 'Increase morning boost offset').click();
    vi.advanceTimersByTime(1000);
    expect(el.hass.calls.at(-1)).toEqual({ domain: 'number', service: 'set_value', data: { entity_id: `number.${PREFIX}_morning_heating_boost_offset`, value: 1.1 } });
  });

  it('each boost tile holds its value stepper and the button that starts it', async () => {
    const el = await mount();
    const tile = (id) => $(el, `.tile[aria-labelledby="tile-${id}"]`);
    const t = tile('boost-target');
    expect(t.getAttribute('role')).toBe('group');
    expect(el.shadowRoot.getElementById('tile-boost-target').textContent).toBe('Boost to target');
    expect(t.querySelector('.stepper .v').textContent).toBe('72°F');
    const go = t.querySelector('.go');
    expect(go.getAttribute('aria-label')).toBe('Start boost to 72°F');
    // value and button are on the same line, in the same tile
    expect(go.parentElement).toBe(t.querySelector('.stepper').parentElement);
    const timed = tile('boost-timed');
    expect(timed.querySelector('.stepper .v').textContent).toBe('30 min');
    expect(timed.querySelector('.go').getAttribute('aria-label')).toBe('Start timed boost for 30 min');
    go.click();
    await flushAsync();
    expect(el.hass.calls).toEqual([{ domain: 'button', service: 'press', data: { entity_id: `button.${PREFIX}_start_boost_to_target` } }]);
  });

  it('Start flushes a debounced value first and its label uses the pending value', async () => {
    const el = await mount();
    const timed = $(el, '.tile[aria-labelledby="tile-boost-timed"]');
    const inc = timed.querySelector('button[aria-label="Increase boost duration"]');
    inc.click();
    await el.updateComplete;
    inc.click();
    await el.updateComplete;
    const go = timed.querySelector('.go');
    expect(go.getAttribute('aria-label')).toBe('Start timed boost for 32 min');
    go.click(); // well inside the 500 ms debounce
    await flushAsync();
    expect(el.hass.calls).toEqual([
      { domain: 'number', service: 'set_value', data: { entity_id: `number.${PREFIX}_boost_duration`, value: 32 } },
      { domain: 'button', service: 'press', data: { entity_id: `button.${PREFIX}_start_timed_boost` } },
    ]);
    vi.advanceTimersByTime(1000); // the debounce timer was cleared: no second write
    expect(el.hass.calls).toHaveLength(2);
  });

  it('Start waits for a write the debounce already sent but that is not answered yet', async () => {
    const el = await mount();
    const calls = el.hass.calls;
    let release;
    el.hass.callService = (domain, service, data) => {
      calls.push({ domain, service, data });
      return service === 'set_value' ? new Promise((r) => (release = r)) : Promise.resolve();
    };
    const timed = $(el, '.tile[aria-labelledby="tile-boost-timed"]');
    timed.querySelector('button[aria-label="Increase boost duration"]').click();
    vi.advanceTimersByTime(600); // debounce fired: set_value sent, still in flight
    expect(calls.map((c) => c.service)).toEqual(['set_value']);
    timed.querySelector('.go').click();
    await flushAsync();
    expect(calls.map((c) => c.service)).toEqual(['set_value']); // press must not overtake it
    release();
    await flushAsync();
    expect(calls.map((c) => c.service)).toEqual(['set_value', 'press']);
  });

  it('a failed flush or press clears the pending value and shows the error', async () => {
    const el = await mount();
    el.hass.callService = (domain, service) => (service === 'set_value' ? Promise.reject(new Error('Zigbee timeout')) : Promise.resolve());
    const timed = $(el, '.tile[aria-labelledby="tile-boost-timed"]');
    timed.querySelector('button[aria-label="Increase boost duration"]').click();
    await el.updateComplete;
    expect(timed.querySelector('.stepper .v').textContent).toBe('31 min');
    const pressed = [];
    const orig = el.hass.callService;
    el.hass.callService = (d, sv, data) => (sv === 'press' ? (pressed.push(data), Promise.resolve()) : orig(d, sv, data));
    timed.querySelector('.go').click();
    await flushAsync();
    await el.updateComplete;
    expect(pressed).toEqual([]); // never started with an unsaved length
    expect($(el, '.banner.error[role="alert"]').textContent).toContain("Couldn't start the boost: Zigbee timeout");
    expect($(el, '.tile[aria-labelledby="tile-boost-timed"] .stepper .v').textContent).toBe('30 min');

    el.hass.callService = (d, sv) => (sv === 'press' ? Promise.reject(new Error('device busy')) : Promise.resolve());
    $(el, '.tile[aria-labelledby="tile-boost-target"] .go').click();
    await flushAsync();
    await el.updateComplete;
    expect($(el, '.banner.error[role="alert"]').textContent).toContain("Couldn't start the boost: device busy");
    vi.advanceTimersByTime(10000);
    await el.updateComplete;
    expect($(el, '.banner.error[role="alert"]')).toBeNull();
  });

  it('a service call with no answer in 10 s counts as failed', async () => {
    const el = await mount();
    const calls = [];
    el.hass.callService = (d, sv, data) => (calls.push(sv), new Promise(() => {})); // never answers
    const timed = $(el, '.tile[aria-labelledby="tile-boost-timed"]');
    timed.querySelector('button[aria-label="Increase boost duration"]').click();
    await el.updateComplete;
    timed.querySelector('.go').click();
    await flushAsync();
    expect(calls).toEqual(['set_value']);
    vi.advanceTimersByTime(9900);
    await flushAsync();
    await el.updateComplete;
    expect($(el, '.banner.error[role="alert"]')).toBeNull();
    vi.advanceTimersByTime(200);
    await flushAsync();
    await el.updateComplete;
    expect(calls).toEqual(['set_value']); // never started
    expect($(el, '.banner.error[role="alert"]').textContent).toContain('no answer from Home Assistant after 10 s');
    expect($(el, '.tile[aria-labelledby="tile-boost-timed"] .stepper .v').textContent).toBe('30 min');
  });

  it('the error banner has an accessible dismiss button', async () => {
    const el = await mount();
    el._fail(null, new Error('boom'), 'Oops');
    await el.updateComplete;
    const b = $(el, '.banner.error[role="alert"] button.dismiss');
    expect(b.getAttribute('type')).toBe('button');
    expect(b.getAttribute('aria-label')).toBe('Dismiss error');
    b.click();
    await el.updateComplete;
    expect($(el, '.banner.error[role="alert"]')).toBeNull();
    expect(el._errTimer).toBeUndefined();
  });

  it('disconnecting clears the error timer', async () => {
    const el = await mount();
    el._fail(null, new Error('boom'), 'Oops');
    expect(vi.getTimerCount()).toBeGreaterThan(0);
    el.remove();
    expect(el._errTimer).toBeUndefined();
    expect(vi.getTimerCount()).toBe(0);
  });

  it('the running boost tile takes the heat/cool colour, with the direction also in words', async () => {
    const el = await mount();
    const run = async (state, rem = '22') => {
      const h = el.hass;
      const st = { ...h.states };
      st[`binary_sensor.${PREFIX}_boost_active`] = { ...st[`binary_sensor.${PREFIX}_boost_active`], state: 'on' };
      st[`sensor.${PREFIX}_boost`] = { ...st[`sensor.${PREFIX}_boost`], state };
      st[`sensor.${PREFIX}_boost_time_remaining`] = { ...st[`sensor.${PREFIX}_boost_time_remaining`], state: rem };
      el.hass = { ...h, states: st };
      await el.updateComplete;
      return $(el, '.tile.running');
    };
    let t = await run('Timed_heat');
    expect(t.getAttribute('aria-labelledby')).toBe('tile-boost-timed');
    expect([...t.classList]).toEqual(expect.arrayContaining(['running', 'heat']));
    expect(t.querySelector('.run-chip').textContent.trim()).toBe('Heating · 22 min left');
    t = await run('Target_cool');
    expect(t.getAttribute('aria-labelledby')).toBe('tile-boost-target');
    expect(t.classList.contains('cool')).toBe(true);
    expect(t.classList.contains('heat')).toBe(false);
    expect(t.querySelector('.run-chip').textContent.trim()).toBe('Cooling · Running');
    // The quirk's BoostState names (connie_thermostat_v3.py): Heating_to_target, Cooling_to_target, Timed_heat, Timed_cool.
    t = await run('Cooling_to_target');
    expect(t.getAttribute('aria-labelledby')).toBe('tile-boost-target');
    expect([...t.classList]).toEqual(expect.arrayContaining(['running', 'cool']));
    expect(t.classList.contains('heat')).toBe(false);
    expect(t.querySelector('.run-chip').textContent.trim()).toBe('Cooling · Running');
    t = await run('Heating_to_target');
    expect(t.getAttribute('aria-labelledby')).toBe('tile-boost-target');
    expect(t.classList.contains('heat')).toBe(true);
    t = await run('Timed_cool');
    expect(t.getAttribute('aria-labelledby')).toBe('tile-boost-timed');
    expect(t.classList.contains('cool')).toBe(true);
    t = await run('Timed'); // direction unknown: theme accent, no colour class, no direction word
    expect(t.className.trim().split(/\s+/).sort()).toEqual(['running', 'tile']);
    expect(t.querySelector('.run-chip').textContent.trim()).toBe('22 min left');
    const css = [customElements.get('connie-thermostat-card').styles].flat().map((x) => x.cssText ?? String(x)).join('');
    expect(css).toContain('.tile.running { --ct-run: var(--primary-color)');
    expect(css).toContain('.tile.running.heat { --ct-run: var(--ct-heat); }');
    expect(css).toContain('.tile.running.cool { --ct-run: var(--ct-cool); }');
    expect(css).toMatch(/\.run-chip \{[^}]*color: color-mix\(in srgb, var\(--ct-run, var\(--primary-color\)\) 55%, var\(--primary-text-color\)\)/);
    expect(css).toMatch(/\.go \{[^}]*background: color-mix\(in srgb, var\(--primary-color\) 70%, #000\)/);
  });

  it('morning boost tiles use the heat colour while it runs', async () => {
    const el = await mount();
    const h = el.hass;
    el.hass = { ...h, states: { ...h.states, [`sensor.${PREFIX}_schedule_period`]: { ...h.states[`sensor.${PREFIX}_schedule_period`], state: 'Day_morning_boost' } } };
    await el.updateComplete;
    const tiles = $$(el, '.tile[aria-labelledby^="tile-mb-"]');
    expect(tiles.length).toBe(2);
    for (const t of tiles) expect([...t.classList]).toEqual(expect.arrayContaining(['running', 'heat']));
  });

  it('tints the top only while actively heating or cooling (hvac_action)', async () => {
    const el = await mount();
    const set = async (state, action) => {
      const h = el.hass;
      const c = h.states[`climate.${PREFIX}`];
      el.hass = { ...h, states: { ...h.states, [`climate.${PREFIX}`]: { ...c, state, attributes: { ...c.attributes, hvac_action: action } } } };
      await el.updateComplete;
      return el.getAttribute('action');
    };
    expect(await set('heat', 'heating')).toBe('heating');
    expect(await set('cool', 'cooling')).toBe('cooling');
    expect(await set('heat', 'idle')).toBeNull();
    expect(await set('off', 'off')).toBeNull();
    expect(await set('off', 'fan')).toBeNull();
    expect(await set('unavailable', 'heating')).toBeNull();
    const css = [customElements.get('connie-thermostat-card').styles].flat().map((x) => x.cssText ?? String(x)).join('');
    expect(css).toMatch(/:host\(\[action='heating'\]\)\s*\{\s*--ct-tint: var\(--ct-heat\)/);
    expect(css).toMatch(/:host\(\[action='cooling'\]\)\s*\{\s*--ct-tint: var\(--ct-cool\)/);
    expect(css).toMatch(/color-mix\(in srgb, var\(--ct-tint\) 16%, transparent\)/);
  });

  it('Start, Stop and Direction are disabled while the thermostat is unavailable', async () => {
    const el = await mount({}, { patch: { [`climate.${PREFIX}`]: { state: 'unavailable' }, [`binary_sensor.${PREFIX}_boost_active`]: { state: 'on' }, [`sensor.${PREFIX}_boost`]: { state: 'Timed_heat' } } });
    const gos = $$(el, '.tile .go');
    expect(gos).toHaveLength(2);
    expect(gos.every((b) => b.disabled)).toBe(true);
    expect(gos.some((b) => b.classList.contains('stop'))).toBe(true);
    expect($$(el, '.mini-seg button').every((b) => b.disabled)).toBe(true);
  });

  it('Start/Stop is one persistent button: focus survives and only its name changes', async () => {
    const el = await mount();
    const btn = $(el, '.tile[aria-labelledby="tile-boost-timed"] .go');
    btn.focus();
    el.hass = makeHass({ patch: { [`binary_sensor.${PREFIX}_boost_active`]: { state: 'on' }, [`sensor.${PREFIX}_boost`]: { state: 'Timed_heat' } } });
    await el.updateComplete;
    const after = $(el, '.tile[aria-labelledby="tile-boost-timed"] .go');
    expect(after).toBe(btn);
    expect(after.getAttribute('aria-label')).toBe('Stop timed boost');
    expect(el.shadowRoot.activeElement).toBe(btn);
  });

  it('a running boost turns its own tile button into Stop; direction is a segmented control', async () => {
    const el = await mount({}, { patch: { [`binary_sensor.${PREFIX}_boost_active`]: { state: 'on' }, [`sensor.${PREFIX}_boost`]: { state: 'Timed_heat' }, [`sensor.${PREFIX}_boost_time_remaining`]: { state: '12' } } });
    expect(text(el)).toContain('Timed heat · 12 min left');
    const timed = $(el, '.tile[aria-labelledby="tile-boost-timed"]');
    expect(timed.classList.contains('running')).toBe(true);
    expect(timed.querySelector('.run-chip').textContent).toContain('12 min left');
    const target = $(el, '.tile[aria-labelledby="tile-boost-target"]');
    expect(target.classList.contains('running')).toBe(false);
    // The firmware replaces a running boost (CtrlBoostStart), so the other tile offers a switch.
    expect(target.querySelector('.go').textContent.trim()).toBe('Switch');
    expect(target.querySelector('.go').getAttribute('aria-label')).toBe('Switch to boost to 72°F');
    expect(target.querySelector('.go').disabled).toBe(false);
    expect(timed.querySelector('.go').getAttribute('aria-label')).toBe('Stop timed boost');
    timed.querySelector('.go.stop').click();
    const dir = $$(el, '.mini-seg button');
    expect(dir.map((b) => b.textContent.trim())).toEqual(['Auto', 'Heat', 'Cool']);
    expect(dir[0].getAttribute('aria-pressed')).toBe('true');
    dir[1].click();
    expect(el.hass.calls).toEqual([
      { domain: 'button', service: 'press', data: { entity_id: `button.${PREFIX}_cancel_boost` } },
      { domain: 'select', service: 'select_option', data: { entity_id: `select.${PREFIX}_boost_mode`, option: 'Heat' } },
    ]);
  });

  it('morning boost: each value is a tile with its own stepper', async () => {
    const el = await mount();
    for (const [id, v] of [['mb-duration', '45 min'], ['mb-offset', '1.8°F']]) {
      const t = $(el, `.tile[aria-labelledby="tile-${id}"]`);
      expect(t.querySelector('.stepper .v').textContent).toBe(v);
      expect(t.querySelector('.info')).not.toBeNull();
    }
  });

  it('warns on faults and unavailability', async () => {
    const el = await mount({}, { patch: { [`climate.${PREFIX}`]: { state: 'unavailable' }, [`sensor.${PREFIX}_last_boost_result`]: { state: 'Stopped_by_failsafe' } } });
    expect(text(el)).toContain('Thermostat is unavailable');
    expect(text(el)).toContain('Last boost: Stopped by failsafe');
    expect($$(el, '.controls .seg button').every((b) => b.disabled)).toBe(true);
  });

  it('explains when no device is found', async () => {
    const el = document.createElement('connie-thermostat-card');
    el.setConfig({});
    el.hass = { ...makeHass(), devices: {}, entities: {} };
    document.body.appendChild(el);
    await el.updateComplete;
    expect(text(el)).toContain('No Connie thermostat V3 found');
  });

  it('remembers each section on its own for the climate entity', async () => {
    const key = sectionStorageKey(`climate.${PREFIX}`);
    const el = await mount();
    expect(sectionOpen(el)).toEqual({ Boost: true, 'Morning heating boost': true, 'Program schedule': true, 'Tuning & status': false });
    sectionHeader(el, 'Boost').click();
    sectionHeader(el, 'Tuning & status').click();
    await el.updateComplete;
    expect(sectionOpen(el)).toEqual({ Boost: false, 'Morning heating boost': true, 'Program schedule': true, 'Tuning & status': true });
    // A hass refresh re-renders the card; it must not snap back to the defaults.
    el.hass = makeHass();
    await el.updateComplete;
    expect(sectionOpen(el)).toEqual({ Boost: false, 'Morning heating boost': true, 'Program schedule': true, 'Tuning & status': true });
    const saved = JSON.parse(localStorage.getItem(key));
    expect(saved.boost).toBe(false);
    expect(saved.status).toBe(true);
    expect(saved.morning).not.toBe(false);
    expect(saved.schedule).not.toBe(false);
    el.remove();

    const again = await mount({ collapsed: ['morning'] });
    // Remembered boost/status win over defaults and over collapsed; morning was never stored.
    expect(sectionOpen(again)).toEqual({ Boost: false, 'Morning heating boost': false, 'Program schedule': true, 'Tuning & status': true });
  });

  it('keeps section state per climate entity so two cards do not share it', async () => {
    const otherId = 'climate.other_room';
    const el = await mount();
    sectionHeader(el, 'Boost').click();
    await el.updateComplete;

    const hass = makeHass();
    hass.states[otherId] = { ...hass.states[`climate.${PREFIX}`], entity_id: otherId };
    hass.entities[otherId] = { entity_id: otherId, device_id: DEVICE_ID, platform: 'zha' };
    const other = document.createElement('connie-thermostat-card');
    other.setConfig({ type: 'custom:connie-thermostat-card', entities: { climate: otherId } });
    other.hass = hass;
    other.nowMinutes = 10 * 60;
    document.body.appendChild(other);
    await other.updateComplete;
    expect(sectionOpen(other).Boost).toBe(true);
    sectionHeader(other, 'Morning heating boost').click();
    await other.updateComplete;

    expect(sectionOpen(el)).toMatchObject({ Boost: false, 'Morning heating boost': true });
    expect(sectionOpen(other)).toMatchObject({ Boost: true, 'Morning heating boost': false });
    expect(JSON.parse(localStorage.getItem(sectionStorageKey(`climate.${PREFIX}`))).boost).toBe(false);
    const otherSaved = JSON.parse(localStorage.getItem(sectionStorageKey(otherId)));
    expect(otherSaved.morning).toBe(false);
    expect(otherSaved.boost).not.toBe(false);
  });

  it('uses the default sections when storage is missing, unreadable, or throws', async () => {
    localStorage.setItem(sectionStorageKey(`climate.${PREFIX}`), '{');
    let el = await mount({ collapsed: ['boost'] });
    expect(sectionOpen(el)).toEqual({ Boost: false, 'Morning heating boost': true, 'Program schedule': true, 'Tuning & status': false });
    el.remove();
    localStorage.clear();

    vi.spyOn(Storage.prototype, 'getItem').mockImplementation(() => {
      throw new Error('blocked');
    });
    vi.spyOn(Storage.prototype, 'setItem').mockImplementation(() => {
      throw new Error('blocked');
    });
    el = await mount({ collapsed: ['schedule'] });
    expect(sectionOpen(el)).toEqual({ Boost: true, 'Morning heating boost': true, 'Program schedule': false, 'Tuning & status': false });
    sectionHeader(el, 'Boost').click();
    sectionHeader(el, 'Tuning & status').click();
    await el.updateComplete;
    expect(sectionOpen(el)).toEqual({ Boost: false, 'Morning heating boost': true, 'Program schedule': false, 'Tuning & status': true });
    el.hass = makeHass();
    await el.updateComplete;
    expect(sectionOpen(el)).toEqual({ Boost: false, 'Morning heating boost': true, 'Program schedule': false, 'Tuning & status': true });
  });

  it('does not let a later update wipe sections the demo opened by assigning _open', async () => {
    const el = document.createElement('connie-thermostat-card');
    el.setConfig({ type: 'custom:connie-thermostat-card' });
    el.hass = makeHass();
    el._open = { boost: false, morning: false, schedule: true, status: true };
    el.nowMinutes = 10 * 60;
    document.body.appendChild(el);
    await el.updateComplete;
    expect(sectionOpen(el)).toEqual({ Boost: false, 'Morning heating boost': false, 'Program schedule': true, 'Tuning & status': true });
  });

  it('lets text in the shadow root be selected in light and dark themes', async () => {
    const css = cardCss();
    // hui-root's user-select: none inherits into the shadow tree; the host resets it.
    expect(css).toMatch(/:host\s*\{[^}]*user-select:\s*text/);
    // One rule for press targets (buttons include section headers, steppers, segments, toggles) and their contents.
    expect(css).toMatch(/button[^{]*\{[^}]*user-select:\s*none/);

    for (const theme of ['light', 'dark']) {
      localStorage.clear();
      document.documentElement.className = theme;
      const wrap = document.createElement('div');
      wrap.style.userSelect = 'none';
      document.body.appendChild(wrap);
      const el = document.createElement('connie-thermostat-card');
      el.setConfig({ type: 'custom:connie-thermostat-card' });
      el.hass = makeHass({ patch: { [`sensor.${PREFIX}_schedule_period`]: { state: 'No_clock_holding_day' } } });
      el.nowMinutes = 10 * 60;
      wrap.appendChild(el);
      await el.updateComplete;
      $(el, '.info-b[data-tip="setpoint"] button.info').click();
      sectionHeader(el, 'Tuning & status').click();
      await el.updateComplete;

      const shadow = shadowCss(el);
      expect(shadow).toMatch(/:host\s*\{[^}]*user-select:\s*text/);
      expect(shadow).toMatch(/button[^{]*\{[^}]*user-select:\s*none/);
      expect(getComputedStyle(el).userSelect).toBe('text');

      const selectable = ['.room .t', '.room .h', '.sub', '.sp-val', '.period', '.tip', '.banner span', '.stepper .v', '.tile-h span', '.legend', '.row .r'];
      for (const sel of selectable) {
        const node = $(el, sel);
        expect(node, sel).not.toBeNull();
        expect(getComputedStyle(node).userSelect, `${theme} ${sel}`).not.toBe('none');
      }
      for (const sel of ['.sec-h', '.round', '.seg button', '.fanbtn', '.mini', '.go', '.info', 'button svg']) {
        expect(getComputedStyle($(el, sel)).userSelect, `${theme} ${sel}`).toBe('none');
      }
      el.remove();
    }
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
