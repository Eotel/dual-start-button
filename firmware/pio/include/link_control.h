#pragma once
#include <stdint.h>
#include <stddef.h>

#include "protocol.h"

// Pure, hardware-free link/control state machine.
//
// evaluateControl() decodes a Control packet and decides the outcome of a
// LINK/UNLINK/SET_ARMED/IDENTIFY/FACTORY_RESET_LINK command against the
// current link state. It performs no I/O: no Preferences/NVS, no NimBLE, no
// DeviceAdapter, no Serial, no String. The caller (main.cpp) applies the
// returned side effects.
//
// The ControlOutcome intentionally captures enough to reproduce the firmware's
// original, BLE-observable behavior exactly: the ControlResult fields
// (ok/cmd/error_code/message), the resulting LinkState, whether link fields
// must be persisted, the published ButtonState type/aux, the status blink, and
// the precise order those effects are emitted in (the two notifying
// characteristics fire in different orders across commands).

namespace dsb {

struct LinkState {
  uint32_t group_id;
  uint8_t slot;
  uint32_t generation;
  bool armed;
};

// Order in which the caller must emit ControlResult (R), publishState (P), and
// blinkStatus (B). Persist + apply-state always happen first, before any of
// these. A blink_ms of 0 means "skip the blink".
enum class SideEffectOrder : uint8_t {
  ResultPublishBlink,  // R, P, B  — decode error, rejects, SET_ARMED, unknown
  ResultBlinkPublish,  // R, B, P  — LINK accept, IDENTIFY
  BlinkPublishResult,  // B, P, R  — UNLINK/FACTORY clear (via clearLink)
};

struct ControlOutcome {
  bool ok;                 // ControlResult "ok"
  uint8_t cmd;             // ControlResult "cmd" (0 when the packet is undecodable)
  const char* error_code;  // ControlResult "error"; nullptr when ok
  const char* message;     // ControlResult "message"
  LinkState state;         // link state after applying this command
  bool persist;            // save link fields to NVS + refresh DeviceInfo
  StateType publish_type;  // ButtonState type to publish
  uint16_t publish_aux;    // ButtonState aux (error code 1-7, hold_ms, or 0)
  uint8_t blink_r;
  uint8_t blink_g;
  uint8_t blink_b;
  uint32_t blink_ms;       // status blink duration; 0 = no blink
  SideEffectOrder order;
};

// All rejects share the orange error blink, Error publish type, and R,P,B
// ordering; only the aux code and blink duration vary.
inline ControlOutcome makeReject(const LinkState& current, uint8_t cmd, const char* code,
                                 const char* message, uint16_t aux, uint32_t blink_ms) {
  ControlOutcome o{};
  o.ok = false;
  o.cmd = cmd;
  o.error_code = code;
  o.message = message;
  o.state = current;
  o.persist = false;
  o.publish_type = StateType::Error;
  o.publish_aux = aux;
  o.blink_r = 64;
  o.blink_g = 32;
  o.blink_b = 0;
  o.blink_ms = blink_ms;
  o.order = SideEffectOrder::ResultPublishBlink;
  return o;
}

inline ControlOutcome evaluateControl(const uint8_t* data, size_t len, const LinkState& current) {
  ControlCommandV1 cmd{};
  if (!decodeControlCommand(data, len, cmd)) {
    return makeReject(current, 0, "invalid_packet",
                      "control command must be 12 bytes and version=1", 1, 1200);
  }

  const bool force = (cmd.flags & ControlFlag0) != 0;
  const uint8_t commandValue = cmd.command;

  ControlOutcome o{};
  o.cmd = commandValue;
  o.state = current;

  switch (static_cast<ControlCommand>(cmd.command)) {
    case ControlCommand::Link: {
      if (cmd.group_id == 0) {
        return makeReject(current, commandValue, "invalid_group",
                          "LINK requires nonzero group_id", 2, 1200);
      }
      if (cmd.slot != 1 && cmd.slot != 2) {
        return makeReject(current, commandValue, "invalid_slot",
                          "LINK requires slot 1 or 2", 3, 1200);
      }
      if (current.group_id != 0 && current.group_id != cmd.group_id && !force) {
        return makeReject(current, commandValue, "link_conflict",
                          "already linked to another group; use force", 4, 1500);
      }
      o.ok = true;
      o.error_code = nullptr;
      o.message = "linked";
      o.state.group_id = cmd.group_id;
      o.state.slot = cmd.slot;
      o.state.generation = current.generation + 1;
      o.persist = true;
      o.publish_type = StateType::Link;
      o.publish_aux = 0;
      o.blink_r = 0;
      o.blink_g = 64;
      o.blink_b = 0;
      o.blink_ms = 900;
      o.order = SideEffectOrder::ResultBlinkPublish;
      return o;
    }

    case ControlCommand::Unlink: {
      if (current.group_id == 0) {
        o.ok = true;
        o.error_code = nullptr;
        o.message = "already unlinked";
        o.persist = false;
        o.publish_type = StateType::Link;
        o.publish_aux = 0;
        o.blink_ms = 0;  // no blink
        o.order = SideEffectOrder::ResultPublishBlink;  // R, P (blink skipped)
        return o;
      }
      if (cmd.group_id != 0 && cmd.group_id != current.group_id && !force) {
        return makeReject(current, commandValue, "link_conflict",
                          "linked to different group; use force", 5, 1500);
      }
      // clearLink(): reset link fields, bump generation, persist, blue-cyan blink.
      o.ok = true;
      o.error_code = nullptr;
      o.message = "unlinked";
      o.state.group_id = 0;
      o.state.slot = 0;
      o.state.generation = current.generation + 1;
      o.persist = true;
      o.publish_type = StateType::Link;
      o.publish_aux = 0;
      o.blink_r = 0;
      o.blink_g = 64;
      o.blink_b = 64;
      o.blink_ms = 1200;
      o.order = SideEffectOrder::BlinkPublishResult;
      return o;
    }

    case ControlCommand::SetArmed: {
      o.ok = true;
      o.error_code = nullptr;
      o.state.armed = force;  // flags bit0 = armed
      o.message = o.state.armed ? "armed" : "disarmed";
      o.persist = false;
      o.publish_type = StateType::State;
      o.publish_aux = 0;
      o.blink_r = o.state.armed ? 0 : 64;
      o.blink_g = o.state.armed ? 64 : 32;
      o.blink_b = 0;
      o.blink_ms = 700;
      o.order = SideEffectOrder::ResultPublishBlink;
      return o;
    }

    case ControlCommand::Identify: {
      uint32_t duration = cmd.value;
      if (duration == 0 || duration > 30000) duration = 3000;
      o.ok = true;
      o.error_code = nullptr;
      o.message = "identify";
      o.persist = false;
      o.publish_type = StateType::State;
      o.publish_aux = 0;
      o.blink_r = 64;
      o.blink_g = 0;
      o.blink_b = 64;
      o.blink_ms = duration;
      o.order = SideEffectOrder::ResultBlinkPublish;
      return o;
    }

    case ControlCommand::FactoryResetLink: {
      if (!force) {
        return makeReject(current, commandValue, "force_required",
                          "FACTORY_RESET_LINK requires force flag", 6, 1500);
      }
      // clearLink(): same clear + blink as UNLINK, different message.
      o.ok = true;
      o.error_code = nullptr;
      o.message = "factory reset link done";
      o.state.group_id = 0;
      o.state.slot = 0;
      o.state.generation = current.generation + 1;
      o.persist = true;
      o.publish_type = StateType::Link;
      o.publish_aux = 0;
      o.blink_r = 0;
      o.blink_g = 64;
      o.blink_b = 64;
      o.blink_ms = 1200;
      o.order = SideEffectOrder::BlinkPublishResult;
      return o;
    }

    default:
      return makeReject(current, commandValue, "unknown_command",
                        "unsupported control command", 7, 1200);
  }
}

}  // namespace dsb
