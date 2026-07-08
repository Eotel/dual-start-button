// Shared test helper: hand-crafts a 12-byte Control packet. This is the sole
// encoder of the Control wire layout (production firmware only decodes), so
// the test suites share one copy instead of drifting.
#pragma once
#include <stdint.h>

#include "protocol.h"

inline void craft(uint8_t out[dsb::CONTROL_COMMAND_SIZE], uint8_t command, uint8_t slot,
                  uint8_t flags, uint32_t group_id, uint32_t value) {
  out[0] = dsb::PROTOCOL_VERSION;
  out[1] = command;
  out[2] = slot;
  out[3] = flags;
  dsb::writeLe32(out + 4, group_id);
  dsb::writeLe32(out + 8, value);
}
