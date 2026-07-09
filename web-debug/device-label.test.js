import assert from 'node:assert/strict';
import { test } from 'node:test';

import { deviceDisplayName, shortDeviceId } from './device-label.js';

test('shortDeviceId uses the leading device id characters', () => {
  assert.equal(shortDeviceId('C0FEBCDAD4D4'), 'C0FEBC');
  assert.equal(shortDeviceId('38AF84DAD4D4'), '38AF84');
});

test('deviceDisplayName disambiguates duplicate BLE local names', () => {
  assert.equal(
    deviceDisplayName({ name: 'DSB-D4D4', device_id: 'C0FEBCDAD4D4' }),
    'DSB-D4D4 (C0FEBC)',
  );
  assert.equal(
    deviceDisplayName({ name: 'DSB-D4D4', device_id: '38AF84DAD4D4' }),
    'DSB-D4D4 (38AF84)',
  );
});

test('deviceDisplayName falls back to the browser Bluetooth device name', () => {
  assert.equal(deviceDisplayName(null, { name: 'DSB-8DE99B' }), 'DSB-8DE99B');
});
