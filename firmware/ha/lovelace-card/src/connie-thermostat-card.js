// Lovelace card for Connie's Super Awesome Thermostat V3 (ZHA + connie_thermostat_v3 quirk).
// Single bundled file at runtime: Lit and the icon paths are inlined by esbuild.
import { LitElement, html, nothing, svg } from 'lit';
import * as I from './icons.js';
import { cardStyles } from './styles.js';
import {
  boostResultIsBad,
  decimalsOf,
  deltaFactor,
  discoverEntities,
  editsDay,
  findDevice,
  fmtTime,
  hoursToMin,
  humanize,
  periodOf,
  snap,
  timeToHours,
} from './model.js';
import './editor.js';

export const CARD_VERSION = '1.0.0';
const WRITE_DELAY_MS = 500;

const icon = (path, cls = '') => html`<svg class="icon ${cls}" viewBox="0 0 24 24" aria-hidden="true">${svg`<path d=${path}></path>`}</svg>`;

const num = (s) => {
  const v = Number.parseFloat(s?.state);
  return Number.isFinite(v) ? v : null;
};

export class ConnieThermostatCard extends LitElement {
  static properties = {
    hass: { attribute: false },
    _config: { state: true },
    _open: { state: true },
    _pending: { state: true },
    _tip: { state: true },
  };

  static styles = cardStyles;

  static getConfigElement() {
    return document.createElement('connie-thermostat-card-editor');
  }

  static getStubConfig(hass) {
    const device_id = findDevice(hass);
    return device_id ? { device_id } : {};
  }

  constructor() {
    super();
    this._open = { boost: true, morning: true, schedule: true, status: false };
    /** Open tooltip: { id, pinned } (pinned = opened by click/tap, survives mouseleave/blur). */
    this._tip = null;
    this._onDocClick = (ev) => {
      if (this._tip && !ev.composedPath().some((n) => n.classList?.contains('info-w') && n.dataset.tip === this._tip.id)) this._tip = null;
    };
    this._onDocKey = (ev) => {
      if (ev.key !== 'Escape' || !this._tip) return;
      ev.stopPropagation(); // only when we consumed it, so HA dialogs still get Esc otherwise
      this._tip = null;
    };
    this._pending = {};
    this._timers = {};
    /** Overridable clock (minutes after local midnight), for tests and the demo. */
    this.nowMinutes = null;
  }

  setConfig(config) {
    if (!config || typeof config !== 'object') throw new Error('Invalid configuration');
    this._config = {
      show_schedule: true,
      show_morning_boost: true,
      show_manual_boost: true,
      show_status: true,
      ...config,
    };
    if (config.collapsed) this._open = { ...this._open, ...Object.fromEntries(config.collapsed.map((k) => [k, false])) };
  }

  getCardSize() {
    return 10;
  }

  getGridOptions() {
    return { columns: 12, min_columns: 6, rows: 'auto' };
  }

  connectedCallback() {
    super.connectedCallback();
    document.addEventListener('click', this._onDocClick, true);
    document.addEventListener('keydown', this._onDocKey, true);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener('click', this._onDocClick, true);
    document.removeEventListener('keydown', this._onDocKey, true);
    for (const t of Object.values(this._timers)) clearTimeout(t);
    this._timers = {};
  }

  // ---------- data ----------

  get _deviceId() {
    return this._config?.device_id || findDevice(this.hass);
  }

  get _ents() {
    return discoverEntities(this.hass, this._deviceId, this._config?.entities);
  }

  _st(key) {
    const id = this._e?.[key];
    return id ? this.hass.states[id] : undefined;
  }

  get _haUnit() {
    return this.hass?.config?.unit_system?.temperature || '°C';
  }

  _nowMin() {
    if (Number.isFinite(this.nowMinutes)) return this.nowMinutes;
    const tz = this.hass?.config?.time_zone;
    try {
      const p = new Intl.DateTimeFormat('en-GB', { timeZone: tz, hour: '2-digit', minute: '2-digit', hourCycle: 'h23' }).formatToParts(new Date());
      return Number(p.find((x) => x.type === 'hour').value) * 60 + Number(p.find((x) => x.type === 'minute').value);
    } catch {
      const d = new Date();
      return d.getHours() * 60 + d.getMinutes();
    }
  }

