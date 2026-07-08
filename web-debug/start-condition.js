// Host-side start-condition evaluation (SPEC.md section 12).
// Pure function: the host decides whether both linked slots satisfy the
// connected / armed / fresh / pressed / confirm-hold condition. Firmware only
// reports state. `slot1` and `slot2` are entry-like objects ({ connected,
// state } or null); `now`, `staleMs`, and `confirmHoldMs` are injected so the
// decision is deterministic and testable.

export function evaluateStartCondition({ pair, slot1, slot2, now, staleMs, confirmHoldMs }) {
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
  if (!a.state.pressed) return { ok: false, reason: 'slot 1 is not pressed' };
  if (!b.state.pressed) return { ok: false, reason: 'slot 2 is not pressed' };
  const since = Math.max(a.state.pressedSince || now, b.state.pressedSince || now);
  const held = now - since;
  if (held < confirmHoldMs) return { ok: false, reason: `both pressed, confirming ${held}/${confirmHoldMs}ms` };
  return { ok: true, reason: `both buttons pressed and fresh; held ${held}ms` };
}
