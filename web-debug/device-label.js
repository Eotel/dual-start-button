export function shortDeviceId(deviceId) {
  const text = String(deviceId || '');
  return text.length > 6 ? text.slice(0, 6) : text;
}

export function deviceDisplayName(info, bluetoothDevice = null) {
  const base =
    info?.name || bluetoothDevice?.name || info?.device_id || bluetoothDevice?.id || 'device';
  const suffix = shortDeviceId(info?.device_id);
  return suffix ? `${base} (${suffix})` : base;
}
