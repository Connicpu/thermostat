import { css } from 'lit';

export const cardStyles = css`
  :host {
    --ct-heat: var(--state-climate-heat-color, #ff8100);
    --ct-cool: var(--state-climate-cool-color, #2b9af9);
    --ct-fan: var(--state-climate-fan_only-color, #00bcd4);
    --ct-off: var(--state-climate-off-color, var(--disabled-color, #8a8a8a));
    --ct-accent: var(--ct-off);
    --ct-radius: var(--ha-card-border-radius, 12px);
    --ct-tile: var(--secondary-background-color, rgba(127, 127, 127, 0.08));
    --ct-divider: var(--divider-color, rgba(127, 127, 127, 0.2));
    display: block;
  }
  :host([mode='heat']) { --ct-accent: var(--ct-heat); }
  :host([mode='cool']) { --ct-accent: var(--ct-cool); }
  ha-card { overflow: hidden; }
  svg.icon { width: 24px; height: 24px; fill: currentColor; flex: none; }
  svg.sm { width: 18px; height: 18px; }
  .content { padding: 16px; display: flex; flex-direction: column; gap: 16px; }
  .banner {
    display: flex; align-items: center; gap: 8px; padding: 8px 12px; border-radius: 8px;
    background: rgba(var(--rgb-warning-color, 255, 166, 0), 0.15); color: var(--primary-text-color);
    font-size: 14px;
  }
  .banner svg { color: var(--warning-color, #ffa600); }
  .banner.error { background: rgba(var(--rgb-error-color, 219, 68, 55), 0.15); }
  .banner.error svg { color: var(--error-color, #db4437); }

  /* header */
  .head { display: flex; align-items: flex-start; justify-content: space-between; gap: 12px; }
  .title { font-size: 16px; font-weight: 500; color: var(--primary-text-color); }
  .sub { font-size: 13px; color: var(--secondary-text-color); margin-top: 2px; }
  .room { display: flex; align-items: baseline; gap: 12px; margin-top: 4px; }
  .room .t { font-size: 44px; font-weight: 400; line-height: 1.1; color: var(--primary-text-color); }
  .room .t small { font-size: 20px; color: var(--secondary-text-color); }
  .room .h { display: inline-flex; align-items: center; gap: 2px; color: var(--secondary-text-color); font-size: 15px; }
  .chips { display: flex; flex-wrap: wrap; gap: 6px; justify-content: flex-end; }
  .chip {
    display: inline-flex; align-items: center; gap: 4px; height: 28px; padding: 0 10px 0 8px;
    border-radius: 14px; font-size: 13px; font-weight: 500; color: var(--secondary-text-color);
    background: var(--ct-tile);
  }
  .chip.on { color: var(--c); background: color-mix(in srgb, var(--c) 18%, transparent); }
  .chip.heat { --c: var(--ct-heat); }
  .chip.cool { --c: var(--ct-cool); }
  .chip.fan { --c: var(--ct-fan); }
  .chip.on.fan svg { animation: spin 1.6s linear infinite; }
  @keyframes spin { to { transform: rotate(360deg); } }
  @media (prefers-reduced-motion: reduce) { .chip.on.fan svg { animation: none; } }

  /* setpoint */
  .setpoint {
    border-radius: var(--ct-radius); padding: 14px 12px 12px;
    background: color-mix(in srgb, var(--ct-accent) 12%, transparent);
    display: flex; flex-direction: column; align-items: center; gap: 6px;
  }
  .period {
    display: inline-flex; align-items: center; gap: 6px; font-size: 13px; font-weight: 500;
    padding: 4px 10px; border-radius: 14px; color: var(--ct-accent);
    background: var(--card-background-color, var(--ha-card-background, #fff));
  }
  .sp-row { display: flex; align-items: center; justify-content: center; gap: 20px; width: 100%; }
  .sp-val { font-size: 52px; line-height: 1; color: var(--primary-text-color); min-width: 3.4ch; text-align: center; font-variant-numeric: tabular-nums; }
  .sp-val small { font-size: 22px; color: var(--secondary-text-color); }
  .sp-val.pending { opacity: 0.6; }
  .caption { font-size: 13px; color: var(--secondary-text-color); text-align: center; }
  .round {
    width: 48px; height: 48px; border-radius: 50%; border: none; cursor: pointer; display: grid; place-items: center;
    background: var(--card-background-color, var(--ha-card-background, #fff)); color: var(--ct-accent);
    box-shadow: 0 1px 2px rgba(0, 0, 0, 0.15);
  }
  .round:disabled { opacity: 0.4; cursor: default; }
  .round:focus-visible, .seg button:focus-visible, .mini:focus-visible, .btn:focus-visible { outline: 2px solid var(--primary-color); outline-offset: 2px; }
  .off-note { font-size: 14px; color: var(--secondary-text-color); padding: 18px 0; }

  /* modes */
  .controls { display: flex; gap: 8px; align-items: stretch; }
  .seg { flex: 1; display: grid; grid-template-columns: repeat(3, 1fr); gap: 4px; padding: 4px; border-radius: var(--ct-radius); background: var(--ct-tile); }
  .seg button, .fanbtn {
    border: none; background: transparent; cursor: pointer; border-radius: 8px; height: 44px; font: inherit;
    font-size: 13px; font-weight: 500; color: var(--secondary-text-color); display: flex; align-items: center; justify-content: center; gap: 6px;
  }
  .seg button[aria-pressed='true'] { color: var(--c, var(--primary-text-color)); background: var(--card-background-color, var(--ha-card-background, #fff)); box-shadow: 0 1px 2px rgba(0,0,0,0.12); }
  .seg .heat { --c: var(--ct-heat); }
  .seg .cool { --c: var(--ct-cool); }
  .seg .off { --c: var(--primary-text-color); }
  .fanbtn { padding: 0 14px; border-radius: var(--ct-radius); background: var(--ct-tile); height: auto; }
  .fanbtn[aria-pressed='true'] { color: var(--ct-fan); background: color-mix(in srgb, var(--ct-fan) 18%, transparent); }

  /* sections */
  section { border-top: 1px solid var(--ct-divider); padding-top: 12px; }
  .sec-h { display: flex; align-items: center; justify-content: space-between; cursor: pointer; user-select: none; background: none; border: none; width: 100%; padding: 0; color: inherit; font: inherit; }
  .sec-h h3 { margin: 0; font-size: 15px; font-weight: 500; color: var(--primary-text-color); }
  .sec-h .meta { font-size: 13px; color: var(--secondary-text-color); display: flex; align-items: center; gap: 4px; }
  .sec-h svg.chev { transition: transform 0.2s; color: var(--secondary-text-color); }
  section.closed .sec-h svg.chev { transform: rotate(-90deg); }
  .sec-b { margin-top: 12px; display: flex; flex-direction: column; gap: 12px; }
  section.closed .sec-b { display: none; }

  /* timeline */
  .tl { display: grid; grid-template-columns: 40px 1fr; gap: 6px 8px; align-items: center; font-size: 12px; color: var(--secondary-text-color); }
  .bar { position: relative; height: 14px; border-radius: 7px; overflow: hidden; background: color-mix(in srgb, var(--c) 22%, transparent); }
  .bar.heat { --c: var(--ct-heat); }
  .bar.cool { --c: var(--ct-cool); }
  .bar .day { position: absolute; top: 0; bottom: 0; background: color-mix(in srgb, var(--c) 70%, transparent); }
  .bar .mb { position: absolute; top: 0; bottom: 0; background: repeating-linear-gradient(45deg, var(--c), var(--c) 3px, transparent 3px, transparent 6px); }
  .bar .now { position: absolute; top: -2px; bottom: -2px; width: 2px; background: var(--primary-text-color); }
  .ticks { grid-column: 2; display: flex; justify-content: space-between; font-size: 11px; }

  /* schedule grid */
  .sched { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; }
  .slot { border-radius: var(--ct-radius); padding: 10px; background: var(--ct-tile); display: flex; flex-direction: column; gap: 8px; border: 2px solid transparent; min-width: 0; }
  .slot.heat { --c: var(--ct-heat); }
  .slot.cool { --c: var(--ct-cool); }
  .slot.active { border-color: var(--c); background: color-mix(in srgb, var(--c) 10%, transparent); }
  .slot-h { display: flex; align-items: center; gap: 6px; font-size: 13px; font-weight: 500; color: var(--primary-text-color); }
  .slot-h svg { color: var(--c); }
  .slot-h .now-tag { margin-left: auto; font-size: 11px; font-weight: 600; color: var(--c); text-transform: uppercase; letter-spacing: 0.04em; }
  .field { display: flex; align-items: center; justify-content: space-between; gap: 6px; font-size: 13px; color: var(--secondary-text-color); }
  input[type='time'] {
    font: inherit; font-size: 14px; color: var(--primary-text-color); background: var(--card-background-color, var(--ha-card-background, #fff));
    border: 1px solid var(--ct-divider); border-radius: 6px; padding: 4px 4px; min-width: 0; width: 100%; max-width: 112px; color-scheme: light dark;
  }
  .stepper { display: inline-flex; align-items: center; gap: 2px; }
  .stepper .v { min-width: 5.2ch; white-space: nowrap; text-align: center; font-size: 14px; color: var(--primary-text-color); font-variant-numeric: tabular-nums; }
  .stepper .v.pending { opacity: 0.6; }
  .mini { width: 30px; height: 30px; border-radius: 50%; border: none; background: var(--card-background-color, var(--ha-card-background, #fff)); color: var(--primary-text-color); display: grid; place-items: center; cursor: pointer; }
  .mini:disabled { opacity: 0.35; cursor: default; }
  .mini svg { width: 18px; height: 18px; }
  .rows .mini, .rows select { background: var(--ct-tile); border-color: transparent; }
  .rows { display: flex; flex-direction: column; gap: 10px; }
  .row { display: flex; align-items: center; justify-content: space-between; gap: 8px; font-size: 14px; color: var(--primary-text-color); min-height: 36px; }
  .row .l { display: flex; flex-direction: column; }
  .row .l span:last-child:not(:first-child) { font-size: 12px; color: var(--secondary-text-color); }
  .row .r { color: var(--secondary-text-color); }
  .row .r.bad { color: var(--error-color, #db4437); font-weight: 500; }
  select { font: inherit; font-size: 14px; color: var(--primary-text-color); background: var(--card-background-color, var(--ha-card-background, #fff)); border: 1px solid var(--ct-divider); border-radius: 6px; padding: 4px 6px; }
  .btns { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
  .btn { border: none; border-radius: var(--ct-radius); background: var(--ct-tile); color: var(--primary-text-color); font: inherit; font-size: 13px; font-weight: 500; padding: 10px 6px; display: flex; flex-direction: column; align-items: center; gap: 4px; cursor: pointer; }
  .btn svg { color: var(--primary-color); }
  .btn:disabled { opacity: 0.4; cursor: default; }
  .boost-live { display: flex; align-items: center; gap: 8px; padding: 8px 12px; border-radius: 8px; font-size: 14px; color: var(--primary-text-color); background: color-mix(in srgb, var(--primary-color) 14%, transparent); }
  .boost-live svg { color: var(--primary-color); }
  .missing { font-size: 13px; color: var(--secondary-text-color); font-style: italic; }

  @container (min-width: 620px) {
    .wide { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; align-items: start; }
  }
  .wrap { container-type: inline-size; }
`;
