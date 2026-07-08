#pragma once
#include <stdint.h>

#include "protocol.h"

// Pure, hardware-free ButtonState packet assembly.
//
// main.cpp owns the clocks (millis), the BLE characteristic, and the seq
// counter; everything that decides packet *content* lives here so the native
// tests can pin it without a device.

namespace dsb {

// hold time reported in the 16-bit aux field saturates instead of wrapping,
// so a >65s hold reads as 65535 rather than a small bogus value.
inline uint16_t saturatingHoldMs(uint32_t now_ms, uint32_t press_started_at_ms) {
  const uint32_t hold = now_ms - press_started_at_ms;
  return static_cast<uint16_t>(hold > 65535 ? 65535 : hold);
}

// The FlagLongPressed wire bit and the long-hold link reset share this
// boundary. Callers combine it with their own pressed/latch checks.
inline bool longHoldElapsed(uint32_t now_ms, uint32_t press_started_at_ms, uint32_t threshold_ms) {
  return now_ms - press_started_at_ms >= threshold_ms;
}

// Snapshot of the runtime fields that feed a ButtonState packet.
struct RuntimeState {
  bool pressed;
  bool armed;
  bool connected;
  bool long_pressed;
  uint8_t link_slot;
  uint32_t link_group_id;
  uint32_t device_hash;
};

// SPEC section 8 flags byte. FlagError is derived from the packet type, not
// stored runtime state: every type=error packet carries bit5.
inline uint8_t composeStateFlags(const RuntimeState& rt, StateType type) {
  uint8_t flags = 0;
  if (rt.pressed) flags |= FlagPressed;
  if (rt.armed) flags |= FlagArmed;
  if (rt.link_group_id != 0 && rt.link_slot != 0) flags |= FlagLinked;
  if (rt.long_pressed) flags |= FlagLongPressed;
  if (rt.connected) flags |= FlagConnected;
  if (type == StateType::Error) flags |= FlagError;
  return flags;
}

// seq is a 16-bit monotonic counter that wraps 65535 -> 0 (SPEC section 8).
inline uint16_t nextSeq(uint16_t seq) {
  return static_cast<uint16_t>(seq + 1);
}

inline ButtonStateV1 makeButtonStateFields(const RuntimeState& rt, StateType type,
                                           uint16_t seq, uint32_t uptime_ms, uint16_t aux) {
  ButtonStateV1 s{};
  s.version = PROTOCOL_VERSION;
  s.type = static_cast<uint8_t>(type);
  s.flags = composeStateFlags(rt, type);
  s.link_slot = rt.link_slot;
  s.seq = seq;
  s.uptime_ms = uptime_ms;
  s.device_hash = rt.device_hash;
  s.link_group_id = rt.link_group_id;
  s.aux = aux;
  return s;
}

}  // namespace dsb
