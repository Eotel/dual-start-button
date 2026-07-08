import { test } from 'node:test';
import assert from 'node:assert/strict';

import { COMMAND } from './protocol.js';
import { createResultCorrelator, describeControlError } from './control-result.js';

// Deterministic stand-in for setTimeout/clearTimeout so the timeout branch is
// testable without real waiting.
function fakeClock() {
  const timers = new Map();
  let nextId = 1;
  return {
    setTimeoutFn(fn, ms) {
      const id = nextId;
      nextId += 1;
      timers.set(id, { fn, ms });
      return id;
    },
    clearTimeoutFn(id) {
      timers.delete(id);
    },
    fireAll() {
      for (const [id, timer] of [...timers]) {
        timers.delete(id);
        timer.fn();
      }
    },
    get size() {
      return timers.size;
    },
  };
}

function correlator(clock, timeoutMs = 3000) {
  return createResultCorrelator({
    timeoutMs,
    setTimeoutFn: clock.setTimeoutFn,
    clearTimeoutFn: clock.clearTimeoutFn,
  });
}

test('deliver resolves a pending expect with the matching-cmd result', async () => {
  const clock = fakeClock();
  const c = correlator(clock);
  const pending = c.expect(COMMAND.LINK);
  const result = { v: 1, ok: true, cmd: COMMAND.LINK, message: 'linked' };
  assert.equal(c.deliver(result), true);
  assert.deepEqual(await pending, result);
  assert.equal(clock.size, 0); // timer cleared
});

test('a rejected command still resolves: correlation is by cmd, not by ok', async () => {
  const clock = fakeClock();
  const c = correlator(clock);
  const pending = c.expect(COMMAND.LINK);
  const result = { v: 1, ok: false, cmd: COMMAND.LINK, error: 'link_conflict' };
  assert.equal(c.deliver(result), true);
  const got = await pending;
  assert.equal(got.ok, false);
  assert.equal(got.error, 'link_conflict');
});

test('deliver ignores results that match no pending command', () => {
  const clock = fakeClock();
  const c = correlator(clock);
  assert.equal(c.deliver({ v: 1, ok: true, cmd: COMMAND.LINK }), false);
  c.expect(COMMAND.LINK).catch(() => {});
  assert.equal(c.deliver({ v: 1, ok: true, cmd: COMMAND.UNLINK }), false);
  assert.equal(c.deliver({ v: 1, ok: true }), false);
  assert.equal(c.deliver(null), false);
});

test('two pending expects for the same cmd resolve in FIFO order', async () => {
  const clock = fakeClock();
  const c = correlator(clock);
  const first = c.expect(COMMAND.SET_ARMED);
  const second = c.expect(COMMAND.SET_ARMED);
  c.deliver({ v: 1, ok: true, cmd: COMMAND.SET_ARMED, message: 'armed' });
  c.deliver({ v: 1, ok: true, cmd: COMMAND.SET_ARMED, message: 'disarmed' });
  assert.equal((await first).message, 'armed');
  assert.equal((await second).message, 'disarmed');
});

test('timeout rejects the pending expect and forgets it', async () => {
  const clock = fakeClock();
  const c = correlator(clock, 1234);
  const pending = c.expect(COMMAND.UNLINK);
  clock.fireAll();
  await assert.rejects(pending, /no ControlResult for cmd=2 within 1234ms/);
  // A late result no longer matches anything.
  assert.equal(c.deliver({ v: 1, ok: true, cmd: COMMAND.UNLINK }), false);
});

test('describeControlError points link_conflict at the force option', () => {
  const text = describeControlError({
    ok: false,
    cmd: COMMAND.LINK,
    error: 'link_conflict',
    message: 'already linked to another group; use force',
  });
  assert.match(text, /link_conflict/);
  assert.match(text, /Force link/);
});

test('describeControlError reports other errors with code and message', () => {
  const text = describeControlError({ ok: false, cmd: COMMAND.LINK, error: 'invalid_slot', message: 'LINK requires slot 1 or 2' });
  assert.match(text, /invalid_slot/);
  assert.match(text, /LINK requires slot 1 or 2/);
  assert.doesNotMatch(text, /Force link/);
});
