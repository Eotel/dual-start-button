// Native (host) Unity tests for the wire protocol in include/protocol.h.
//
// Vectors come from the shared JSON contract in ../../test-vectors via the
// generated header below (see generate_test_vectors.py). Nothing here
// hand-copies a vector value: the same bytes drive the Python, JS, and C++
// suites, so a divergence in any implementation shows up as a failure.

#include <string.h>
#include <unity.h>

#include "../generated/test_vectors.h"
#include "protocol.h"

using namespace dsb;

void setUp() {}
void tearDown() {}

// Every valid ButtonState vector must encode to exactly its golden bytes.
void test_button_state_encode_matches_vectors() {
  for (size_t i = 0; i < kDsbButtonStateValidCount; ++i) {
    const DsbButtonStateVector& v = kDsbButtonStateValid[i];
    ButtonStateV1 s{};
    s.version = v.version;
    s.type = v.type;
    s.flags = v.flags;
    s.link_slot = v.link_slot;
    s.seq = v.seq;
    s.uptime_ms = v.uptime_ms;
    s.device_hash = v.device_hash;
    s.link_group_id = v.link_group_id;
    s.aux = v.aux;

    uint8_t out[BUTTON_STATE_SIZE];
    encodeButtonState(s, out);
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(v.bytes, out, BUTTON_STATE_SIZE, v.name);
  }
}

// Every valid Control vector must decode back to its expected fields.
void test_control_decode_matches_vectors() {
  for (size_t i = 0; i < kDsbControlValidCount; ++i) {
    const DsbControlVector& v = kDsbControlValid[i];
    ControlCommandV1 c{};
    const bool ok = decodeControlCommand(v.bytes, CONTROL_COMMAND_SIZE, c);
    TEST_ASSERT_TRUE_MESSAGE(ok, v.name);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(v.version, c.version, v.name);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(v.command, c.command, v.name);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(v.slot, c.slot, v.name);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(v.flags, c.flags, v.name);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(v.group_id, c.group_id, v.name);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(v.value, c.value, v.name);
  }
}

// Malformed Control packets (wrong length, version != 1) must be rejected.
void test_control_decode_rejects_invalid_vectors() {
  for (size_t i = 0; i < kDsbControlInvalidCount; ++i) {
    const DsbRawVector& v = kDsbControlInvalid[i];
    ControlCommandV1 c{};
    const bool ok = decodeControlCommand(v.bytes, v.len, c);
    TEST_ASSERT_FALSE_MESSAGE(ok, v.name);
  }
}

// FNV-1a 32-bit against values computed independently (in Python).
void test_fnv1a32_known_values() {
  TEST_ASSERT_EQUAL_UINT32(2166136261u, fnv1a32(""));             // offset basis
  TEST_ASSERT_EQUAL_UINT32(663773663u, fnv1a32("A1B2C3D4E5F6"));  // SPEC sample id
  TEST_ASSERT_EQUAL_UINT32(98073254u, fnv1a32("DSB"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_button_state_encode_matches_vectors);
  RUN_TEST(test_control_decode_matches_vectors);
  RUN_TEST(test_control_decode_rejects_invalid_vectors);
  RUN_TEST(test_fnv1a32_known_values);
  return UNITY_END();
}
