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

  return {
    // Resolves with the next ControlResult whose cmd matches; rejects when no
    // result arrives within timeoutMs (lost notification or disconnect).
    expect(cmd) {
      return new Promise((resolve, reject) => {
        const entry = { cmd, resolve, reject, timer: null };
        entry.timer = setTimeoutFn(() => {
          const index = pending.indexOf(entry);
          if (index !== -1) pending.splice(index, 1);
          reject(new Error(`no ControlResult for cmd=${cmd} within ${timeoutMs}ms`));
        }, timeoutMs);
        pending.push(entry);
      });
    },

    // Routes a parsed ControlResult to the oldest matching expectation.
    // Returns false for results nobody is waiting for (late, unsolicited, or
    // missing cmd), which callers may still log.
    deliver(result) {
      if (typeof result?.cmd !== 'number') return false;
      const index = pending.findIndex((entry) => entry.cmd === result.cmd);
      if (index === -1) return false;
      const [entry] = pending.splice(index, 1);
      clearTimeoutFn(entry.timer);
      entry.resolve(result);
      return true;
    },
  };
}

// Human-readable rejection reason for the log pane. link_conflict is the one
// error with a documented recovery path (SPEC section 11.3: force link), so it
// carries an explicit hint.
export function describeControlError(result) {
  const code = result?.error || 'unknown_error';
  const message = result?.message || '';
  const base = message ? `${code}: ${message}` : code;
  if (code === 'link_conflict') {
    return `${base} — enable "Force link" to overwrite the existing link`;
  }
  return base;
}
