#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace dsb {

static constexpr uint8_t HID_KEYBOARD_REPORT_ID = 1;
static constexpr size_t HID_KEYBOARD_REPORT_SIZE = 8;

struct HidKeyMapping {
  uint8_t usage;
  const char* code;
};

inline HidKeyMapping hidKeyForDeviceHash(uint32_t device_hash) {
  static constexpr HidKeyMapping kKeys[] = {
      {0x68, "F13"}, {0x69, "F14"}, {0x6a, "F15"}, {0x6b, "F16"}, {0x6c, "F17"}, {0x6d, "F18"},
      {0x6e, "F19"}, {0x6f, "F20"}, {0x70, "F21"}, {0x71, "F22"}, {0x72, "F23"}, {0x73, "F24"},
  };
  return kKeys[device_hash % (sizeof(kKeys) / sizeof(kKeys[0]))];
}

inline void makeHidKeyboardReport(uint8_t usage, bool pressed, uint8_t out[HID_KEYBOARD_REPORT_SIZE]) {
  memset(out, 0, HID_KEYBOARD_REPORT_SIZE);
  if (pressed) {
    out[2] = usage;
  }
}

}  // namespace dsb
