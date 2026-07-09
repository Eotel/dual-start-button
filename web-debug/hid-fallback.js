import { reduceActive, resetActive } from './active-state.js';

export const HID_STORAGE_KEY = 'dsb.hidFallback.v1';
export const HID_RELEASE_TIMEOUT_MS = 1000;

const SLOT_IDS = [1, 2];

function emptySlot(assignment = null) {
  return {
    assignment: normalizeAssignment(assignment),
    pressed: false,
    active: resetActive(false),
    lastDownAt: null,
    releaseDeadlineAt: null,
  };
}

function cloneAssignment(assignment) {
  return assignment ? { id: assignment.id, label: assignment.label } : null;
}

function cloneSlot(slot) {
  return {
    assignment: cloneAssignment(slot.assignment),
    pressed: slot.pressed,
    active: { ...slot.active },
    lastDownAt: slot.lastDownAt,
    releaseDeadlineAt: slot.releaseDeadlineAt,
  };
}

function cloneState(state) {
  return {
    captureSlot: state.captureSlot,
    focusOk: state.focusOk,
    lastObserved: state.lastObserved ? { ...state.lastObserved } : null,
    error: state.error,
    slots: {
      1: cloneSlot(state.slots[1]),
      2: cloneSlot(state.slots[2]),
    },
  };
}

function normalizeAssignment(value) {
  if (!value || typeof value !== 'object') return null;
  if (typeof value.id !== 'string' || !value.id) return null;
  const label = typeof value.label === 'string' && value.label ? value.label : value.id;
  return { id: value.id, label };
}

function assignmentFromInput(assignments, slot) {
  return normalizeAssignment(assignments?.[`slot${slot}`] ?? assignments?.[slot] ?? null);
}

function slotNumber(slot) {
  const n = Number(slot);
  if (n !== 1 && n !== 2) throw new RangeError(`invalid HID slot: ${slot}`);
  return n;
}

function findAssignedSlot(state, identityId, exceptSlot = null) {
  for (const slot of SLOT_IDS) {
    if (slot === exceptSlot) continue;
    if (state.slots[slot].assignment?.id === identityId) return slot;
  }
  return null;
}

function matchingSlot(state, identityId) {
  return findAssignedSlot(state, identityId, null);
}

function setSlot(state, slot, slotState) {
  const next = cloneState(state);
  next.slots[slot] = slotState;
  return next;
}

function observed(kind, identity, event, now) {
  return {
    kind,
    id: identity.id,
    label: identity.label,
    key: event.key || '',
    code: event.code || '',
    repeat: Boolean(event.repeat),
    at: now,
  };
}

export function createHidFallbackState(assignments = {}) {
  return {
    captureSlot: null,
    focusOk: true,
    lastObserved: null,
    error: null,
    slots: {
      1: emptySlot(assignmentFromInput(assignments, 1)),
      2: emptySlot(assignmentFromInput(assignments, 2)),
    },
  };
}

export function hidIdentityFromEvent(event) {
  const code = typeof event?.code === 'string' ? event.code.trim() : '';
  const key = typeof event?.key === 'string' ? event.key.trim() : '';
  if (code && code !== 'Unidentified') return { id: `code:${code}`, label: code };
  if (key && key !== 'Unidentified') return { id: `key:${key}`, label: key };
  return null;
}

export function startHidCapture(state, slot) {
  const next = cloneState(state);
  next.captureSlot = slotNumber(slot);
  next.error = null;
  return next;
}

export function clearHidSlot(state, slot) {
  const n = slotNumber(slot);
  const next = setSlot(state, n, emptySlot());
  if (next.captureSlot === n) next.captureSlot = null;
  next.error = null;
  return next;
}

