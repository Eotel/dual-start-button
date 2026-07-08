import assert from 'node:assert/strict';
import { test } from 'node:test';

import { INITIAL_ACTIVE, reduceActive, resetActive } from './active-state.js';

// Feed a sequence of observed pressed samples and return the final tracker.
function observe(samples, tracker = INITIAL_ACTIVE) {
  return samples.reduce(reduceActive, tracker);
}

test('initial state is not active', () => {
  assert.equal(INITIAL_ACTIVE.active, false);
});

test('press down-edge after a released baseline toggles active on', () => {
  // Also covers a lost down-notification: a heartbeat reporting pressed=true
  // while the last sample was released is observed as the same down-edge.
  assert.equal(observe([false, true]).active, true);
});

test('release does not change active', () => {
  assert.equal(observe([false, true, false]).active, true);
});

test('pressing again toggles active off', () => {
  assert.equal(observe([false, true, false, true]).active, false);
});

test('a third press toggles active on again', () => {
  assert.equal(observe([false, true, false, true, false, true]).active, true);
});

test('first observed sample is a baseline: pressed at connect does not toggle', () => {
  // Reset (disconnect/unlink/relink/manual) reassigns INITIAL_ACTIVE, so this
  // is also the spec for the first sample observed after any reset.
  assert.equal(observe([true]).active, false);
});

test('baseline pressed=true, then release and press toggles on', () => {
  assert.equal(observe([true, false, true]).active, true);
});

test('repeated pressed samples (heartbeat while held) do not toggle again', () => {
  assert.equal(observe([false, true, true, true]).active, true);
});

test('repeated released samples (idle heartbeat) do not toggle', () => {
  assert.equal(observe([false, false, false]).active, false);
});

test('resetActive() with no known state equals the initial tracker', () => {
  assert.deepEqual(resetActive(), INITIAL_ACTIVE);
});

test('reset seeded with released state: the very next press toggles', () => {
  // Codex review: a press right after Reset Active / relink must not be
  // swallowed as a baseline when the slot state is already known.
  assert.equal(reduceActive(resetActive(false), true).active, true);
});

test('reset seeded with held state: no toggle until release and press', () => {
  const held = resetActive(true);
  assert.equal(reduceActive(held, true).active, false);
  assert.equal(observe([false, true], held).active, true);
});

test('reset always clears active regardless of seed', () => {
  assert.equal(resetActive(false).active, false);
  assert.equal(resetActive(true).active, false);
});

test('reducer returns a new tracker and does not mutate its input', () => {
  const before = { ...INITIAL_ACTIVE };
  const input = { ...INITIAL_ACTIVE };
  const out = reduceActive(input, true);
  assert.notEqual(out, input);
  assert.deepEqual(input, before);
});