  /** Value to show for `key`: an un-sent local edit, else the entity state. */
  _val(key, live) {
    return key in this._pending ? this._pending[key] : live;
  }

  willUpdate(changed) {
    if (changed.has('hass') && this.hass) {
      this._e = this._ents;
      // Drop pending edits the device has confirmed.
      const left = { ...this._pending };
      let dirty = false;
      for (const k of Object.keys(left)) {
        if (this._timers[k]) continue;
        const live = k === 'climate' ? this._st('climate')?.attributes?.temperature : num(this._st(k));
        if (live !== null && live !== undefined && Math.abs(live - left[k]) < 1e-6) {
          delete left[k];
          dirty = true;
        }
      }
      if (dirty) this._pending = left;
      const mode = this._st('climate')?.state;
      this.setAttribute('mode', mode === 'heat' || mode === 'cool' ? mode : 'off');
    }
  }

  /** Show `value` immediately and send it after WRITE_DELAY_MS of no further changes. */
  _queue(key, value, send) {
    this._pending = { ...this._pending, [key]: value };
    clearTimeout(this._timers[key]);
    this._timers[key] = setTimeout(() => {
      delete this._timers[key];
      send(value);
      // If the device never echoes it, forget the local value after a while.
      setTimeout(() => {
        if (this._pending[key] === value && !this._timers[key]) {
          const { [key]: _, ...rest } = this._pending;
          this._pending = rest;
        }
      }, 15000);
    }, WRITE_DELAY_MS);
  }

  _call(domain, service, data) {
    return this.hass.callService(domain, service, data);
  }

  _setNumber(key, value) {
    this._queue(key, value, (v) => this._call('number', 'set_value', { entity_id: this._e[key], value: v }));
  }

  _press(key) {
    this._call('button', 'press', { entity_id: this._e[key] });
  }

  _setMode(mode) {
    this._call('climate', 'set_hvac_mode', { entity_id: this._e.climate, hvac_mode: mode });
  }

  _toggleFan() {
    const c = this._st('climate');
    const on = c?.attributes?.fan_mode === 'on';
    this._call('climate', 'set_fan_mode', { entity_id: this._e.climate, fan_mode: on ? 'auto' : 'on' });
  }

  get _spStep() {
    return this._st('climate')?.attributes?.target_temp_step || (this._haUnit === '°F' ? 1 : 0.5);
  }

  /** Move the setpoint `dir` (±1) by `steps` steps, clamped to min_temp..max_temp. */
  _stepSetpoint(dir, steps = 1) {
    const a = this._st('climate')?.attributes ?? {};
    const step = this._spStep;
    const cur = this._val('climate', a.temperature);
    if (cur === null || cur === undefined) return;
    const v = snap(cur + dir * steps * step, a.min_temp, a.max_temp, step);
    if (v === cur) return;
    this._queue('climate', v, (t) => this._call('climate', 'set_temperature', { entity_id: this._e.climate, temperature: t }));
  }

  /** Slider keys: arrows ±1 step, PageUp/PageDown ±5, Home/End to min/max. */
  _spKey(ev) {
    const a = this._st('climate')?.attributes ?? {};
    const cur = this._val('climate', a.temperature);
    const step = this._spStep;
    const toEnd = (lim) => (Number.isFinite(lim) && Number.isFinite(cur) ? Math.ceil(Math.abs(cur - lim) / step) : 0);
    const map = {
      ArrowUp: [1, 1], ArrowRight: [1, 1], ArrowDown: [-1, 1], ArrowLeft: [-1, 1],
      PageUp: [1, 5], PageDown: [-1, 5], Home: [-1, toEnd(a.min_temp)], End: [1, toEnd(a.max_temp)],
    };
    const m = map[ev.key];
    if (!m) return;
    ev.preventDefault();
    if (m[1] > 0) this._stepSetpoint(m[0], m[1]);
  }

  _toggle(sec) {
    this._open = { ...this._open, [sec]: !this._open[sec] };
  }

  // ---------- render helpers ----------

