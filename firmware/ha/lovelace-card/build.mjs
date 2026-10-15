import * as esbuild from 'esbuild';
import { readFileSync } from 'fs';
await esbuild.build({
  entryPoints: ['src/connie-thermostat-card.js'],
  bundle: true,
  format: 'esm',
  minify: true,
  legalComments: 'eof',
  target: 'es2021',
  banner: { js: readFileSync('mdi-banner.txt', 'utf8').trim() },
  outfile: 'dist/connie-thermostat-card.js',
});
