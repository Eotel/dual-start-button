import { INITIAL_ACTIVE, reduceActive, resetActive } from './active-state.js';
import { confirmsLink, createResultCorrelator, describeControlError } from './control-result.js';
import { COMMAND, CONTROL_FLAG0, makeCommand, parseState } from './protocol.js';
import { evaluateStartCondition } from './start-condition.js';

const SERVICE_UUID = '7b1f0000-6d4f-4f4a-9a4f-2d0c7a7a0001';
const DEVICE_INFO_UUID = '7b1f0001-6d4f-4f4a-9a4f-2d0c7a7a0001';
const BUTTON_STATE_UUID = '7b1f0002-6d4f-4f4a-9a4f-2d0c7a7a0001';
const CONTROL_UUID = '7b1f0003-6d4f-4f4a-9a4f-2d0c7a7a0001';
const CONTROL_RESULT_UUID = '7b1f0004-6d4f-4f4a-9a4f-2d0c7a7a0001';

const STALE_MS = 1500;

const devices = new Map(); // key: browser bluetooth device id
// Host-derived Active per slot (SPEC section 12): toggled by press down-edges
// of the locally linked device, reset on disconnect/unlink/relink/new group.
const slotActive = { 1: INITIAL_ACTIVE, 2: INITIAL_ACTIVE };
const decoder = new TextDecoder();

const $ = (selector) => document.querySelector(selector);
const devicesEl = $('#devices');
const logEl = $('#log');
const template = $('#deviceTemplate');

function loadPair() {
  const raw = localStorage.getItem('dsb.pair');
  if (raw) {
    try {
      const parsed = JSON.parse(raw);
      if (parsed.groupId) return parsed;
    } catch (_) {}
  }
  return { groupId: newGroupId(), slot1: null, slot2: null };
}

let pair = loadPair();

function savePair() {
  localStorage.setItem('dsb.pair', JSON.stringify(pair));
}

function newGroupId() {
  const arr = new Uint32Array(1);
  do {
    crypto.getRandomValues(arr);
  } while (arr[0] === 0);
  return arr[0] >>> 0;
}

function log(message, data) {
  const time = new Date().toLocaleTimeString();
  let line = `[${time}] ${message}`;
  if (data !== undefined) {
    line += ` ${typeof data === 'string' ? data : JSON.stringify(data)}`;
  }
  logEl.textContent = `${line}\n${logEl.textContent}`.slice(0, 12000);
}

function dataViewToText(dv) {
  return decoder.decode(new Uint8Array(dv.buffer, dv.byteOffset, dv.byteLength));
}

async function writeCommand(entry, command, slot = 0, flags = 0, groupId = 0, value = 0) {
  const buf = makeCommand(command, slot, flags, groupId, value);
  const char = entry.chars.control;
  if (char.writeValueWithResponse) {
    await char.writeValueWithResponse(buf);
  } else {
    await char.writeValue(buf);
  }
}

function getForceFlag() {
  return $('#forceLinkCheck').checked ? CONTROL_FLAG0 : 0;
}

function findEntryByDeviceId(deviceId) {
  for (const entry of devices.values()) {
    if (entry.info?.device_id === deviceId) return entry;
  }
  return null;
}

function slotEntry(slot) {
  const slotRecord = slot === 1 ? pair.slot1 : pair.slot2;
  if (!slotRecord?.deviceId) return null;
  return findEntryByDeviceId(slotRecord.deviceId);
}

function linkedSlotsOf(deviceId) {
  if (!deviceId) return [];
  const slots = [];
  if (pair.slot1?.deviceId === deviceId) slots.push(1);
  if (pair.slot2?.deviceId === deviceId) slots.push(2);
  return slots;
}

// Reset a slot's active state. When the slot's device state is already known
// (connected and received), seed the baseline from it so the very next press
// toggles instead of being consumed as a baseline sample.
function resetSlotActive(slot) {
  const entry = slotEntry(slot);
  const known = entry?.connected && entry.state ? entry.state.pressed : null;
  slotActive[slot] = resetActive(known);
}

// Single owner of slot-binding changes: whenever a slot's device record
// changes, its active state resets and the pair is persisted.
function setSlotRecord(slot, record) {
  if (slot === 1) pair.slot1 = record;
  if (slot === 2) pair.slot2 = record;
  resetSlotActive(slot);
  savePair();
}

