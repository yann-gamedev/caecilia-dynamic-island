'use strict';

// Offline only: evaluate the prototype's data/export sections, never its UI,
// browser scripts, CDN dependencies, or JSX.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');

const root = path.resolve(__dirname, '..');
const read = name => fs.readFileSync(path.join(root, name), 'utf8');
const normalize = text => text.replace(/\r\n/g, '\n').trimEnd();
const design = read('DESIGN.md');
const html = read('Caecilia Notch Island.html');
const header = read('src/spec/NotchIslandSpec.h');
const appendix = design.slice(design.indexOf('## Lampiran A'));
const match = appendix.match(/```cpp\r?\n([\s\S]*?)\r?\n```/);
assert.ok(match, 'DESIGN.md must contain Appendix A');
assert.equal(normalize(header), normalize(match[1]),
  'Spec header must match Appendix A verbatim');

function section(start, end) {
  const from = html.indexOf(start);
  const to = html.indexOf(end, from);
  assert.ok(from >= 0 && to > from, `Missing HTML section: ${start}`);
  return html.slice(from, to);
}

const source = [
  section('const STATES =', 'function simulate('),
  section('const R =', 'const LMAP ='),
  section('const pascal =', '/* ---------- small UI pieces ---------- */'),
  'cppExport();',
].join('\n');
const exported = vm.runInNewContext(source, Object.create(null), {
  timeout: 1000,
  contextCodeGeneration: { strings: false, wasm: false },
});

// The two documents use some different identifiers and comments. Compare
// declaration values in order, not identifiers; never rewrite the header.
function values(text, type) {
  const pattern = new RegExp(`constexpr ${type} \\w+[^=\\n]*=?[^\\n]*`, 'g');
  return [...text.matchAll(pattern)].map(([line]) => {
    const initializer = line.slice(line.indexOf('{'), line.indexOf(';'));
    return [...initializer.matchAll(/-?\d+(?:\.\d+)?f\b/g)]
      .map(([value]) => Number(value.slice(0, -1)));
  });
}
for (const type of ['IslandGeom', 'D2D1_ROUNDED_RECT', 'D2D1_RECT_F',
  'D2D1_COLOR_F', 'SpringParams']) {
  assert.deepEqual(values(header, type), values(exported, type),
    `HTML export differs from Appendix A: ${type}`);
}

const geometry = text => normalize(text.slice(
  text.indexOf('inline HRESULT BuildIslandGeometry('),
  text.indexOf('// Surface color for a sprung mix value')));
assert.equal(geometry(header), geometry(exported),
  'HTML geometry builder differs from Appendix A');
console.log('PASS: header matches DESIGN Appendix A verbatim.');
console.log('PASS: HTML export matches geometry, layout, colors, and spring values.');
console.log('NOTE: export does not define every visual constant needed for M0.');
