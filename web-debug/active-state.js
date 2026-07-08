// Host-derived per-slot Active state (SPEC.md section 12).
// Active is not on the wire: the host toggles it on each observed press
// down-edge (pressed false -> true). With an unknown baseline (prevPressed
// null) the first observed sample never toggles, so a button already held
// down at (re)connect cannot flip the slot. Reset (disconnect, unlink,
// relink, new group, manual reset) assigns resetActive(...) at the call
// site, seeding the baseline from the slot's last known pressed value when
// one is available so the very next press still toggles.

export function resetActive(prevPressed = null) {
  return { active: false, prevPressed };
}

export const INITIAL_ACTIVE = Object.freeze(resetActive());

export function reduceActive(tracker, pressed) {
  const downEdge = tracker.prevPressed === false && pressed === true;
  return { active: downEdge ? !tracker.active : tracker.active, prevPressed: pressed };
}
