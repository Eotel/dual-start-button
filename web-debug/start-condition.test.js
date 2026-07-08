import { test } from 'node:test';
import assert from 'node:assert/strict';

import { evaluateStartCondition } from './start-condition.js';

const GROUP = 11259375;
const STALE_MS = 1500;
const CONFIRM_HOLD_MS = 300;
const NOW = 1_000_000;

// A slot entry as app.js passes it: { connected, state } or null.
// state defaults to a fully-satisfied slot; overrides mutate one field.
function entry(slot, stateOverrides = {}, entryOverrides = {}) {
  return {
    connected: true,
    ...entryOverrides,
    state: {
      linkGroupId: GROUP,
      linkSlot: slot,
      armed: true,
      pressed: true,
      lastReceivedAt: NOW,
      pressedSince: NOW - CONFIRM_HOLD_MS,
      ...stateOverrides,
    },
  };
}

// Fully-satisfied argument bundle; each test overrides one axis.
function args(overrides = {}) {
  return {
    pair: { groupId: GROUP, slot1: { deviceId: 'd1' }, slot2: { deviceId: 'd2' } },
    slot1: entry(1),
    slot2: entry(2),
    now: NOW,
    staleMs: STALE_MS,
    confirmHoldMs: CONFIRM_HOLD_MS,
    ...overrides,
  };
}

test('all conditions satisfied → ok, held reason', () => {
  assert.deepEqual(evaluateStartCondition(args()), {
    ok: true,
    reason: 'both buttons pressed and fresh; held 300ms',
  });
});

test('slot 1 not linked in pair', () => {
  const pair = { groupId: GROUP, slot1: null, slot2: { deviceId: 'd2' } };
  assert.deepEqual(evaluateStartCondition(args({ pair })), {
    ok: false,
    reason: 'slot 1 is not linked',
  });
});

test('slot 2 not linked in pair', () => {
  const pair = { groupId: GROUP, slot1: { deviceId: 'd1' }, slot2: null };
  assert.deepEqual(evaluateStartCondition(args({ pair })), {
    ok: false,
    reason: 'slot 2 is not linked',
  });
});

test('slot 1 entry missing (null)', () => {
  assert.deepEqual(evaluateStartCondition(args({ slot1: null })), {
    ok: false,
    reason: 'slot 1 is not connected',
  });
});

test('slot 1 connected=false', () => {
  assert.deepEqual(evaluateStartCondition(args({ slot1: entry(1, {}, { connected: false }) })), {
    ok: false,
    reason: 'slot 1 is not connected',
  });
});

test('slot 2 entry missing (null)', () => {
  assert.deepEqual(evaluateStartCondition(args({ slot2: null })), {
    ok: false,
    reason: 'slot 2 is not connected',
  });
});

test('slot 2 connected=false', () => {
  assert.deepEqual(evaluateStartCondition(args({ slot2: entry(2, {}, { connected: false }) })), {
    ok: false,
    reason: 'slot 2 is not connected',
  });
});

test('slot 1 state null', () => {
  assert.deepEqual(evaluateStartCondition(args({ slot1: { connected: true, state: null } })), {
    ok: false,
    reason: 'slot 1 has no state yet',
  });
});

test('slot 2 state null', () => {
  assert.deepEqual(evaluateStartCondition(args({ slot2: { connected: true, state: null } })), {
    ok: false,
    reason: 'slot 2 has no state yet',
  });
});

test('slot 1 link group id mismatch', () => {
  assert.deepEqual(evaluateStartCondition(args({ slot1: entry(1, { linkGroupId: GROUP + 1 }) })), {
    ok: false,
    reason: 'slot 1 device-link mismatch',
  });
});

test('slot 1 link slot mismatch', () => {
  assert.deepEqual(evaluateStartCondition(args({ slot1: entry(1, { linkSlot: 2 }) })), {
    ok: false,
    reason: 'slot 1 device-link mismatch',
  });
});

test('slot 2 link group id mismatch', () => {
  assert.deepEqual(evaluateStartCondition(args({ slot2: entry(2, { linkGroupId: GROUP + 1 }) })), {
    ok: false,
    reason: 'slot 2 device-link mismatch',
  });
});

test('slot 2 link slot mismatch', () => {
  assert.deepEqual(evaluateStartCondition(args({ slot2: entry(2, { linkSlot: 1 }) })), {
    ok: false,
    reason: 'slot 2 device-link mismatch',
  });
});

test('slot 1 disarmed', () => {
  assert.deepEqual(evaluateStartCondition(args({ slot1: entry(1, { armed: false }) })), {
    ok: false,
    reason: 'slot 1 is disarmed',
  });
});

