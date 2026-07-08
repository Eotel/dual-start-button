// Native (host) Unity tests for the pure ButtonState packet assembly in
// include/state_packet.h. These pin the flag composition (including the
// FlagError bit for type=error packets), the full field mapping main.cpp's
// publishState must reproduce, seq wraparound, and hold_ms saturation.

#include <unity.h>

#include "state_packet.h"

using namespace dsb;

void setUp() {}
void tearDown() {}

// --- saturatingHoldMs --------------------------------------------------------

void test_hold_ms_below_cap_passes_through() {
  TEST_ASSERT_EQUAL_UINT16(0, saturatingHoldMs(1000, 1000));
  TEST_ASSERT_EQUAL_UINT16(450, saturatingHoldMs(1450, 1000));
  TEST_ASSERT_EQUAL_UINT16(65534, saturatingHoldMs(66534, 1000));
}

void test_hold_ms_at_cap_is_65535() {
  TEST_ASSERT_EQUAL_UINT16(65535, saturatingHoldMs(66535, 1000));
}

void test_hold_ms_above_cap_saturates_to_65535() {
  TEST_ASSERT_EQUAL_UINT16(65535, saturatingHoldMs(66536, 1000));
  TEST_ASSERT_EQUAL_UINT16(65535, saturatingHoldMs(10000000, 0));
}

void test_heartbeat_aux_saturates_live_hold_and_keeps_last_hold() {
  TEST_ASSERT_EQUAL_UINT16(450, heartbeatAux(true, 1450, 1000, 123));    // pressed: live hold
  TEST_ASSERT_EQUAL_UINT16(65535, heartbeatAux(true, 70001, 1000, 0));   // pressed: saturated
  TEST_ASSERT_EQUAL_UINT16(123, heartbeatAux(false, 99999, 1000, 123));  // released: last hold
}

// --- longHoldElapsed ---------------------------------------------------------

void test_long_hold_boundary_at_threshold() {
  TEST_ASSERT_FALSE(longHoldElapsed(10999, 1000, 10000));
  TEST_ASSERT_TRUE(longHoldElapsed(11000, 1000, 10000));
  TEST_ASSERT_TRUE(longHoldElapsed(11001, 1000, 10000));
}

// --- composeStateFlags -------------------------------------------------------

static RuntimeState idle() {
  return RuntimeState{/*pressed=*/false, /*armed=*/false, /*connected=*/false,
                      /*long_pressed=*/false, /*link_slot=*/0, /*link_group_id=*/0,
                      /*device_hash=*/0};
}

void test_flags_each_bit_maps_to_its_runtime_field() {
  TEST_ASSERT_EQUAL_UINT8(0, composeStateFlags(idle(), StateType::State));

  RuntimeState rt = idle();
  rt.pressed = true;
  TEST_ASSERT_EQUAL_UINT8(FlagPressed, composeStateFlags(rt, StateType::State));

  rt = idle();
  rt.armed = true;
  TEST_ASSERT_EQUAL_UINT8(FlagArmed, composeStateFlags(rt, StateType::State));

  rt = idle();
  rt.long_pressed = true;
  TEST_ASSERT_EQUAL_UINT8(FlagLongPressed, composeStateFlags(rt, StateType::State));

  rt = idle();
  rt.connected = true;
  TEST_ASSERT_EQUAL_UINT8(FlagConnected, composeStateFlags(rt, StateType::State));
}

void test_flags_linked_requires_both_group_and_slot() {
  RuntimeState rt = idle();
  rt.link_group_id = 777;
  rt.link_slot = 1;
  TEST_ASSERT_EQUAL_UINT8(FlagLinked, composeStateFlags(rt, StateType::State));

  rt.link_slot = 0;
  TEST_ASSERT_EQUAL_UINT8(0, composeStateFlags(rt, StateType::State));

  rt.link_slot = 2;
  rt.link_group_id = 0;
  TEST_ASSERT_EQUAL_UINT8(0, composeStateFlags(rt, StateType::State));
}

