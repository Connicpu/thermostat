// Visual config editor. Uses HA's <ha-form> (device + entity selectors) when it is loaded,
// and falls back to plain inputs otherwise (e.g. in tests or before HA lazy-loads it).
import { LitElement, css, html, nothing } from 'lit';
import { ENTITY_KEYS, MANUFACTURER, MODEL, discoverEntities, findDevice } from './model.js';

const LABELS = {
  device_id: 'Thermostat device',
  name: 'Name (optional)',
  show_schedule: 'Show program schedule',
  show_morning_boost: 'Show morning boost',
  show_manual_boost: 'Show boost controls',
  show_status: 'Show tuning & status',
};

const TOGGLES = ['show_schedule', 'show_morning_boost', 'show_manual_boost', 'show_status'];

/** ha-form schema for the editor (exported for tests). */
export function editorSchema() {
  return [
    { name: 'device_id', selector: { device: { filter: [{ manufacturer: MANUFACTURER, model: MODEL }] } } },
    { name: 'name', selector: { text: {} } },
    { type: 'grid', name: '', schema: TOGGLES.map((name) => ({ name, selector: { boolean: {} } })) },
    {
      type: 'expandable',
      name: 'entities',
      title: 'Entity overrides (auto-discovered from the device)',
      schema: Object.entries(ENTITY_KEYS).map(([name, s]) => ({ name, selector: { entity: { domain: s.domain } } })),
    },
  ];
}

export class ConnieThermostatCardEditor extends LitElement {
  static properties = { hass: { attribute: false }, _config: { state: true } };

  static styles = css`
    .found { font-size: 13px; color: var(--secondary-text-color); margin-top: 8px; }
    label { display: block; margin: 8px 0; }
    input[type='text'] { width: 100%; box-sizing: border-box; }
  `;

  setConfig(config) {
    this._config = { ...config };
  }

  /** Emit the new config the way Lovelace editors do. */
  _emit(config) {
    this._config = config;
    this.dispatchEvent(new CustomEvent('config-changed', { detail: { config }, bubbles: true, composed: true }));
  }

  /** Merge a form value into the config, dropping empty overrides and defaults. */
  _changed(value) {
    const next = { ...this._config, ...value };
    if (value.entities) {
      const ents = Object.fromEntries(Object.entries(value.entities).filter(([, v]) => v));
      if (Object.keys(ents).length) next.entities = ents;
      else delete next.entities;
    }
    for (const k of ['name', 'device_id']) if (!next[k]) delete next[k];
    for (const k of TOGGLES) if (next[k] === true) delete next[k];
    this._emit(next);
  }

  render() {
    if (!this.hass || !this._config) return nothing;
    const devId = this._config.device_id || findDevice(this.hass);
    const found = discoverEntities(this.hass, devId, this._config.entities);
    const total = Object.keys(ENTITY_KEYS).length;
    const data = {
      ...Object.fromEntries(TOGGLES.map((k) => [k, true])),
      ...this._config,
      device_id: this._config.device_id ?? '',
      entities: this._config.entities ?? {},
    };
    const summary = html`<div class="found">${devId ? `Found ${Object.keys(found).length} of ${total} entities on this device.` : 'No Connie thermostat V3 device found.'}</div>`;
    if (customElements.get('ha-form')) {
      return html`<ha-form .hass=${this.hass} .data=${data} .schema=${editorSchema()}
          .computeLabel=${(s) => LABELS[s.name] ?? s.name.replace(/_/g, ' ')}
          @value-changed=${(e) => this._changed(e.detail.value)}></ha-form>${summary}`;
    }
    return html`
      <label>${LABELS.device_id}<input type="text" name="device_id" .value=${data.device_id}
        @change=${(e) => this._changed({ device_id: e.target.value.trim() })} /></label>
      <label>${LABELS.name}<input type="text" name="name" .value=${data.name ?? ''}
        @change=${(e) => this._changed({ name: e.target.value })} /></label>
      ${TOGGLES.map((k) => html`<label><input type="checkbox" name=${k} .checked=${data[k] !== false}
        @change=${(e) => this._changed({ [k]: e.target.checked })} /> ${LABELS[k]}</label>`)}
      ${summary}`;
  }
}

if (!customElements.get('connie-thermostat-card-editor')) customElements.define('connie-thermostat-card-editor', ConnieThermostatCardEditor);