test('slot 2 disarmed', () => {
  assert.deepEqual(evaluateStartCondition(args({ slot2: entry(2, { armed: false }) })), {
    ok: false,
    reason: 'slot 2 is disarmed',
  });
});

test('slot 1 stale: age > staleMs fails', () => {
  const slot1 = entry(1, { lastReceivedAt: NOW - (STALE_MS + 1) });
  assert.deepEqual(evaluateStartCondition(args({ slot1 })), {
    ok: false,
    reason: 'slot 1 state is stale',
  });
});

test('slot 1 fresh: age === staleMs passes (freshness boundary uses >)', () => {
  const slot1 = entry(1, { lastReceivedAt: NOW - STALE_MS });
  assert.deepEqual(evaluateStartCondition(args({ slot1 })), {
    ok: true,
    reason: 'both buttons pressed and fresh; held 300ms',
  });
});

test('slot 2 stale: age > staleMs fails', () => {
  const slot2 = entry(2, { lastReceivedAt: NOW - (STALE_MS + 1) });
  assert.deepEqual(evaluateStartCondition(args({ slot2 })), {
    ok: false,
    reason: 'slot 2 state is stale',
  });
});

test('slot 2 fresh: age === staleMs passes', () => {
  const slot2 = entry(2, { lastReceivedAt: NOW - STALE_MS });
  assert.deepEqual(evaluateStartCondition(args({ slot2 })), {
    ok: true,
    reason: 'both buttons pressed and fresh; held 300ms',
  });
});

test('slot 1 not pressed', () => {
  assert.deepEqual(evaluateStartCondition(args({ slot1: entry(1, { pressed: false }) })), {
    ok: false,
    reason: 'slot 1 is not pressed',
  });
});

test('slot 2 not pressed', () => {
  assert.deepEqual(evaluateStartCondition(args({ slot2: entry(2, { pressed: false }) })), {
    ok: false,
    reason: 'slot 2 is not pressed',
  });
});

test('confirm hold: held just below confirmHoldMs → confirming', () => {
  const since = NOW - (CONFIRM_HOLD_MS - 1);
  const slot1 = entry(1, { pressedSince: since });
  const slot2 = entry(2, { pressedSince: since });
  assert.deepEqual(evaluateStartCondition(args({ slot1, slot2 })), {
    ok: false,
    reason: 'both pressed, confirming 299/300ms',
  });
});

test('confirm hold: held === confirmHoldMs → ok', () => {
  const since = NOW - CONFIRM_HOLD_MS;
  const slot1 = entry(1, { pressedSince: since });
  const slot2 = entry(2, { pressedSince: since });
  assert.deepEqual(evaluateStartCondition(args({ slot1, slot2 })), {
    ok: true,
    reason: 'both buttons pressed and fresh; held 300ms',
  });
});

test('pressedSince undefined on one side → held 0, confirming', () => {
  const slot1 = entry(1, { pressedSince: undefined });
  assert.deepEqual(evaluateStartCondition(args({ slot1 })), {
    ok: false,
    reason: 'both pressed, confirming 0/300ms',
  });
});

test('pressedSince undefined on both sides → held 0, confirming', () => {
  const slot1 = entry(1, { pressedSince: undefined });
  const slot2 = entry(2, { pressedSince: undefined });
  assert.deepEqual(evaluateStartCondition(args({ slot1, slot2 })), {
    ok: false,
    reason: 'both pressed, confirming 0/300ms',
  });
});

test('pressedSince 0 is a valid timestamp, not "missing" (SPEC uses ??)', () => {
  // With a zero-origin clock, pressedSince=0 and now=confirmHoldMs must
  // satisfy the hold; `||` would treat 0 as missing and report held 0.
  const slot1 = entry(1, { pressedSince: 0, lastReceivedAt: CONFIRM_HOLD_MS });
  const slot2 = entry(2, { pressedSince: 0, lastReceivedAt: CONFIRM_HOLD_MS });
  assert.deepEqual(
    evaluateStartCondition(args({ slot1, slot2, now: CONFIRM_HOLD_MS })),
    { ok: true, reason: 'both buttons pressed and fresh; held 300ms' },
  );
});

test('connected=false wins even if state still reports pressed', () => {
  // Disconnect semantics: caller zeroes pressed, but the pure function must
  // already fail on connected before it ever inspects pressed/state.
  const slot1 = entry(1, { pressed: true, armed: true }, { connected: false });
  assert.deepEqual(evaluateStartCondition(args({ slot1 })), {
    ok: false,
    reason: 'slot 1 is not connected',
  });
});