void test_flags_error_bit_composed_only_for_error_type() {
  RuntimeState rt = idle();
  rt.armed = true;
  rt.connected = true;
  TEST_ASSERT_EQUAL_UINT8(FlagArmed | FlagConnected | FlagError,
                          composeStateFlags(rt, StateType::Error));
  TEST_ASSERT_EQUAL_UINT8(FlagArmed | FlagConnected, composeStateFlags(rt, StateType::State));
  TEST_ASSERT_EQUAL_UINT8(FlagArmed | FlagConnected, composeStateFlags(rt, StateType::Heartbeat));
  TEST_ASSERT_EQUAL_UINT8(FlagArmed | FlagConnected, composeStateFlags(rt, StateType::Boot));
  TEST_ASSERT_EQUAL_UINT8(FlagArmed | FlagConnected, composeStateFlags(rt, StateType::Link));
}

// --- makeButtonStateFields ---------------------------------------------------

void test_packet_fields_map_runtime_and_arguments() {
  RuntimeState rt = idle();
  rt.pressed = true;
  rt.armed = true;
  rt.connected = true;
  rt.link_slot = 2;
  rt.link_group_id = 0xabcdef;
  rt.device_hash = 0x12345678;

  const ButtonStateV1 s = makeButtonStateFields(rt, StateType::Heartbeat,
                                                /*seq=*/42, /*uptime_ms=*/10000, /*aux=*/278);
  TEST_ASSERT_EQUAL_UINT8(PROTOCOL_VERSION, s.version);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)StateType::Heartbeat, s.type);
  TEST_ASSERT_EQUAL_UINT8(FlagPressed | FlagArmed | FlagLinked | FlagConnected, s.flags);
  TEST_ASSERT_EQUAL_UINT8(2, s.link_slot);
  TEST_ASSERT_EQUAL_UINT16(42, s.seq);
  TEST_ASSERT_EQUAL_UINT32(10000, s.uptime_ms);
  TEST_ASSERT_EQUAL_UINT32(0x12345678, s.device_hash);
  TEST_ASSERT_EQUAL_UINT32(0xabcdef, s.link_group_id);
  TEST_ASSERT_EQUAL_UINT16(278, s.aux);
}

void test_error_packet_carries_flag_error_bit5_on_the_wire() {
  RuntimeState rt = idle();
  rt.armed = true;
  rt.connected = true;

  const ButtonStateV1 s = makeButtonStateFields(rt, StateType::Error,
                                                /*seq=*/7, /*uptime_ms=*/500, /*aux=*/4);
  uint8_t packet[BUTTON_STATE_SIZE];
  encodeButtonState(s, packet);
  TEST_ASSERT_EQUAL_UINT8(5, packet[1]);                       // type=error
  TEST_ASSERT_NOT_EQUAL(0, packet[2] & FlagError);             // bit5 set
  TEST_ASSERT_EQUAL_UINT8(FlagArmed | FlagConnected | FlagError, packet[2]);
  TEST_ASSERT_EQUAL_UINT8(4, packet[18]);                      // aux lo (error code)
  TEST_ASSERT_EQUAL_UINT8(0, packet[19]);                      // aux hi
}

// --- nextSeq -----------------------------------------------------------------

void test_seq_increments_and_wraps_65535_to_0() {
  TEST_ASSERT_EQUAL_UINT16(1, nextSeq(0));
  TEST_ASSERT_EQUAL_UINT16(65535, nextSeq(65534));
  TEST_ASSERT_EQUAL_UINT16(0, nextSeq(65535));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_hold_ms_below_cap_passes_through);
  RUN_TEST(test_hold_ms_at_cap_is_65535);
  RUN_TEST(test_hold_ms_above_cap_saturates_to_65535);
  RUN_TEST(test_heartbeat_aux_saturates_live_hold_and_keeps_last_hold);
  RUN_TEST(test_long_hold_boundary_at_threshold);
  RUN_TEST(test_flags_each_bit_maps_to_its_runtime_field);
  RUN_TEST(test_flags_linked_requires_both_group_and_slot);
  RUN_TEST(test_flags_error_bit_composed_only_for_error_type);
  RUN_TEST(test_packet_fields_map_runtime_and_arguments);
  RUN_TEST(test_error_packet_carries_flag_error_bit5_on_the_wire);
  RUN_TEST(test_seq_increments_and_wraps_65535_to_0);
  return UNITY_END();
}
