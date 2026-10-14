// Render the demo page in headless Chrome (system install, no download) to screenshots/*.png.
// Usage: npm run screenshots [-- --chrome /path/to/chrome] [--out dir]
import { createServer } from 'node:http';
import { mkdir, readFile } from 'node:fs/promises';
import { extname, join, normalize, resolve } from 'node:path';
import { chromium } from 'playwright-core';

const root = resolve(import.meta.dirname, '..');
const types = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.css': 'text/css' };
const server = createServer(async (req, res) => {
  const p = normalize(join(root, decodeURIComponent(new URL(req.url, 'http://x').pathname)));
  const file = req.url.split('?')[0].endsWith('/') ? join(p, 'index.html') : p;
  if (!file.startsWith(root)) return res.writeHead(403).end();
  try {
    const body = await readFile(file);
    res.writeHead(200, { 'content-type': types[extname(file)] ?? 'application/octet-stream' }).end(body);
  } catch {
    res.writeHead(404).end();
  }
}).listen(0, '127.0.0.1');
await new Promise((r) => server.once('listening', r));
const base = `http://127.0.0.1:${server.address().port}/demo/`;

const i = process.argv.indexOf('--chrome');
const executablePath = i > 0 ? process.argv[i + 1] : process.env.CHROME_PATH || '/usr/bin/google-chrome';
const o = process.argv.indexOf('--out');
const outDir = o > 0 ? resolve(process.argv[o + 1]) : join(root, 'screenshots');
await mkdir(outDir, { recursive: true });
const browser = await chromium.launch({ executablePath, args: ['--no-sandbox', '--font-render-hinting=none'] });
const shots = [
  ['light', 'heat-day'],
  ['dark', 'heat-day'],
  ['light', 'cool-night'],
  ['dark', 'cool-night'],
  ['light', 'morning-boost'],
  ['dark', 'morning-boost'],
  ['light', 'heat-boost'],
  ['dark', 'heat-boost'],
  ['light', 'tooltip'],
  ['dark', 'tooltip'],
];
try {
  for (const [theme, scenario] of shots) {
    const page = await browser.newPage({ viewport: { width: 412, height: 915 }, deviceScaleFactor: 2, colorScheme: theme });
    const errors = [];
    page.on('pageerror', (e) => errors.push(e.message));
    await page.goto(`${base}?theme=${theme}&scenario=${scenario}`);
    await page.waitForSelector('body[data-ready]');
    if (errors.length) throw new Error(`${theme}/${scenario}: ${errors.join('; ')}`);
    const out = join(outDir, `${scenario}-${theme}.png`);
    await page.screenshot({ path: out, fullPage: true });
    console.log(out);
    await page.close();
  }
  // Close-ups of the boost sections, light and dark, while a timed boost runs (cool, then heat).
  for (const [theme, scenario, tag] of [['light', 'cool-night', ''], ['dark', 'cool-night', ''], ['light', 'heat-boost', 'heat-'], ['dark', 'heat-boost', 'heat-']]) {
    const page = await browser.newPage({ viewport: { width: 412, height: 915 }, deviceScaleFactor: 2, colorScheme: theme });
    await page.goto(`${base}?theme=${theme}&scenario=${scenario}`);
    await page.waitForSelector('body[data-ready]');
    await page.locator('connie-thermostat-card').evaluate(async (c) => {
      c._open = { boost: true, morning: true, schedule: false, status: false };
      await c.updateComplete;
    });
    for (const [h, f] of tag ? [['Boost', 'boost']] : [['Boost', 'boost'], ['Morning heating boost', 'morning-boost']]) {
      const out = join(outDir, `closeup-${tag}${f}-${theme}.png`);
      await page.locator('section').filter({ has: page.locator('h3', { hasText: new RegExp(`^${h}$`) }) }).screenshot({ path: out });
      console.log(out);
    }
    await page.close();
  }

  // Geometry check: every tooltip, opened in turn, stays inside the card and covers none of
  // the controls in its own row (or, for the setpoint, the setpoint box). Phone and narrow widths.
  for (const width of [412, 320]) {
    for (const scenario of ['heat-day', 'morning-boost', 'cool-night', 'heat-boost']) {
      const page = await browser.newPage({ viewport: { width, height: 915 } });
      await page.goto(`${base}?theme=light&scenario=${scenario}`);
      await page.waitForSelector('body[data-ready]');
      const problems = await page.evaluate(async () => {
        const card = document.querySelector('connie-thermostat-card');
        card._open = { boost: true, morning: true, schedule: true, status: true };
        await card.updateComplete;
        const root = card.shadowRoot;
        const box = root.querySelector('ha-card').getBoundingClientRect();
        const hit = (a, b) => a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
        const out = [];
        const ids = [...root.querySelectorAll('.info-b')].map((w) => w.dataset.tip);
        for (const id of ids) {
          card._tip = { id, pinned: true };
          await card.updateComplete;
          const w = root.querySelector(`.info-b[data-tip="${id}"]`);
          const t = root.getElementById(`tip-${id}`).getBoundingClientRect();
          if (t.left < box.left || t.right > box.right || t.top < box.top) out.push(`${id}: outside the card`);
          // Nothing else in the card that's visible text or a control may sit under an overlay tooltip.
          if (!root.getElementById(`tip-${id}`).classList.contains('inline'))
            for (const c of root.querySelectorAll('.sub, .room, .period')) if (hit(t, c.getBoundingClientRect())) out.push(`${id}: covers ${c.className}`);
          const own = w.closest('.tile') ?? w.closest('.row') ?? w.closest('.setpoint') ?? w.closest('.legend');
          for (const c of own.querySelectorAll('button, input, select, [role="slider"]')) {
            if (w.contains(c)) continue;
            if (hit(t, c.getBoundingClientRect())) out.push(`${id}: covers ${c.getAttribute('aria-label') || c.className}`);
          }
        }
        return out;
      });
      await page.close();
      if (problems.length) throw new Error(`tooltip overlap at ${width}px (${scenario}): ${problems.join('; ')}`);
      console.log(`tooltips OK at ${width}px (${scenario})`);
    }
  }
} finally {
  await browser.close();
  server.close();
}