async function connectBluetoothDevice(bluetoothDevice) {
  const existing = devices.get(bluetoothDevice.id);
  if (existing?.connected) return existing;

  log(`connecting ${bluetoothDevice.name || bluetoothDevice.id}`);
  bluetoothDevice.addEventListener('gattserverdisconnected', () => {
    const entry = devices.get(bluetoothDevice.id);
    if (!entry) return;
    entry.connected = false;
    if (entry.state) {
      entry.state.pressed = false;
    }
    for (const slot of linkedSlotsOf(entry.info?.device_id)) resetSlotActive(slot);
    log(`disconnected ${entry.info?.device_id || bluetoothDevice.name || bluetoothDevice.id}`);
    render();
  });

  const server = await bluetoothDevice.gatt.connect();
  const service = await server.getPrimaryService(SERVICE_UUID);
  const deviceInfoChar = await service.getCharacteristic(DEVICE_INFO_UUID);
  const stateChar = await service.getCharacteristic(BUTTON_STATE_UUID);
  const controlChar = await service.getCharacteristic(CONTROL_UUID);
  const resultChar = await service.getCharacteristic(CONTROL_RESULT_UUID);

  const infoText = dataViewToText(await deviceInfoChar.readValue());
  const info = JSON.parse(infoText);

  const entry = {
    bluetoothDevice,
    connected: true,
    info,
    state: null,
    result: null,
    correlator: createResultCorrelator(),
    chars: {
      deviceInfo: deviceInfoChar,
      state: stateChar,
      control: controlChar,
      result: resultChar,
    },
  };

  devices.set(bluetoothDevice.id, entry);

  resultChar.addEventListener('characteristicvaluechanged', (event) => {
    const text = dataViewToText(event.target.value);
    try {
      entry.result = JSON.parse(text);
    } catch (_) {
      entry.result = { raw: text };
    }
    log(`control result ${entry.info.device_id}`, entry.result);
    entry.correlator.deliver(entry.result);
    refreshInfo(entry).catch((err) => log(`refresh info failed: ${err.message}`));
    render();
  });
  await resultChar.startNotifications();

  stateChar.addEventListener('characteristicvaluechanged', (event) => {
    applyState(entry, event.target.value);
  });
  await stateChar.startNotifications();
  applyState(entry, await stateChar.readValue());

  log(`connected ${info.device_id}`, info);
  render();
  return entry;
}

function applyState(entry, dv) {
  const prev = entry.state;
  const next = parseState(dv);
  if (prev && prev.seq === next.seq && prev.uptimeMs === next.uptimeMs) {
    return;
  }
  // Reception bookkeeping (freshness, Active toggling) belongs to the host
  // layer, not the wire decoder (SPEC section 12).
  next.lastReceivedAt = Date.now();
  entry.state = next;
  for (const slot of linkedSlotsOf(entry.info?.device_id)) {
    slotActive[slot] = reduceActive(slotActive[slot], next.pressed);
  }
  render();
}

async function refreshInfo(entry) {
  const text = dataViewToText(await entry.chars.deviceInfo.readValue());
  entry.info = JSON.parse(text);
}

async function addDevice() {
  if (!navigator.bluetooth) {
    log('Web Bluetooth is not available in this browser');
    return;
  }
  const bluetoothDevice = await navigator.bluetooth.requestDevice({
    filters: [{ services: [SERVICE_UUID] }],
    optionalServices: [SERVICE_UUID],
  });
  await connectBluetoothDevice(bluetoothDevice);
}

async function reconnectGrantedDevices() {
  if (!navigator.bluetooth?.getDevices) {
    log('navigator.bluetooth.getDevices is not available in this browser');
    return;
  }
  const granted = await navigator.bluetooth.getDevices();
  log(`granted devices: ${granted.length}`);
  for (const bluetoothDevice of granted) {
    try {
      await connectBluetoothDevice(bluetoothDevice);
    } catch (err) {
      log(`reconnect failed: ${bluetoothDevice.name || bluetoothDevice.id}: ${err.message}`);
    }
  }
}