  /** "70°F" / "70.5°F" (whole values drop the decimal), "1.8°F", "45 min". */
  _fmtNum(v, decimals, unit) {
    const isTemp = unit === '°F' || unit === '°C';
    const n = isTemp && Number.isInteger(Number(v.toFixed(decimals))) ? String(Math.round(v)) : v.toFixed(decimals);
    if (!unit) return n;
    return isTemp ? `${n}${unit}` : `${n} ${unit}`;
  }

  /** " +1.8°F" for the morning-boost offset in the HA unit (°C difference ×1.8 for °F), or "". */
  _boostOffsetText() {
    const s = this._st('morning_boost_offset');
    const v = num(s);
    if (v === null || v <= 0) return '';
    const f = deltaFactor(s.attributes?.unit_of_measurement, this._haUnit);
    return ` +${this._fmtNum(v * f, 1, f !== 1 ? this._haUnit : (s.attributes?.unit_of_measurement ?? ''))}`;
  }

  /** Stepper for a number entity. `delta` shows a °C difference in the HA unit. */
  _numStepper(key, { delta = false, label = '' } = {}) {
    const s = this._st(key);
    if (!s) return html`<span class="missing">n/a</span>`;
    const a = s.attributes ?? {};
    const step = Number(a.step) || 1;
    const live = num(s);
    const v = this._val(key, live);
    const unavailable = live === null;
    const f = delta ? deltaFactor(a.unit_of_measurement, this._haUnit) : 1;
    const unit = delta && f !== 1 ? this._haUnit : (a.unit_of_measurement ?? '');
    const shown = v === null ? '—' : this._fmtNum(v * f, f !== 1 ? 1 : decimalsOf(step), unit);
    const go = (dir) => this._setNumber(key, snap((v ?? 0) + dir * step, Number(a.min), Number(a.max), step));
    return html`<span class="stepper">
      <button class="mini" aria-label="Decrease ${label}" ?disabled=${unavailable || v <= Number(a.min)} @click=${() => go(-1)}>${icon(I.mdiMinus)}</button>
      <span class="v ${key in this._pending ? 'pending' : ''}" aria-label="${label}: ${shown}${key in this._pending ? ', sending…' : ''}">${shown}</span>
      <button class="mini" aria-label="Increase ${label}" ?disabled=${unavailable || v >= Number(a.max)} @click=${() => go(1)}>${icon(I.mdiPlus)}</button>
    </span>`;
  }

  _timeInput(key, label) {
    const s = this._st(key);
    if (!s) return html`<span class="missing">n/a</span>`;
    const v = this._val(key, num(s));
    const step = Number(s.attributes?.step) || 0.25;
    return html`<input type="time" step=${Math.round(step * 3600)} aria-label=${label}
      .value=${v === null ? '' : fmtTime(hoursToMin(v))}
      @change=${(ev) => {
        const h = timeToHours(ev.target.value, step);
        if (h !== null) this._setNumber(key, h);
      }} />`;
  }

  /**
   * Small (i) button with a tooltip. Opens on hover and focus (closes on leave/blur) and
   * pins on click/tap; Esc or a click elsewhere closes it. The text stays in the DOM so
   * aria-describedby works even while hidden.
   */
  _info(id, text, { align = 'row', label = 'More info' } = {}) {
    const open = this._tip?.id === id;
    const show = (pinned) => {
      if (!this._tip || this._tip.id !== id || pinned) this._tip = { id, pinned: pinned || (this._tip?.id === id && this._tip.pinned) };
    };
    const hide = () => {
      if (this._tip?.id === id && !this._tip.pinned) this._tip = null;
    };
    return html`<span class="info-w ${align}" data-tip=${id} @mouseenter=${() => show(false)} @mouseleave=${hide}>
      <button class="info" type="button" aria-label=${label} aria-describedby="tip-${id}" aria-expanded=${open ? 'true' : 'false'}
        @focus=${() => show(false)} @blur=${hide}
        @click=${(e) => {
          e.stopPropagation();
          this._tip = open && this._tip.pinned ? null : { id, pinned: true };
        }}>${icon(I.mdiInformationOutline, 'xs')}</button>
      <span class="tip ${open ? 'open' : ''}" role="tooltip" id="tip-${id}">${text}</span>
    </span>`;
  }

