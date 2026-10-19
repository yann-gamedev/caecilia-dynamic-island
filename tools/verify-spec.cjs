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
// Only approved M0 and v0.2 additions are excluded from the original HTML comparison.
// They remain covered by Appendix A equality and the v0.2/Appendix B checks below.
const originalDeclarations = header
  .replace(/^\s*constexpr D2D1_RECT_F (Body|LeftEar|RightEar|FpsRect) = .*;.*$/gm, '')
  .replace(/^\s*constexpr D2D1_ROUNDED_RECT Dot[0-3] = .*;.*$/gm, '')
  .replace(/^\s*constexpr SpringParams HoverSwell .*$/gm, '')
  .replace(/^\s*constexpr D2D1_COLOR_F (Transparent|ActiveColor|IdleColor|ArtworkPlaceholder) = .*;.*$/gm, '');
for (const type of ['IslandGeom', 'D2D1_ROUNDED_RECT', 'D2D1_RECT_F',
  'D2D1_COLOR_F', 'SpringParams']) {
  assert.deepEqual(values(originalDeclarations, type), values(exported, type),
    `HTML export differs from Appendix A: ${type}`);
}

const geometry = text => normalize(text.slice(
  text.indexOf('inline HRESULT BuildIslandGeometry('),
  text.indexOf('// Surface color for a sprung mix value')));
assert.equal(geometry(header), geometry(exported),
  'HTML geometry builder differs from Appendix A');

// v0.2 additions must match the DESIGN tables (SPEC-PATCH v0.2 sections C, E, F).
const num = (pattern, label) => {
  const found = header.match(pattern);
  assert.ok(found, `Missing header token: ${label}`);
  return Number(found[1]);
};
const dots = values(header, 'D2D1_ROUNDED_RECT').filter(v => v[1] === 15.5);
assert.deepEqual(dots, [209, 215, 221, 227].map(x => [x, 15.5, x + 3, 18.5, 1.5, 1.5]),
  'Paused dots must be 3 x 3 DIP, centered on y = 17, sharing bar x');
for (const [x, y, r, b] of dots) {
  assert.ok(design.includes(`\`{${x}, ${y}, ${r}, ${b}}\``), `DESIGN missing paused dot {${x}, ${y}, ${r}, ${b}}`);
}
const swell = ['Width', 'Height', 'BottomRadius'].map(n =>
  num(new RegExp(`kSwellDelta${n}\\s*=\\s*([\\d.]+)f`), `kSwellDelta${n}`));
assert.deepEqual(swell, [8, 2, 1], 'Hover Swell deltas');
assert.match(design, /\| Width \| \+8 \| 168 \| 248 \|/, 'DESIGN swell width row');
assert.match(design, /\| Height \| \+2 \| 30 \| 36 \|/, 'DESIGN swell height row');
assert.match(design, /\| Bottom radius \| \+1 \| 13 \| 18 \|/, 'DESIGN swell radius row');
assert.equal(num(/kPausedArtOpacity\s*=\s*([\d.]+)f/, 'kPausedArtOpacity'), 0.5);
assert.equal(num(/kPausedDotHeight\s*=\s*([\d.]+)f/, 'kPausedDotHeight'), 3);
const timing = {
  HoverDelayMsDefault: 150, CollapseDelayMs: 250, HudVisibleMs: 1800, PauseGraceMs: 30000,
  PausedMorphMs: 200, ContentFadeInMs: 240, ContentFadeDelayMs: 70, ContentFadeOutMs: 120,
  TelemetryPollMs: 1000,
};
for (const [name, ms] of Object.entries(timing)) {
  assert.equal(num(new RegExp(`unsigned ${name} = (\\d+);`), `Timing::${name}`), ms, `Timing::${name}`);
  assert.match(design, new RegExp(`\\| \`${name}\` \\| ${ms} \\|`), `DESIGN 8.2 row ${name}`);
}
// Spring rows in DESIGN 8.1 must equal the header (catches HoverSwell, which HTML lacks).
for (const [, name, k, c] of header.matchAll(/SpringParams (\w+) \{ ([\d.]+)f, ([\d.]+)f, 1\.0f \}/g)) {
  assert.match(design, new RegExp(`\\| \`${name}\` \\| ${k} \\| ${c} \\| 1\\.0 \\|`), `DESIGN 8.1 row ${name}`);
}
assert.match(header, /kDefaultConfig \{ 1, TitleFont::Serif, Color::Sky, 150u, 0 \}/,
  'kDefaultConfig must match DESIGN section 10 defaults');

// Appendix B: restored M0/M1 tokens must exist, with Motion timing aliased to Timing.
for (const ns of ['WindowSpec', 'PaintSpec', 'Typography', 'DebugHud', 'Motion', 'Glyph', 'Demo']) {
  assert.match(header, new RegExp(`^namespace ${ns} \\{`, 'm'), `Missing restored namespace ${ns}`);
}
for (const token of ['Width = 492.0f', 'Height = 193.0f', 'DpiBase = 96.0f', 'RimStroke = 2.0f',
  'DotOpacityIdle = 0.35f', 'D2D1_COLOR_F Transparent', 'D2D1_RECT_F Body', 'FpsRect = { 8.0f, 6.0f, 64.0f, 17.0f }',
  'ShowDebugFpsDefault = false', 'ShowBoundsDefault = false', 'TabStride = Tab1.left - Tab0.left',
  'ReducedFadeMs = 100', 'ArtworkPlaceholder = Color::Label']) {
  assert.ok(header.includes(token), `Missing restored token: ${token}`);
}
const aliases = { HoverEnterMs: 'HoverDelayMsDefault', HoverLeaveMs: 'CollapseDelayMs', HudMs: 'HudVisibleMs',
  FadeInMs: 'ContentFadeInMs', FadeDelayMs: 'ContentFadeDelayMs', FadeOutMs: 'ContentFadeOutMs' };
for (const [old, now] of Object.entries(aliases)) {
  assert.match(header, new RegExp(`unsigned ${old} = Timing::${now};`), `Motion::${old} must alias Timing::${now}`);
}
assert.doesNotMatch(header, /Hover\w* = 120\b/, 'Old 120 ms hover delay must not return');
assert.doesNotMatch(design.slice(0, design.indexOf('## Lampiran A')), /hover[^|\n]*120 ms/i,
  'DESIGN must not reintroduce the 120 ms hover delay');
const minScale = num(/WaveMinimumScale = ([\d.]+)f/, 'WaveMinimumScale');
assert.equal(Math.round(16 * minScale * 100) / 100, 4.48, 'Wave minimum must stay 16 x 0.28 = 4.48 DIP');
assert.equal(num(/ReducedMotionWaveHeight = ([\d.]+)f/, 'ReducedMotionWaveHeight'), 10);
assert.match(design, /tinggi 10 DIP, `Motion::ReducedMotionWaveHeight`/, 'DESIGN 8.3 reduced-motion wave');
const prose = design.slice(0, design.indexOf('## Lampiran A'));
assert.doesNotMatch(prose, /h 4–16|height 4–16|\[4, 16\]|4\.5 ↔/, 'Wave minimum prose must say 4.48');
assert.ok(design.includes('## Lampiran B'), 'DESIGN must document restored tokens in Appendix B');

console.log('PASS: header matches DESIGN Appendix A verbatim.');
console.log('PASS: HTML export matches geometry, layout, colors, and spring values.');
console.log('PASS: v0.2 dots, swell, paused, Timing, springs, and config defaults match DESIGN.');
console.log('PASS: restored M0/M1 tokens (Appendix B) present; Motion aliases Timing; wave 4.48 / reduced 10 DIP.');