// Writes a command and returns the device's ControlResult verdict. A completed
// GATT write is delivery, not acceptance: the device may reject (link_conflict
// etc.) and the result arrives asynchronously via Notify. The expectation is
// registered before writing so a fast notify cannot slip past, and cancelled
// if the write throws so a retry cannot be satisfied by the dead attempt.
async function writeConfirmed(entry, command, slot = 0, flags = 0, groupId = 0, value = 0) {
  const expectation = entry.correlator.expect(command);
  expectation.result.catch(() => {}); // rejection is surfaced via the return below
  try {
    await writeCommand(entry, command, slot, flags, groupId, value);
  } catch (err) {
    expectation.cancel();
    throw err;
  }
  return expectation.result;
}

// The local pair is only updated once the device confirms with ok:true.
async function linkAsSlot(entry, slot) {
  const result = await writeConfirmed(entry, COMMAND.LINK, slot, getForceFlag(), pair.groupId, 0);
  if (!result.ok) {
    log(`LINK slot ${slot} rejected by ${entry.info.device_id}: ${describeControlError(result)}`);
    render();
    return;
  }
  if (!confirmsLink(result, { groupId: pair.groupId, slot })) {
    log(
      `LINK slot ${slot} result from ${entry.info.device_id} does not match the request (stale result?)`,
      result,
    );
    render();
    return;
  }
  const record = {
    deviceId: entry.info.device_id,
    deviceHash: entry.info.device_hash,
    displayName: entry.info.name || entry.bluetoothDevice.name || entry.info.device_id,
  };
  setSlotRecord(slot, record);
  log(`linked local slot ${slot}`, record);
  render();
}

async function unlinkDevice(entry) {
  const result = await writeConfirmed(entry, COMMAND.UNLINK, 0, getForceFlag(), pair.groupId, 0);
  if (!result.ok) {
    log(`UNLINK rejected by ${entry.info.device_id}: ${describeControlError(result)}`);
    render();
    return;
  }
  for (const slot of linkedSlotsOf(entry.info.device_id)) setSlotRecord(slot, null);
  log(`unlinked local ${entry.info.device_id}`);
  render();
}

async function identifyDevice(entry) {
  await writeCommand(entry, COMMAND.IDENTIFY, 0, 0, pair.groupId, 3000);
}

async function setArmed(entry, armed) {
  await writeCommand(entry, COMMAND.SET_ARMED, 0, armed ? CONTROL_FLAG0 : 0, pair.groupId, 0);
}

function freshText(state) {
  if (!state) return 'no state';
  const age = Date.now() - state.lastReceivedAt;
  return age <= STALE_MS ? `fresh ${age}ms` : `stale ${age}ms`;
}

function slotSummary(slot) {
  const record = slot === 1 ? pair.slot1 : pair.slot2;
  const entry = slotEntry(slot);
  if (!record) return '<div class="slotState empty">未link</div>';
  if (!entry) {
    return `<div class="slotState empty">${escapeHtml(record.displayName || record.deviceId)}<br><small>not connected</small></div>`;
  }
  const state = entry.state;
  const okLink = state && state.linkGroupId === pair.groupId && state.linkSlot === slot;
  return `<div class="slotState">
    <strong>${escapeHtml(record.displayName || record.deviceId)}</strong><br>
    <small>${escapeHtml(record.deviceId)}</small><br>
    active=${slotActive[slot].active ? 'YES' : 'no'} / pressed=${state?.pressed ? 'YES' : 'no'} / armed=${state?.armed ? 'YES' : 'no'} / ${freshText(state)}<br>
    device-link=${okLink ? 'ok' : 'mismatch'}
  </div>`;
}

function computeStartCondition() {
  return evaluateStartCondition({
    pair,
    slot1: slotEntry(1),
    slot2: slotEntry(2),
    slot1Active: slotActive[1].active,
    slot2Active: slotActive[2].active,
    now: Date.now(),
    staleMs: STALE_MS,
  });
}