  _section(id, title, meta, body) {
    const open = this._open[id];
    return html`<section class=${open ? '' : 'closed'}>
      <button class="sec-h" aria-expanded=${open ? 'true' : 'false'} @click=${() => this._toggle(id)}>
        <h3>${title}</h3><span class="meta">${meta}${icon(I.mdiChevronDown, 'chev sm')}</span>
      </button>
      <div class="sec-b">${open ? body() : nothing}</div>
    </section>`;
  }

  /** Program info for the setpoint caption: which period, which program row, since when. */
  _programInfo(mode, period) {
    const fam = mode === 'cool' ? 'cool' : 'heat';
    const day = editsDay(period);
    const startKey = `${day ? 'day' : 'night'}_${fam}_start`;
    const endKey = `${day ? 'night' : 'day'}_${fam}_start`;
    const st = num(this._st(startKey));
    const en = num(this._st(endKey));
    return {
      fam,
      day,
      label: `${day ? 'Day' : 'Night'} ${fam === 'heat' ? 'heating' : 'cooling'}`,
      span: st !== null && en !== null ? `${fmtTime(hoursToMin(st))}–${fmtTime(hoursToMin(en))}` : '',
    };
  }

  // ---------- render ----------

  render() {
    if (!this._config || !this.hass) return nothing;
    const c = this._st('climate');
    if (!c) {
      return html`<ha-card><div class="content"><div class="banner error">${icon(I.mdiAlertCircleOutline)}
        <span>No Connie thermostat V3 found. Pick the device in the card editor or set <code>device_id</code>.</span></div></div></ha-card>`;
    }
    const a = c.attributes ?? {};
    const unavailable = c.state === 'unavailable' || c.state === 'unknown';
    const mode = c.state;
    const period = periodOf(this._st('schedule_state')?.state);
    const dev = this.hass.devices?.[this._deviceId];
    const name = this._config.name || dev?.name_by_user || dev?.name || a.friendly_name || 'Thermostat';
    return html`<ha-card>
      <div class="content wrap">
        ${this._renderBanners(unavailable, period)}
        ${this._renderHeader(name, c)}
        ${this._renderSetpoint(c, mode, period, unavailable)}
        ${this._renderModes(c, mode, unavailable)}
        ${this._config.show_manual_boost ? this._renderBoost() : nothing}
        ${this._config.show_morning_boost ? this._renderMorning(period) : nothing}
        ${this._config.show_schedule ? this._renderSchedule(mode, period) : nothing}
        ${this._config.show_status ? this._renderStatus() : nothing}
      </div>
    </ha-card>`;
  }

  _renderBanners(unavailable, period) {
    const out = [];
    if (unavailable) out.push(html`<div class="banner error">${icon(I.mdiAlertCircleOutline)}<span>Thermostat is unavailable (not reporting to Zigbee).</span></div>`);
    if (period === 'noclock')
      out.push(html`<div class="banner">${icon(I.mdiClockAlertOutline)}<span>No time from Home Assistant yet: holding the day program.</span></div>`);
    const res = this._st('boost_last_result')?.state;
    if (boostResultIsBad(res)) out.push(html`<div class="banner">${icon(I.mdiAlertCircleOutline)}<span>Last boost: ${humanize(res)}</span></div>`);
    return out;
  }

  _renderHeader(name, c) {
    const a = c.attributes;
    const action = a.hvac_action;
    const hum = num(this._st('humidity'));
    // ZHA reports the fan bit of ThermostatRunningState only as hvac_action 'fan' (fan alone).
    const fanOn = action === 'fan' || a.fan_mode === 'on';
    const t = a.current_temperature;
    return html`<div class="head">
      <div>
        <div class="title">${name}</div>
        <div class="room">
          <span class="t" aria-label="Room temperature">${t ?? '—'}<small>${this._haUnit}</small></span>
          ${hum !== null ? html`<span class="h">${icon(I.mdiWaterPercent, 'sm')}${Math.round(hum)}%</span>` : nothing}
        </div>
        <div class="sub">${this._actionText(action, a.fan_mode)}</div>
      </div>
      <div class="chips" aria-label="Running state">
        <span class="chip heat ${action === 'heating' ? 'on' : ''}">${icon(I.mdiFire, 'sm')}Heat</span>
        <span class="chip cool ${action === 'cooling' ? 'on' : ''}">${icon(I.mdiSnowflake, 'sm')}Cool</span>
        <span class="chip fan ${fanOn ? 'on' : ''}">${icon(I.mdiFan, 'sm')}Fan</span>
      </div>
    </div>`;
  }

