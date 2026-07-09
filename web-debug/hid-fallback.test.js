import assert from 'node:assert/strict';
import { test } from 'node:test';

import {
  applyHidKeyDown,
  applyHidKeyUp,
  clearHidSlot,
  createHidFallbackState,
  evaluateHidStartCondition,
  expireHidReleases,
  HID_RELEASE_TIMEOUT_MS,
  parseHidAssignments,
  resetHidActive,
  serializeHidAssignments,
  setHidFocus,
  startHidCapture,
} from './hid-fallback.js';

function keyEvent(code, overrides = {}) {
  return {
    code,
    key: overrides.key ?? code,
    repeat: false,
    altKey: false,
    ctrlKey: false,
    metaKey: false,
    shiftKey: false,
    ...overrides,
  };
}

test('captures the next HID key identity into a local slot binding', () => {
  const initial = startHidCapture(createHidFallbackState(), 1);
  const state = applyHidKeyDown(initial, keyEvent('F13'), 100);

  assert.equal(state.captureSlot, null);
  assert.deepEqual(state.slots[1].assignment, { id: 'code:F13', label: 'F13' });
  assert.equal(state.slots[1].active.active, false);
  assert.equal(state.slots[1].pressed, true);
  assert.deepEqual(evaluateHidStartCondition(state), {
    ok: false,
    reason: 'HID slot 2 is not assigned',
  });
});

test('persisted HID assignments round-trip without pressed or active state', () => {
  let state = createHidFallbackState();
  state = startHidCapture(state, 1);
  state = applyHidKeyDown(state, keyEvent('F13'), 100);
  state = applyHidKeyUp(state, keyEvent('F13'), 120);
  state = applyHidKeyDown(state, keyEvent('F13'), 200);

  const restored = createHidFallbackState(parseHidAssignments(serializeHidAssignments(state)));

  assert.deepEqual(restored.slots[1].assignment, { id: 'code:F13', label: 'F13' });
  assert.equal(restored.slots[1].active.active, false);
  assert.equal(restored.slots[1].pressed, false);
});

test('duplicate HID key identity is rejected for the second slot', () => {
  let state = createHidFallbackState();
  state = startHidCapture(state, 1);
  state = applyHidKeyDown(state, keyEvent('F13'), 100);
  state = applyHidKeyUp(state, keyEvent('F13'), 120);
  state = startHidCapture(state, 2);
  state = applyHidKeyDown(state, keyEvent('F13'), 200);

  assert.equal(state.captureSlot, 2);
  assert.equal(state.slots[2].assignment, null);
  assert.equal(state.error, 'F13 is already assigned to HID slot 1');
});

test('clearing and reassigning a HID slot resets only that slot', () => {
  let state = createHidFallbackState();
  state = startHidCapture(state, 1);
  state = applyHidKeyDown(state, keyEvent('F13'), 100);
  state = startHidCapture(applyHidKeyUp(state, keyEvent('F13'), 120), 2);
  state = applyHidKeyDown(state, keyEvent('F14'), 200);

  state = clearHidSlot(state, 1);
  state = startHidCapture(state, 1);
  state = applyHidKeyDown(state, keyEvent('F15'), 300);

  assert.deepEqual(state.slots[1].assignment, { id: 'code:F15', label: 'F15' });
  assert.deepEqual(state.slots[2].assignment, { id: 'code:F14', label: 'F14' });
  assert.equal(state.slots[1].active.active, false);
});

test('assigned HID down-edge toggles active and repeat keydown does not toggle again', () => {
  let state = createHidFallbackState({
    slot1: { id: 'code:F13', label: 'F13' },
    slot2: { id: 'code:F14', label: 'F14' },
  });

  state = applyHidKeyDown(state, keyEvent('F13'), 100);
  assert.equal(state.slots[1].active.active, true);
  assert.equal(state.slots[1].pressed, true);

  state = applyHidKeyDown(state, keyEvent('F13', { repeat: true }), 110);
  state = applyHidKeyDown(state, keyEvent('F13'), 120);
  assert.equal(state.slots[1].active.active, true);
  assert.equal(state.slots[1].pressed, true);
});

test('keyup clears transient pressed state without changing active', () => {
  let state = createHidFallbackState({ slot1: { id: 'code:F13', label: 'F13' } });
  state = applyHidKeyDown(state, keyEvent('F13'), 100);
  state = applyHidKeyUp(state, keyEvent('F13'), 140);

  assert.equal(state.slots[1].pressed, false);
  assert.equal(state.slots[1].active.active, true);
});

test('release timeout recovers from a missed keyup without changing active', () => {
  let state = createHidFallbackState({ slot1: { id: 'code:F13', label: 'F13' } });
  state = applyHidKeyDown(state, keyEvent('F13'), 100);
  state = expireHidReleases(state, 100 + HID_RELEASE_TIMEOUT_MS + 1);

  assert.equal(state.slots[1].pressed, false);
  assert.equal(state.slots[1].active.active, true);

  state = applyHidKeyDown(state, keyEvent('F13'), 100 + HID_RELEASE_TIMEOUT_MS + 20);
  assert.equal(state.slots[1].active.active, false);
});

test('HID start condition requires both assigned slots active and focused page state', () => {
  let state = createHidFallbackState({
    slot1: { id: 'code:F13', label: 'F13' },
    slot2: { id: 'code:F14', label: 'F14' },
  });
  state = applyHidKeyDown(state, keyEvent('F13'), 100);
  state = applyHidKeyUp(state, keyEvent('F13'), 120);
  state = applyHidKeyDown(state, keyEvent('F14'), 200);

  assert.deepEqual(evaluateHidStartCondition(state), {
    ok: true,
    reason: 'both HID fallback slots active',
  });

  state = setHidFocus(state, false);
  assert.deepEqual(evaluateHidStartCondition(state), {
    ok: false,
    reason: 'HID fallback page is not focused',
  });
});

test('resetHidActive clears active state and seeds baselines from current pressed values', () => {
  let state = createHidFallbackState({
    slot1: { id: 'code:F13', label: 'F13' },
    slot2: { id: 'code:F14', label: 'F14' },
  });
  state = applyHidKeyDown(state, keyEvent('F13'), 100);
  state = resetHidActive(state);

  assert.equal(state.slots[1].pressed, true);
  assert.equal(state.slots[1].active.active, false);
  assert.equal(state.slots[1].active.prevPressed, true);
});
