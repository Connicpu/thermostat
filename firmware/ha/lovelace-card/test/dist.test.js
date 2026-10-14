import { expect, it } from 'vitest';
import { makeHass } from './mock-hass.js';

// The shipped single file must work on its own (no imports left, registers both elements).
it('dist bundle is self-contained and renders', async () => {
  const fs = await import('node:fs');
  const path = await import('node:path');
  const src = fs.readFileSync(path.resolve(__dirname, '../dist/connie-thermostat-card.js'), 'utf8');
  expect(src).not.toMatch(/^\s*import\s/m);
  expect(src).not.toMatch(/https?:\/\/(cdn|unpkg|esm\.sh|jsdelivr)/);
  await import('../dist/connie-thermostat-card.js');
  expect(customElements.get('connie-thermostat-card')).toBeTruthy();
  expect(customElements.get('connie-thermostat-card-editor')).toBeTruthy();
  const el = document.createElement('connie-thermostat-card');
  el.setConfig({});
  el.hass = makeHass();
  document.body.appendChild(el);
  await el.updateComplete;
  expect(el.shadowRoot.textContent).toContain('Day heating');
});
