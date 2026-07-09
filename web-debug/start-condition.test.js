import { test } from 'node:test';
import assert from 'node:assert/strict';

import { evaluateStartCondition } from './start-condition.js';

const GROUP = 11259375;
const STALE_MS = 1500;
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
      lastReceivedAt: NOW,
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
    slot1Active: true,
    slot2Active: true,
    now: NOW,
    staleMs: STALE_MS,
    ...overrides,
  };
}

test('all conditions satisfied → ok', () => {
  assert.deepEqual(evaluateStartCondition(args()), {
    ok: true,
    reason: 'both slots active and fresh',
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
  // Disconnect semantics: the caller resets active, but the pure function
  // must already fail on connected before it ever inspects active
  // (args() defaults slot1Active to true, so connected wins here).
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
  // Stale wins over active: args() defaults slot1Active to true, so a stale
  // slot cannot start even while its active toggle is on.
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
    reason: 'both slots active and fresh',
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
    reason: 'both slots active and fresh',
  });
});

test('slot 1 not active', () => {
  assert.deepEqual(evaluateStartCondition(args({ slot1Active: false })), {
    ok: false,
    reason: 'slot 1 is not active',
  });
});

test('slot 2 not active', () => {
  assert.deepEqual(evaluateStartCondition(args({ slot2Active: false })), {
    ok: false,
    reason: 'slot 2 is not active',
  });
});

test('raw pressed state is irrelevant: active alone decides', () => {
  // Toggle model: a released button whose slot was toggled active still
  // satisfies the condition; no simultaneous press or confirm hold.
  const slot1 = entry(1, { pressed: false });
  const slot2 = entry(2, { pressed: false });
  assert.deepEqual(evaluateStartCondition(args({ slot1, slot2 })), {
    ok: true,
    reason: 'both slots active and fresh',
  });
});
