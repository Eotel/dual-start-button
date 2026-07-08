import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

import { COMMAND, FLAG, CONTROL_FLAG0, parseState, makeCommand } from './protocol.js';

const here = dirname(fileURLToPath(import.meta.url));

function loadVectors(name) {
  const path = join(here, '..', 'test-vectors', name);
  return JSON.parse(readFileSync(path, 'utf8'));
}

const buttonState = loadVectors('button_state.json');
const control = loadVectors('control_command.json');

function hexToDataView(hex) {
  const bytes = new Uint8Array(hex.length / 2);
  for (let i = 0; i < bytes.length; i += 1) {
    bytes[i] = parseInt(hex.slice(i * 2, i * 2 + 2), 16);
  }
  return new DataView(bytes.buffer);
}

function bytesToHex(buffer) {
  return [...new Uint8Array(buffer)]
    .map((b) => b.toString(16).padStart(2, '0'))
    .join('');
}

test('protocol constants match the wire flag/command layout', () => {
  assert.equal(COMMAND.LINK, 1);
  assert.equal(COMMAND.UNLINK, 2);
  assert.equal(COMMAND.SET_ARMED, 3);
  assert.equal(COMMAND.IDENTIFY, 4);
  assert.equal(COMMAND.FACTORY_RESET_LINK, 5);
  assert.equal(FLAG.PRESSED, 1 << 0);
  assert.equal(FLAG.ARMED, 1 << 1);
  assert.equal(FLAG.LINKED, 1 << 2);
  assert.equal(FLAG.LONG_PRESSED, 1 << 3);
  assert.equal(FLAG.CONNECTED, 1 << 4);
  assert.equal(FLAG.ERROR, 1 << 5);
  assert.equal(CONTROL_FLAG0, 1 << 0);
});

for (const vec of buttonState.valid) {
  test(`parseState decodes ButtonState vector: ${vec.name}`, () => {
    const dv = hexToDataView(vec.hex);
    const state = parseState(dv);
    const e = vec.expect;

    // Raw fields.
    assert.equal(state.version, e.version);
    assert.equal(state.type, e.type);
    assert.equal(state.flags, e.flags);
    assert.equal(state.linkSlot, e.link_slot);
    assert.equal(state.seq, e.seq);
    assert.equal(state.uptimeMs, e.uptime_ms);
    assert.equal(state.deviceHash, e.device_hash);
    assert.equal(state.linkGroupId, e.link_group_id);
    assert.equal(state.aux, e.aux);

    // Derived booleans, expectations computed from the raw flags value.
    assert.equal(state.pressed, Boolean(e.flags & FLAG.PRESSED));
    assert.equal(state.armed, Boolean(e.flags & FLAG.ARMED));
    assert.equal(state.linked, Boolean(e.flags & FLAG.LINKED));
    assert.equal(state.longPressed, Boolean(e.flags & FLAG.LONG_PRESSED));
    assert.equal(state.deviceConnectedFlag, Boolean(e.flags & FLAG.CONNECTED));
    assert.equal(state.error, Boolean(e.flags & FLAG.ERROR));
  });
}

for (const vec of buttonState.invalid) {
  test(`parseState rejects wrong-length ButtonState: ${vec.name}`, () => {
    const dv = hexToDataView(vec.hex);
    const byteLength = vec.hex.length / 2;
    assert.throws(
      () => parseState(dv),
      (err) => {
        assert.ok(err instanceof Error);
        assert.ok(
          err.message.includes(`got ${byteLength}`),
          `expected message to report ${byteLength} bytes, got: ${err.message}`,
        );
        return true;
      },
    );
  });
}

for (const vec of control.valid) {
  test(`makeCommand encodes Control vector: ${vec.name}`, () => {
    const e = vec.expect;
    const buffer = makeCommand(e.command, e.slot, e.flags, e.group_id, e.value);
    assert.ok(buffer instanceof ArrayBuffer);
    assert.equal(bytesToHex(buffer), vec.hex);
  });
}
