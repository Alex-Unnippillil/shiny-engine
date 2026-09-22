import { test } from 'node:test';
import assert from 'node:assert/strict';
import { DEFAULTS, PRESETS } from '../../dist/compiled/packages/contracts/index.js';
import { presetName, readPreferences, writePreferences, formatTime, exportName, SETTINGS_KEY } from '../../dist/compiled/apps/viewer/preferences.js';
import { fileType, validDimensions } from '../../dist/compiled/apps/viewer/media.js';

test('presets report custom adjustments and all three exact presets', () => {
  for (const [name, preset] of Object.entries(PRESETS)) assert.equal(presetName({ ...DEFAULTS, ...preset }), name);
  assert.equal(presetName({ ...DEFAULTS, strength: 0.41 }), 'custom');
});
test('preferences round-trip every control and preserve bypass', () => {
  let stored;
  const storage = { setItem: (key, value) => { assert.equal(key, SETTINGS_KEY); stored = value; }, getItem: () => stored };
  const config = { ...DEFAULTS, ...PRESETS.detail, enabled: false, split: 0.73, scale: 2 };
  assert(writePreferences(storage, config)); assert.deepEqual(readPreferences(storage), config);
});
for (const invalid of ['{', 'null', '[]', '{"strength":99}', '{"modelPath":"x"}']) {
  test(`invalid stored settings safely reset: ${invalid}`, () => assert.deepEqual(readPreferences({ getItem: () => invalid }), DEFAULTS));
}
test('blocked storage remains nonfatal and unsupported fractional scale is normalized', () => {
  assert.deepEqual(readPreferences({ getItem: () => { throw Error('blocked'); } }), DEFAULTS);
  assert.equal(writePreferences({ setItem: () => { throw Error('full'); } }, DEFAULTS), false);
  assert.equal(readPreferences({ getItem: () => '{"scale":1.37}' }).scale, 1);
});
test('MIME-less OS files are recognized without permitting explicit unsafe formats', () => {
  assert.equal(fileType({ type: '', name: 'Photo.JPG', size: 20 }), 'image/jpeg');
  assert.equal(fileType({ type: '', name: 'recording.ogv', size: 20 }), 'video/ogg');
  assert.throws(() => fileType({ type: 'image/svg+xml', name: 'photo.png', size: 20 }));
  assert.throws(() => fileType({ type: '', name: 'photo.exe', size: 20 }));
});
test('empty files, oversized input and unsafe dimensions are rejected', () => {
  assert.throws(() => fileType({ type: 'image/png', name: 'x', size: 0 }));
  assert.throws(() => fileType({ type: 'image/png', name: 'x', size: 50 * 1024 ** 2 + 1 }));
  assert.throws(() => validDimensions(Infinity, 10));
  assert.throws(() => validDimensions(6000, 6000));
  validDimensions(4096, 2160);
});
test('playback times handle unknown duration and hour-long files', () => {
  assert.equal(formatTime(Infinity), '0:00'); assert.equal(formatTime(-3), '0:00');
  assert.equal(formatTime(65.8), '1:05'); assert.equal(formatTime(3661), '1:01:01');
});
test('PNG export names cannot contain paths or reserved filename punctuation', () => {
  assert.equal(exportName('my photo.jpg', 'enhanced'), 'my-photo-enhanced.png');
  assert(!/[\\/:*?"<>|]/.test(exportName('../<>:x.png', 'original')));
  assert.equal(exportName('🎨.png', 'comparison'), 'shiny-engine-comparison.png');
});