  _actionText(action, fanMode) {
    const m = { heating: 'Heating', cooling: 'Cooling', fan: 'Fan only', idle: 'Idle', off: 'Off' }[action] ?? (action ? humanize(action) : '');
    return fanMode === 'on' && action !== 'fan' ? `${m}${m ? ' · ' : ''}fan always on` : m;
  }

  _renderSetpoint(c, mode, period, unavailable) {
    if (mode !== 'heat' && mode !== 'cool') {
      return html`<div class="setpoint"><div class="off-note">${unavailable ? 'Unavailable' : 'System is off. Choose Heat or Cool to run the program.'}</div></div>`;
    }
    const a = c.attributes;
    const v = this._val('climate', a.temperature);
    const info = this._programInfo(mode, period);
    const periodIcon = period === 'boost' ? I.mdiWeatherSunsetUp : info.day ? I.mdiWeatherSunny : I.mdiWeatherNight;
    const periodName = period === 'boost' ? `Morning boost${this._boostOffsetText()}` : period === 'noclock' ? 'Day (no clock)' : info.day ? 'Day' : 'Night';
    return html`<div class="setpoint">
      <span class="period-row"><span class="period">${icon(periodIcon, 'sm')}${periodName}${info.span ? html` · ${info.span}` : nothing}</span>
        ${this._info('setpoint', `Changes the ${info.label} program setpoint${period === 'boost' ? ' (the morning boost is added on top)' : ''}.`, { align: 'band', label: 'About the setpoint' })}</span>
      <div class="sp-row">
        <button class="round" aria-label="Lower setpoint" ?disabled=${unavailable || v <= a.min_temp} @click=${() => this._stepSetpoint(-1)}>${icon(I.mdiMinus)}</button>
        <span class="sp-val ${'climate' in this._pending ? 'pending' : ''}" role="slider" tabindex=${unavailable ? -1 : 0}
          aria-label="Setpoint" aria-valuenow=${v ?? nothing} aria-valuemin=${a.min_temp ?? nothing} aria-valuemax=${a.max_temp ?? nothing}
          aria-valuetext="${v ?? '—'} ${this._haUnit}, ${info.label}${'climate' in this._pending ? ', sending…' : ''}"
          aria-describedby="tip-setpoint" aria-disabled=${unavailable ? 'true' : 'false'} @keydown=${(e) => !unavailable && this._spKey(e)}>${v ?? '—'}<small>${this._haUnit}</small></span>
        <button class="round" aria-label="Raise setpoint" ?disabled=${unavailable || v >= a.max_temp} @click=${() => this._stepSetpoint(1)}>${icon(I.mdiPlus)}</button>
      </div>
    </div>`;
  }

  _renderModes(c, mode, unavailable) {
    const modes = (c.attributes.hvac_modes ?? ['off', 'heat', 'cool']).filter((m) => ['off', 'heat', 'cool'].includes(m));
    const order = ['off', 'heat', 'cool'].filter((m) => modes.includes(m));
    const lab = { off: ['Off', I.mdiPower], heat: ['Heat', I.mdiFire], cool: ['Cool', I.mdiSnowflake] };
    const fanOn = c.attributes.fan_mode === 'on';
    return html`<div class="controls">
      <div class="seg" role="group" aria-label="System mode">
        ${order.map((m) => html`<button class=${m} aria-pressed=${mode === m ? 'true' : 'false'} ?disabled=${unavailable} @click=${() => this._setMode(m)}>${icon(lab[m][1], 'sm')}${lab[m][0]}</button>`)}
      </div>
      ${(c.attributes.fan_modes ?? ['auto', 'on']).includes('on')
        ? html`<button class="fanbtn" aria-pressed=${fanOn ? 'true' : 'false'} aria-label="Fan always on" ?disabled=${unavailable} @click=${() => this._toggleFan()}>${icon(I.mdiFan, 'sm')}Fan ${fanOn ? 'On' : 'Auto'}</button>`
        : nothing}
    </div>`;
  }

