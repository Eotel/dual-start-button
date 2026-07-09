// Host-side start-condition evaluation (SPEC.md section 12).
// Pure function: the host decides whether both linked slots satisfy the
// connected / armed / fresh / active condition. Firmware only reports state;
// Active is host-derived (see active-state.js) and passed in as separate
// slot1Active/slot2Active booleans rather than on the entries: its lifetime
// is the slot's, not the device entry's (an entry can be null while the
// slot's tracker exists). `slot1` and `slot2` are entry-like objects
// ({ connected, state } or null); `now` and `staleMs` are injected so the
// decision is deterministic and testable.

export function evaluateStartCondition({ pair, slot1, slot2, slot1Active, slot2Active, now, staleMs }) {
  const a = slot1;
  const b = slot2;
  if (!pair.slot1) return { ok: false, reason: 'slot 1 is not linked' };
  if (!pair.slot2) return { ok: false, reason: 'slot 2 is not linked' };
  if (!a?.connected) return { ok: false, reason: 'slot 1 is not connected' };
  if (!b?.connected) return { ok: false, reason: 'slot 2 is not connected' };
  if (!a.state) return { ok: false, reason: 'slot 1 has no state yet' };
  if (!b.state) return { ok: false, reason: 'slot 2 has no state yet' };
  if (a.state.linkGroupId !== pair.groupId || a.state.linkSlot !== 1) return { ok: false, reason: 'slot 1 device-link mismatch' };
  if (b.state.linkGroupId !== pair.groupId || b.state.linkSlot !== 2) return { ok: false, reason: 'slot 2 device-link mismatch' };
  if (!a.state.armed) return { ok: false, reason: 'slot 1 is disarmed' };
  if (!b.state.armed) return { ok: false, reason: 'slot 2 is disarmed' };
  if (now - a.state.lastReceivedAt > staleMs) return { ok: false, reason: 'slot 1 state is stale' };
  if (now - b.state.lastReceivedAt > staleMs) return { ok: false, reason: 'slot 2 state is stale' };
  if (!slot1Active) return { ok: false, reason: 'slot 1 is not active' };
  if (!slot2Active) return { ok: false, reason: 'slot 2 is not active' };
  return { ok: true, reason: 'both slots active and fresh' };
}
