// Host-derived per-slot Active state (SPEC.md section 12).
// Active is not on the wire: the host toggles it on each observed press
// down-edge (pressed false -> true). The first observed sample after
// (re)connect, relink, or reset is a baseline and never toggles, so a button
// already held down cannot flip the slot. Reset (disconnect, unlink, relink,
// new group, manual reset) is assigning INITIAL_ACTIVE at the call site.

export const INITIAL_ACTIVE = Object.freeze({ active: false, prevPressed: null });

export function reduceActive(tracker, pressed) {
  const downEdge = tracker.prevPressed === false && pressed === true;
  return { active: downEdge ? !tracker.active : tracker.active, prevPressed: pressed };
}