function renderDeviceCard(entry) {
  const node = template.content.firstElementChild.cloneNode(true);
  const title = entry.info?.name || entry.bluetoothDevice.name || entry.bluetoothDevice.id;
  node.querySelector('.deviceTitle').textContent = title;
  const badge = node.querySelector('.connectionBadge');
  badge.textContent = entry.connected ? 'connected' : 'disconnected';
  badge.classList.toggle('ok', entry.connected);
  badge.classList.toggle('ng', !entry.connected);
  node.querySelector('.deviceId').textContent = entry.info?.device_id || '-';
  node.querySelector('.deviceHash').textContent = entry.info?.device_hash ?? '-';
  node.querySelector('.fw').textContent = entry.info?.fw || '-';
  node.querySelector('.model').textContent = entry.info?.model || '-';
  node.querySelector('.linkInfo').textContent =
    `group=${entry.state?.linkGroupId ?? entry.info?.link_group_id ?? 0}, slot=${entry.state?.linkSlot ?? entry.info?.link_slot ?? 0}`;
  node.querySelector('.pressed').textContent = entry.state?.pressed ? 'YES' : 'no';
  node.querySelector('.armed').textContent = entry.state?.armed ? 'YES' : 'no';
  node.querySelector('.seq').textContent = entry.state?.seq ?? '-';
  node.querySelector('.fresh').textContent = freshText(entry.state);
  node.querySelector('.raw').textContent = JSON.stringify(
    { info: entry.info, state: entry.state, result: entry.result },
    null,
    2,
  );

  node
    .querySelector('.linkSlot1')
    .addEventListener('click', () => linkAsSlot(entry, 1).catch(showError));
  node
    .querySelector('.linkSlot2')
    .addEventListener('click', () => linkAsSlot(entry, 2).catch(showError));
  node
    .querySelector('.unlinkDevice')
    .addEventListener('click', () => unlinkDevice(entry).catch(showError));
  node
    .querySelector('.identifyDevice')
    .addEventListener('click', () => identifyDevice(entry).catch(showError));
  node
    .querySelector('.armDevice')
    .addEventListener('click', () => setArmed(entry, true).catch(showError));
  node
    .querySelector('.disarmDevice')
    .addEventListener('click', () => setArmed(entry, false).catch(showError));
  node.querySelector('.disconnectDevice').addEventListener('click', () => {
    if (entry.bluetoothDevice.gatt.connected) entry.bluetoothDevice.gatt.disconnect();
  });
  return node;
}

function render() {
  $('#groupIdText').textContent = String(pair.groupId >>> 0);
  $('#slot1State').outerHTML = `<div id="slot1State">${slotSummary(1)}</div>`;
  $('#slot2State').outerHTML = `<div id="slot2State">${slotSummary(2)}</div>`;

  const condition = computeStartCondition();
  const conditionEl = $('#startCondition');
  conditionEl.textContent = condition.ok ? 'READY / START' : 'NOT READY';
  conditionEl.classList.toggle('ok', condition.ok);
  conditionEl.classList.toggle('ng', !condition.ok);
  $('#startReason').textContent = condition.reason;

  devicesEl.innerHTML = '';
  for (const entry of devices.values()) {
    devicesEl.appendChild(renderDeviceCard(entry));
  }
}

function escapeHtml(value) {
  return String(value)
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#039;');
}

function showError(err) {
  console.error(err);
  log(`ERROR: ${err.message || err}`);
}

function init() {
  const hasWebBluetooth = Boolean(navigator.bluetooth);
  $('#browserStatus').textContent = hasWebBluetooth
    ? 'Web Bluetooth available'
    : 'Web Bluetooth unavailable. Use a compatible Chromium-based browser on HTTPS or localhost.';
  $('#addDeviceBtn').disabled = !hasWebBluetooth;
  $('#reconnectGrantedBtn').disabled = !hasWebBluetooth || !navigator.bluetooth.getDevices;

  $('#addDeviceBtn').addEventListener('click', () => addDevice().catch(showError));
  $('#reconnectGrantedBtn').addEventListener('click', () =>
    reconnectGrantedDevices().catch(showError),
  );
  $('#newGroupBtn').addEventListener('click', () => {
    if (
      !confirm(
        'Create a new group ID and clear local slot assignments? Device-side links are not cleared.',
      )
    )
      return;
    pair = { groupId: newGroupId(), slot1: null, slot2: null };
    resetSlotActive(1);
    resetSlotActive(2);
    savePair();
    render();
  });
  $('#resetActiveBtn').addEventListener('click', () => {
    resetSlotActive(1);
    resetSlotActive(2);
    log('active reset (both slots)');
    render();
  });

  savePair();
  render();
  setInterval(render, 250);
}

init();