  _bar(fam, period, mode) {
    const ds = num(this._st(`day_${fam}_start`));
    const ns = num(this._st(`night_${fam}_start`));
    if (ds === null || ns === null) return html`<span></span>`;
    const d = hoursToMin(ds);
    const n = hoursToMin(ns);
    const pct = (m) => `${(m / 1440) * 100}%`;
    const segs = d <= n ? [[d, n]] : [[d, 1440], [0, n]];
    const mbMin = fam === 'heat' ? num(this._st('morning_boost_duration')) ?? 0 : 0;
    const mbEnd = d + mbMin;
    const mbSegs = mbMin > 0 ? (mbEnd <= 1440 ? [[d, mbEnd]] : [[d, 1440], [0, mbEnd - 1440]]) : [];
    const now = this._nowMin();
    return html`<div class="bar ${fam}" role="img" aria-label="${fam === 'heat' ? 'Heating' : 'Cooling'}: day ${fmtTime(d)} to ${fmtTime(n)}">
      ${segs.map(([s, e]) => html`<span class="day" style="left:${pct(s)};width:${pct(e - s)}"></span>`)}
      ${mbSegs.map(([s, e]) => html`<span class="mb" style="left:${pct(s)};width:${pct(e - s)}"></span>`)}
      ${mode === fam || (mode !== 'cool' && fam === 'heat') ? html`<span class="now" style="left:calc(${pct(now)} - 1px)"></span>` : nothing}
    </div>`;
  }

  _slot(fam, which, mode, period) {
    const active = mode === fam && (which === 'day') === editsDay(period);
    const famName = fam === 'heat' ? 'heating' : 'cooling';
    return html`<div class="slot ${fam} ${active ? 'active' : ''}">
      <div class="slot-h">${icon(which === 'day' ? I.mdiWeatherSunny : I.mdiWeatherNight, 'sm')}${which === 'day' ? 'Day' : 'Night'}
        ${active ? html`<span class="now-tag">Now</span>` : nothing}</div>
      <div class="field"><span>Starts</span>${this._timeInput(`${which}_${fam}_start`, `${which} ${famName} start`)}</div>
      <div class="field"><span>Target</span>${this._numStepper(`${which}_${fam}_temp`, { label: `${which} ${famName} temperature` })}</div>
    </div>`;
  }

  _renderSchedule(mode, period) {
    const now = this._nowMin();
    const meta = mode === 'heat' || mode === 'cool' ? `${editsDay(period) ? 'Day' : 'Night'} ${mode === 'heat' ? 'heating' : 'cooling'} active` : 'Off';
    return this._section('schedule', 'Program schedule', meta, () => html`
      <div class="tl">
        <span>Heat</span>${this._bar('heat', period, mode)}
        <span>Cool</span>${this._bar('cool', period, mode)}
        <span class="ticks"><span>00</span><span>06</span><span>12</span><span>18</span><span>24</span></span>
      </div>
      <div class="legend">Now ${fmtTime(now)}${this._info('timeline', `Solid = day, light = night${num(this._st('morning_boost_duration')) ? ', hatched = morning boost' : ''}. The line marks now.`, { label: 'About the timeline' })}</div>
      <div class="wide">
        <div><div class="slot-h" style="margin:0 0 6px">${icon(I.mdiFire, 'sm')}Heating</div>
          <div class="sched">${this._slot('heat', 'day', mode, period)}${this._slot('heat', 'night', mode, period)}</div></div>
        <div><div class="slot-h" style="margin:8px 0 6px">${icon(I.mdiSnowflake, 'sm')}Cooling</div>
          <div class="sched">${this._slot('cool', 'day', mode, period)}${this._slot('cool', 'night', mode, period)}</div></div>
      </div>`);
  }

