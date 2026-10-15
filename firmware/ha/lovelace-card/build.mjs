import * as esbuild from 'esbuild';
import { readFileSync, writeFileSync } from 'fs';
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

/* Full license text for the bundled Lit packages and @mdi/js icon paths, shipped beside the HACS asset. */
const kLitPkgs = ['lit', 'lit-element', 'lit-html', '@lit/reactive-element'];
writeFileSync(
  'dist/THIRD_PARTY_LICENSES.txt',
  kLitPkgs
    .map((p) => `===== ${p} (bundled in connie-thermostat-card.js) =====\n\n` +
      readFileSync(`node_modules/${p}/LICENSE`, 'utf8').trim() + '\n')
    .join('\n') +
    '\n===== @mdi/js =====\n\n' +
    readFileSync('node_modules/@mdi/js/LICENSE', 'utf8').trim() +
    '\n',
);
