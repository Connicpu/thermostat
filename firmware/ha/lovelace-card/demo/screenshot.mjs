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
  ['light', 'tooltip'],
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
} finally {
  await browser.close();
  server.close();
}