  _renderMorning(period) {
    const dur = num(this._st('morning_boost_duration'));
    const meta = period === 'boost' ? 'Running now' : dur ? `${dur} min` : 'Off';
    return this._section('morning', 'Morning heating boost', meta, () => html`<div class="rows">
      <div class="row"><span class="l">Duration${this._info('mb-duration', 'Runs from the day heating start for this long; 0 = off.', { label: 'About morning boost duration' })}</span>${this._numStepper('morning_boost_duration', { label: 'morning boost duration' })}</div>
      <div class="row"><span class="l">Offset${this._info('mb-offset', 'Added to the day heating target while the morning boost runs.', { label: 'About morning boost offset' })}</span>${this._numStepper('morning_boost_offset', { delta: true, label: 'morning boost offset' })}</div>
    </div>`);
  }

  _renderBoost() {
    const active = this._st('boost_active')?.state === 'on';
    const state = this._st('boost_state')?.state;
    const rem = num(this._st('boost_remaining'));
    const meta = active ? `${humanize(state)}${rem ? ` · ${Math.round(rem)} min left` : ''}` : 'Idle';
    const sel = this._st('boost_mode');
    return this._section('boost', 'Boost', meta, () => html`
      ${active ? html`<div class="boost-live">${icon(I.mdiThermometerChevronUp)}<span>${humanize(state)}${rem ? ` — ${Math.round(rem)} min left` : ''}</span></div>` : nothing}
      <div class="rows">
        <div class="row"><span>Target</span>${this._numStepper('boost_setpoint', { label: 'boost target' })}</div>
        <div class="row"><span>Timed length</span>${this._numStepper('boost_duration', { label: 'boost duration' })}</div>
        <div class="row"><span>Direction</span>${sel
          ? html`<select aria-label="Boost direction" @change=${(e) => this._call('select', 'select_option', { entity_id: this._e.boost_mode, option: e.target.value })}>
              ${(sel.attributes?.options ?? []).map((o) => html`<option .value=${o} ?selected=${o === sel.state}>${humanize(o)}</option>`)}</select>`
          : html`<span class="missing">n/a</span>`}</div>
      </div>
      <div class="btns">
        <button class="btn" ?disabled=${!this._e.boost_start_target} @click=${() => this._press('boost_start_target')}>${icon(I.mdiThermometerChevronUp)}To target</button>
        <button class="btn" ?disabled=${!this._e.boost_start_timed} @click=${() => this._press('boost_start_timed')}>${icon(I.mdiTimerPlayOutline)}Timed</button>
        <button class="btn" ?disabled=${!this._e.boost_cancel || !active} @click=${() => this._press('boost_cancel')}>${icon(I.mdiCancel)}Cancel</button>
      </div>`);
  }

  _renderStatus() {
    const res = this._st('boost_last_result')?.state;
    const sched = this._st('schedule_state')?.state;
    const sw = this._st('temperature_swing');
    return this._section('status', 'Tuning & status', '', () => html`<div class="rows">
      <div class="row"><span class="l">Temperature swing${this._info('swing', 'How far (±) the room may drift from the setpoint before heating or cooling switches.', { label: 'About temperature swing' })}</span>${sw ? this._numStepper('temperature_swing', { delta: true, label: 'temperature swing' }) : html`<span class="missing">n/a</span>`}</div>
      <div class="row"><span>Program period</span><span class="r">${sched ? humanize(sched) : '—'}</span></div>
      <div class="row"><span>Last boost result</span><span class="r ${boostResultIsBad(res) ? 'bad' : ''}">${res ? humanize(res) : '—'}</span></div>
    </div>`);
  }
}

if (!customElements.get('connie-thermostat-card')) customElements.define('connie-thermostat-card', ConnieThermostatCard);

window.customCards = window.customCards || [];
if (!window.customCards.some((c) => c.type === 'connie-thermostat-card')) {
  window.customCards.push({
    type: 'connie-thermostat-card',
    name: 'Connie Thermostat V3',
    description: 'Room temperature, modes, fan, program schedule, morning boost and boost for the Connie thermostat V3 (ZHA).',
    preview: true,
    documentationURL: 'https://github.com/Connicpu/thermostat/tree/main/firmware/ha/lovelace-card',
  });
}
