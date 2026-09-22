import { test } from 'node:test';
import assert from 'node:assert/strict';
import { DEFAULTS, settings, geometry, percentile, nativeRequest, BACKENDS } from '../../dist/compiled/packages/contracts/index.js';
test('settings defaults are immutable and patches do not mutate them', () => {
  assert.equal(Object.isFrozen(DEFAULTS), true);
  assert.deepEqual(settings({}), DEFAULTS);
  assert.equal(settings({ strength: 0.75 }).strength, 0.75);
  assert.equal(DEFAULTS.strength, 0.4);
});
for (const bad of [null, [], 'x', { enabled: 1 }, { strength: NaN }, { denoise: Infinity }, { strength: -1 },
  { strength: 1.1 }, { denoise: 0.6 }, { split: 2 }, { scale: 0 }, { scale: 3 }, { modelPath: '/secret' }]) {
  test(`reject unsafe settings ${JSON.stringify(bad)}`, () => assert.throws(() => settings(bad)));
}
test('geometry preserves aspect and caps allocation at 4K/8.3 MP', () => {
  assert.deepEqual(geometry(1280, 720, 1.5), { width: 1920, height: 1080 });
  const result = geometry(4096, 4096, 2);
  assert(result.width * result.height <= 8_294_400);
  assert.equal(result.width, result.height);
  assert.deepEqual(geometry(1, 1), { width: 1, height: 1 });
  assert.equal(geometry(8000, 4000, 2, 2048).width, 2048);
});
for (const args of [[0, 1], [1, -1], [Infinity, 720], [NaN, 2], [1, 1, 3]]) {
  test(`reject unsafe dimensions ${args}`, () => assert.throws(() => geometry(...args)));
}
test('percentiles use nearest-rank without mutating samples', () => {
  const samples = [8, 1, 9, 3]; assert.equal(percentile(samples, .5), 3); assert.equal(percentile(samples, .95), 9);
  assert.equal(percentile([], .95), 0); assert.deepEqual(samples, [8, 1, 9, 3]);
});
test('native envelope supports only explicit allowlisted commands and sessions', () => {
  assert.equal(nativeRequest({ v: 1, id: 'a', command: 'hello' }).command, 'hello');
  assert.equal(nativeRequest({ v: 1, id: 'a', command: 'startSession', session: 'owned-session' }).session, 'owned-session');
  assert.equal(nativeRequest({ v: 1, id: 'a', command: 'updateSettings', session: 's', settings: { strength: 0.5 } }).settings.strength, 0.5);
});
const invalidNative = [
  { v: 2, id: 'a', command: 'hello' }, { v: 1, id: '../escape', command: 'hello' },
  { v: 1, id: 'a', command: 'exec' }, { v: 1, id: 'a', command: 'startSession' },
  { v: 1, id: 'a', command: 'hello', path: '/private' },
  { v: 1, id: 'a', command: 'hello', settings: {} },
  { v: 1, id: 'a', command: 'updateSettings', session: 's' },
  { v: 1, id: 'a', command: 'updateSettings', session: 's', settings: { scale: 2 } },
  { v: 1, id: 'a', command: 'updateSettings', session: 's', settings: { strength: 9 } },
  { v: 1, id: 'a', command: 'hello', session: {} },
];
for (const envelope of invalidNative) test(`reject native ${JSON.stringify(envelope)}`, () => assert.throws(() => nativeRequest(envelope)));
test('unverified neural backends are explicitly unavailable', () => {
  assert.equal(BACKENDS.find(b => b.id === 'nr-experimental').available, false);
  assert.equal(BACKENDS.find(b => b.id === 'animation-neural').available, false);
});