export function applyHidKeyDown(state, event, now = Date.now()) {
  const identity = hidIdentityFromEvent(event);
  if (!identity) {
    const next = cloneState(state);
    next.error = 'Ignored keyboard event with no HID key identity';
    return next;
  }

  const next = cloneState(state);
  next.lastObserved = observed('down', identity, event, now);

  if (next.captureSlot) {
    const slot = next.captureSlot;
    const duplicateSlot = findAssignedSlot(next, identity.id, slot);
    if (duplicateSlot) {
      next.error = `${identity.label} is already assigned to HID slot ${duplicateSlot}`;
      return next;
    }

    next.slots[slot] = {
      assignment: identity,
      pressed: true,
      active: resetActive(true),
      lastDownAt: now,
      releaseDeadlineAt: now + HID_RELEASE_TIMEOUT_MS,
    };
    next.captureSlot = null;
    next.error = null;
    return next;
  }

  const slot = matchingSlot(next, identity.id);
  if (!slot) {
    next.error = null;
    return next;
  }

  const current = next.slots[slot];
  const pressed = true;
  const shouldToggle = !event.repeat && !current.pressed;
  next.slots[slot] = {
    ...current,
    pressed,
    active: shouldToggle ? reduceActive(current.active, pressed) : current.active,
    lastDownAt: current.pressed ? current.lastDownAt : now,
    releaseDeadlineAt: now + HID_RELEASE_TIMEOUT_MS,
  };
  next.error = null;
  return next;
}

export function applyHidKeyUp(state, event, now = Date.now()) {
  const identity = hidIdentityFromEvent(event);
  if (!identity) return state;

  const next = cloneState(state);
  next.lastObserved = observed('up', identity, event, now);

  const slot = matchingSlot(next, identity.id);
  if (!slot) return next;

  const current = next.slots[slot];
  next.slots[slot] = {
    ...current,
    pressed: false,
    active: reduceActive(current.active, false),
    releaseDeadlineAt: null,
  };
  return next;
}

export function expireHidReleases(state, now = Date.now()) {
  let next = null;
  for (const slot of SLOT_IDS) {
    const source = next || state;
    const current = source.slots[slot];
    if (!current.pressed || current.releaseDeadlineAt == null || now <= current.releaseDeadlineAt)
      continue;
    next = next || cloneState(state);
    next.slots[slot] = {
      ...current,
      pressed: false,
      active: reduceActive(current.active, false),
      releaseDeadlineAt: null,
    };
  }
  return next || state;
}

export function resetHidActive(state) {
  const next = cloneState(state);
  for (const slot of SLOT_IDS) {
    next.slots[slot] = {
      ...next.slots[slot],
      active: resetActive(next.slots[slot].pressed),
    };
  }
  next.error = null;
  return next;
}

export function setHidFocus(state, focusOk) {
  const nextFocus = Boolean(focusOk);
  if (state.focusOk === nextFocus && nextFocus) return state;
  const next = cloneState(state);
  next.focusOk = nextFocus;
  if (!next.focusOk) {
    for (const slot of SLOT_IDS) {
      const current = next.slots[slot];
      if (!current.pressed) continue;
      next.slots[slot] = {
        ...current,
        pressed: false,
        active: reduceActive(current.active, false),
        releaseDeadlineAt: null,
      };
    }
  }
  return next;
}

export function evaluateHidStartCondition(state) {
  if (!state.slots[1].assignment) return { ok: false, reason: 'HID slot 1 is not assigned' };
  if (!state.slots[2].assignment) return { ok: false, reason: 'HID slot 2 is not assigned' };
  if (!state.focusOk) return { ok: false, reason: 'HID fallback page is not focused' };
  if (!state.slots[1].active.active) return { ok: false, reason: 'HID slot 1 is not active' };
  if (!state.slots[2].active.active) return { ok: false, reason: 'HID slot 2 is not active' };
  return { ok: true, reason: 'both HID fallback slots active' };
}

export function serializeHidAssignments(state) {
  return JSON.stringify({
    v: 1,
    slot1: cloneAssignment(state.slots[1].assignment),
    slot2: cloneAssignment(state.slots[2].assignment),
  });
}

export function parseHidAssignments(raw) {
  if (!raw) return {};
  try {
    const parsed = typeof raw === 'string' ? JSON.parse(raw) : raw;
    return {
      slot1: normalizeAssignment(parsed.slot1),
      slot2: normalizeAssignment(parsed.slot2),
    };
  } catch (_) {
    return {};
  }
}
