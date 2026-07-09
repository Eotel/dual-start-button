// Correlates Control writes with ControlResult notifications (SPEC section 10).
// ControlResult arrives asynchronously via Notify, so a successful GATT write
// only means the command was delivered, not accepted. The app must register an
// expectation before writing, then act on the matched result's `ok` field.
// Timer functions are injected so node tests can drive the timeout branch.

export const CONTROL_RESULT_TIMEOUT_MS = 3000;

export function createResultCorrelator({
  timeoutMs = CONTROL_RESULT_TIMEOUT_MS,
  setTimeoutFn = (fn, ms) => setTimeout(fn, ms),
  clearTimeoutFn = (id) => clearTimeout(id),
} = {}) {
  const pending = [];

  const remove = (entry) => {
    const index = pending.indexOf(entry);
    if (index === -1) return false;
    pending.splice(index, 1);
    return true;
  };

  return {
    // Registers an expectation for the next ControlResult with this cmd.
    // `result` resolves with the matched payload or rejects on timeout (lost
    // notification or disconnect). `cancel()` frees the slot — call it when
    // the GATT write itself fails, so a later retry of the same cmd cannot be
    // satisfied by this dead attempt.
    expect(cmd) {
      const entry = { cmd };
      entry.result = new Promise((resolve, reject) => {
        entry.resolve = resolve;
        entry.reject = reject;
      });
      entry.timer = setTimeoutFn(() => {
        if (remove(entry)) {
          entry.reject(new Error(`no ControlResult for cmd=${cmd} within ${timeoutMs}ms`));
        }
      }, timeoutMs);
      pending.push(entry);
      return {
        result: entry.result,
        cancel() {
          if (!remove(entry)) return; // already delivered or timed out
          clearTimeoutFn(entry.timer);
          entry.reject(new Error(`ControlResult wait for cmd=${cmd} cancelled`));
        },
      };
    },

    // Routes a parsed ControlResult to the oldest matching expectation.
    // Returns false for results nobody is waiting for (late, unsolicited, or
    // missing cmd), which callers may still log.
    deliver(result) {
      if (typeof result?.cmd !== 'number') return false;
      const entry = pending.find((p) => p.cmd === result.cmd);
      if (!entry) return false;
      remove(entry);
      clearTimeoutFn(entry.timer);
      entry.resolve(result);
      return true;
    },
  };
}

// Correlation is by cmd only, so an ok:true LINK result that arrives after its
// own attempt timed out could satisfy a later retry aimed at a different
// slot/group. Accept a LINK result only when it reflects the requested link.
export function confirmsLink(result, { groupId, slot }) {
  return result?.ok === true && result.link_group_id === groupId && result.link_slot === slot;
}

// Human-readable rejection reason for the log pane. link_conflict is the one
// error with a documented recovery path (SPEC section 11.3: force link), so it
// carries an explicit hint.
export function describeControlError(result) {
  const code = result?.error || 'unknown_error';
  const message = result?.message || '';
  const base = message ? `${code}: ${message}` : code;
  if (code === 'link_conflict') {
    return `${base} — enable "force link/unlink" to overwrite the existing link`;
  }
  return base;
}
