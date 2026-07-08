// Protocol-level encode/decode for the Dual Start Button BLE contract.
// Fixed little-endian binary packets per SPEC.md sections 8 (ButtonState) and
// 9 (Control). Shared by the web-debug app and the golden-vector tests.

export const COMMAND = {
  LINK: 1,
  UNLINK: 2,
  SET_ARMED: 3,
  IDENTIFY: 4,
  FACTORY_RESET_LINK: 5,
};

export const FLAG = {
  PRESSED: 1 << 0,
  ARMED: 1 << 1,
  LINKED: 1 << 2,
  LONG_PRESSED: 1 << 3,
  CONNECTED: 1 << 4,
  ERROR: 1 << 5,
};

export const CONTROL_FLAG0 = 1 << 0;

export function parseState(dv, now = Date.now()) {
  if (dv.byteLength !== 20) {
    throw new Error(`ButtonState must be 20 bytes, got ${dv.byteLength}`);
  }
  const flags = dv.getUint8(2);
  return {
    version: dv.getUint8(0),
    type: dv.getUint8(1),
    flags,
    pressed: Boolean(flags & FLAG.PRESSED),
    armed: Boolean(flags & FLAG.ARMED),
    linked: Boolean(flags & FLAG.LINKED),
    longPressed: Boolean(flags & FLAG.LONG_PRESSED),
    deviceConnectedFlag: Boolean(flags & FLAG.CONNECTED),
    error: Boolean(flags & FLAG.ERROR),
    linkSlot: dv.getUint8(3),
    seq: dv.getUint16(4, true),
    uptimeMs: dv.getUint32(6, true),
    deviceHash: dv.getUint32(10, true),
    linkGroupId: dv.getUint32(14, true),
    aux: dv.getUint16(18, true),
    lastReceivedAt: now,
  };
}

export function makeCommand(command, slot = 0, flags = 0, groupId = 0, value = 0) {
  const buf = new ArrayBuffer(12);
  const dv = new DataView(buf);
  dv.setUint8(0, 1);
  dv.setUint8(1, command);
  dv.setUint8(2, slot);
  dv.setUint8(3, flags);
  dv.setUint32(4, groupId >>> 0, true);
  dv.setUint32(8, value >>> 0, true);
  return buf;
}
