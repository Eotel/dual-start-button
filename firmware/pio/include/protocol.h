#pragma once
#include <stddef.h>
#include <stdint.h>

namespace dsb {

static constexpr uint8_t PROTOCOL_VERSION = 1;
static constexpr size_t BUTTON_STATE_SIZE = 20;
static constexpr size_t CONTROL_COMMAND_SIZE = 12;

enum class StateType : uint8_t {
  State = 1,
  Heartbeat = 2,
  Boot = 3,
  Link = 4,
  Error = 5,
};

enum StateFlags : uint8_t {
  FlagPressed = 1 << 0,
  FlagArmed = 1 << 1,
  FlagLinked = 1 << 2,
  FlagLongPressed = 1 << 3,
  FlagConnected = 1 << 4,
  FlagError = 1 << 5,
};

enum class ControlCommand : uint8_t {
  Link = 1,
  Unlink = 2,
  SetArmed = 3,
  Identify = 4,
  FactoryResetLink = 5,
};

enum ControlFlags : uint8_t {
  ControlFlag0 = 1 << 0,  // LINK/UNLINK: force, SET_ARMED: armed
};

struct ButtonStateV1 {
  uint8_t version;
  uint8_t type;
  uint8_t flags;
  uint8_t link_slot;
  uint16_t seq;
  uint32_t uptime_ms;
  uint32_t device_hash;
  uint32_t link_group_id;
  uint16_t aux;
};

struct ControlCommandV1 {
  uint8_t version;
  uint8_t command;
  uint8_t slot;
  uint8_t flags;
  uint32_t group_id;
  uint32_t value;
};

inline uint16_t readLe16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

inline uint32_t readLe32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

inline void writeLe16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v & 0xff);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xff);
}

inline void writeLe32(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v & 0xff);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xff);
  p[2] = static_cast<uint8_t>((v >> 16) & 0xff);
  p[3] = static_cast<uint8_t>((v >> 24) & 0xff);
}

inline void encodeButtonState(const ButtonStateV1& s, uint8_t out[BUTTON_STATE_SIZE]) {
  out[0] = s.version;
  out[1] = s.type;
  out[2] = s.flags;
  out[3] = s.link_slot;
  writeLe16(out + 4, s.seq);
  writeLe32(out + 6, s.uptime_ms);
  writeLe32(out + 10, s.device_hash);
  writeLe32(out + 14, s.link_group_id);
  writeLe16(out + 18, s.aux);
}

inline bool decodeControlCommand(const uint8_t* data, size_t len, ControlCommandV1& out) {
  if (len != CONTROL_COMMAND_SIZE) return false;
  out.version = data[0];
  out.command = data[1];
  out.slot = data[2];
  out.flags = data[3];
  out.group_id = readLe32(data + 4);
  out.value = readLe32(data + 8);
  return out.version == PROTOCOL_VERSION;
}

inline uint32_t fnv1a32(const char* value) {
  uint32_t hash = 2166136261UL;
  for (; *value != '\0'; ++value) {
    hash ^= static_cast<uint8_t>(*value);
    hash *= 16777619UL;
  }
  return hash;
}

}  // namespace dsb
