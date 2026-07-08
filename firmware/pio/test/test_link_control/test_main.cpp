// Native (host) Unity tests for the pure link/control state machine in
// include/link_control.h. These pin the accept/reject decisions, resulting
// LinkState, persistence, published ButtonState type/aux, and side-effect
// ordering that main.cpp's handleControlBytes must reproduce byte-for-byte.
//
// Where a shared JSON vector already encodes a packet we reuse its bytes;
// stateful cases (current link != unlinked) are crafted inline.

#include <unity.h>
#include <string.h>

#include "link_control.h"
#include "../generated/test_vectors.h"

using namespace dsb;

void setUp() {}
void tearDown() {}

// --- helpers ---------------------------------------------------------------

static void craft(uint8_t out[CONTROL_COMMAND_SIZE], uint8_t command, uint8_t slot,
                  uint8_t flags, uint32_t group_id, uint32_t value) {
  out[0] = PROTOCOL_VERSION;
  out[1] = command;
  out[2] = slot;
  out[3] = flags;
  writeLe32(out + 4, group_id);
  writeLe32(out + 8, value);
}

static const DsbControlVector* vec(const char* name) {
  for (size_t i = 0; i < kDsbControlValidCount; ++i) {
    if (strcmp(kDsbControlValid[i].name, name) == 0) return &kDsbControlValid[i];
  }
  return nullptr;
}

static LinkState linked(uint32_t group, uint8_t slot, uint32_t generation, bool armed) {
  return LinkState{group, slot, generation, armed};
}

static LinkState unlinked() { return LinkState{0, 0, 0, true}; }

static void assert_state(const LinkState& expected, const LinkState& actual) {
  TEST_ASSERT_EQUAL_UINT32(expected.group_id, actual.group_id);
  TEST_ASSERT_EQUAL_UINT8(expected.slot, actual.slot);
  TEST_ASSERT_EQUAL_UINT32(expected.generation, actual.generation);
  TEST_ASSERT_EQUAL(expected.armed, actual.armed);
}

static void assert_reject(const ControlOutcome& o, uint8_t cmd, const char* code, uint16_t aux) {
  TEST_ASSERT_FALSE(o.ok);
  TEST_ASSERT_EQUAL_UINT8(cmd, o.cmd);
  TEST_ASSERT_NOT_NULL(o.error_code);
  TEST_ASSERT_EQUAL_STRING(code, o.error_code);
  TEST_ASSERT_FALSE(o.persist);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)StateType::Error, (uint8_t)o.publish_type);
  TEST_ASSERT_EQUAL_UINT16(aux, o.publish_aux);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)SideEffectOrder::ResultPublishBlink, (uint8_t)o.order);
}

// --- LINK ------------------------------------------------------------------

void test_link_group0_rejected() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::Link, 1, 0, /*group=*/0, 0);
  LinkState cur = unlinked();
  ControlOutcome o = evaluateControl(p, sizeof(p), cur);
  assert_reject(o, 1, "invalid_group", 2);
  assert_state(cur, o.state);  // unchanged
}

void test_link_slot0_rejected() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::Link, 0, 0, 11259375, 0);
  ControlOutcome o = evaluateControl(p, sizeof(p), unlinked());
  assert_reject(o, 1, "invalid_slot", 3);
}

void test_link_slot3_rejected() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::Link, 3, 0, 11259375, 0);
  ControlOutcome o = evaluateControl(p, sizeof(p), unlinked());
  assert_reject(o, 1, "invalid_slot", 3);
}

void test_link_unlinked_accept() {
  const DsbControlVector* v = vec("link_slot1");  // group 11259375, slot 1
  TEST_ASSERT_NOT_NULL(v);
  LinkState cur = unlinked();
  ControlOutcome o = evaluateControl(v->bytes, CONTROL_COMMAND_SIZE, cur);
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_NULL(o.error_code);
  TEST_ASSERT_EQUAL_STRING("linked", o.message);
  TEST_ASSERT_TRUE(o.persist);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)StateType::Link, (uint8_t)o.publish_type);
  TEST_ASSERT_EQUAL_UINT16(0, o.publish_aux);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)SideEffectOrder::ResultBlinkPublish, (uint8_t)o.order);
  assert_state(linked(11259375, 1, cur.generation + 1, true), o.state);
}

void test_link_relink_same_group_accepted() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::Link, 2, 0, /*group=*/777, 0);  // no force
  LinkState cur = linked(777, 1, 4, true);
  ControlOutcome o = evaluateControl(p, sizeof(p), cur);
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_TRUE(o.persist);
  assert_state(linked(777, 2, 5, true), o.state);
}

void test_link_different_group_no_force_conflict() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::Link, 1, 0, /*group=*/999, 0);  // no force
  LinkState cur = linked(777, 1, 4, true);
  ControlOutcome o = evaluateControl(p, sizeof(p), cur);
  assert_reject(o, 1, "link_conflict", 4);
  TEST_ASSERT_EQUAL_STRING("already linked to another group; use force", o.message);
  assert_state(cur, o.state);
}

void test_link_force_overwrites_other_group() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::Link, 2, ControlFlag0, /*group=*/999, 0);  // force
  LinkState cur = linked(777, 1, 4, true);
  ControlOutcome o = evaluateControl(p, sizeof(p), cur);
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_TRUE(o.persist);
  assert_state(linked(999, 2, 5, true), o.state);
}

void test_link_generation_increments_each_accept() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::Link, 1, 0, 777, 0);
  LinkState cur = linked(777, 1, 41, true);
  ControlOutcome o = evaluateControl(p, sizeof(p), cur);
  TEST_ASSERT_EQUAL_UINT32(42, o.state.generation);
}

// --- UNLINK ----------------------------------------------------------------

void test_unlink_when_unlinked_is_noop() {
  const DsbControlVector* v = vec("unlink");  // group 11259375, no force
  TEST_ASSERT_NOT_NULL(v);
  LinkState cur = unlinked();
  ControlOutcome o = evaluateControl(v->bytes, CONTROL_COMMAND_SIZE, cur);
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_NULL(o.error_code);
  TEST_ASSERT_EQUAL_STRING("already unlinked", o.message);
  TEST_ASSERT_FALSE(o.persist);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)StateType::Link, (uint8_t)o.publish_type);
  TEST_ASSERT_EQUAL_UINT32(0, o.blink_ms);  // no blink
  assert_state(cur, o.state);               // generation NOT incremented
}

void test_unlink_same_group_clears() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::Unlink, 0, 0, /*group=*/777, 0);
  LinkState cur = linked(777, 1, 4, true);
  ControlOutcome o = evaluateControl(p, sizeof(p), cur);
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_EQUAL_STRING("unlinked", o.message);
  TEST_ASSERT_TRUE(o.persist);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)StateType::Link, (uint8_t)o.publish_type);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)SideEffectOrder::BlinkPublishResult, (uint8_t)o.order);
  assert_state(linked(0, 0, 5, true), o.state);
}

void test_unlink_group0_clears() {
  const DsbControlVector* v = vec("unlink_group0");  // group 0
  TEST_ASSERT_NOT_NULL(v);
  LinkState cur = linked(777, 2, 4, true);
  ControlOutcome o = evaluateControl(v->bytes, CONTROL_COMMAND_SIZE, cur);
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_TRUE(o.persist);
  assert_state(linked(0, 0, 5, true), o.state);
}

void test_unlink_different_group_no_force_conflict() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::Unlink, 0, 0, /*group=*/999, 0);  // no force
  LinkState cur = linked(777, 1, 4, true);
  ControlOutcome o = evaluateControl(p, sizeof(p), cur);
  assert_reject(o, 2, "link_conflict", 5);
  TEST_ASSERT_EQUAL_STRING("linked to different group; use force", o.message);
  assert_state(cur, o.state);
}

void test_unlink_force_clears() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::Unlink, 0, ControlFlag0, /*group=*/999, 0);  // force
  LinkState cur = linked(777, 1, 4, true);
  ControlOutcome o = evaluateControl(p, sizeof(p), cur);
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_TRUE(o.persist);
  assert_state(linked(0, 0, 5, true), o.state);
}

// --- SET_ARMED -------------------------------------------------------------

void test_set_armed_true() {
  const DsbControlVector* v = vec("set_armed_true");  // flags bit0 = 1
  TEST_ASSERT_NOT_NULL(v);
  LinkState cur = linked(777, 1, 4, false);
  ControlOutcome o = evaluateControl(v->bytes, CONTROL_COMMAND_SIZE, cur);
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_EQUAL_STRING("armed", o.message);
  TEST_ASSERT_FALSE(o.persist);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)StateType::State, (uint8_t)o.publish_type);
  assert_state(linked(777, 1, 4, true), o.state);  // only armed changes
}

void test_set_armed_false() {
  const DsbControlVector* v = vec("set_armed_false");  // flags bit0 = 0
  TEST_ASSERT_NOT_NULL(v);
  LinkState cur = linked(777, 1, 4, true);
  ControlOutcome o = evaluateControl(v->bytes, CONTROL_COMMAND_SIZE, cur);
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_EQUAL_STRING("disarmed", o.message);
  TEST_ASSERT_FALSE(o.persist);
  assert_state(linked(777, 1, 4, false), o.state);
}

// --- IDENTIFY --------------------------------------------------------------

void test_identify_value_3000_kept() {
  const DsbControlVector* v = vec("identify_3000");  // value 3000
  TEST_ASSERT_NOT_NULL(v);
  ControlOutcome o = evaluateControl(v->bytes, CONTROL_COMMAND_SIZE, unlinked());
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_EQUAL_STRING("identify", o.message);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)StateType::State, (uint8_t)o.publish_type);
  TEST_ASSERT_EQUAL_UINT32(3000, o.blink_ms);
}

void test_identify_value_0_becomes_3000() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::Identify, 0, 0, 0, /*value=*/0);
  ControlOutcome o = evaluateControl(p, sizeof(p), unlinked());
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_EQUAL_UINT32(3000, o.blink_ms);
}

void test_identify_value_30001_becomes_3000() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::Identify, 0, 0, 0, /*value=*/30001);
  ControlOutcome o = evaluateControl(p, sizeof(p), unlinked());
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_EQUAL_UINT32(3000, o.blink_ms);
}

void test_identify_value_12345_kept() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::Identify, 0, 0, 0, /*value=*/12345);
  ControlOutcome o = evaluateControl(p, sizeof(p), unlinked());
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_EQUAL_UINT32(12345, o.blink_ms);
}

// --- FACTORY_RESET_LINK ----------------------------------------------------

void test_factory_reset_no_force_rejected() {
  const DsbControlVector* v = vec("factory_reset_link");  // flags = 0
  TEST_ASSERT_NOT_NULL(v);
  LinkState cur = linked(777, 1, 4, true);
  ControlOutcome o = evaluateControl(v->bytes, CONTROL_COMMAND_SIZE, cur);
  assert_reject(o, 5, "force_required", 6);
  TEST_ASSERT_EQUAL_STRING("FACTORY_RESET_LINK requires force flag", o.message);
  assert_state(cur, o.state);
}

void test_factory_reset_force_clears() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::FactoryResetLink, 0, ControlFlag0, 0, 0);  // force
  LinkState cur = linked(777, 1, 4, true);
  ControlOutcome o = evaluateControl(p, sizeof(p), cur);
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_EQUAL_STRING("factory reset link done", o.message);
  TEST_ASSERT_TRUE(o.persist);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)StateType::Link, (uint8_t)o.publish_type);
  assert_state(linked(0, 0, 5, true), o.state);
}

// --- receiver validation policy (SPEC section 9) ----------------------------
// v1 receivers ignore slot for non-LINK commands, group_id for
// SET_ARMED/IDENTIFY/FACTORY_RESET_LINK, and value for non-IDENTIFY commands.

void test_unlink_ignores_nonzero_slot() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::Unlink, /*slot=*/1, 0, /*group=*/0, 0);
  LinkState cur = linked(777, 1, 4, true);
  ControlOutcome o = evaluateControl(p, sizeof(p), cur);
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_EQUAL_STRING("unlinked", o.message);
  assert_state(linked(0, 0, 5, true), o.state);
}

void test_set_armed_ignores_slot_and_group() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::SetArmed, /*slot=*/2, ControlFlag0, /*group=*/123, 0);
  ControlOutcome o = evaluateControl(p, sizeof(p), unlinked());
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_TRUE(o.state.armed);
}

void test_identify_ignores_nonzero_slot() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::Identify, /*slot=*/2, 0, 0, /*value=*/3000);
  ControlOutcome o = evaluateControl(p, sizeof(p), unlinked());
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_EQUAL_UINT32(3000, o.blink_ms);
}

void test_factory_reset_ignores_nonzero_group() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, (uint8_t)ControlCommand::FactoryResetLink, 0, ControlFlag0, /*group=*/11259375, 0);
  LinkState cur = linked(777, 1, 4, true);
  ControlOutcome o = evaluateControl(p, sizeof(p), cur);
  TEST_ASSERT_TRUE(o.ok);
  TEST_ASSERT_EQUAL_STRING("factory reset link done", o.message);
  assert_state(linked(0, 0, 5, true), o.state);
}

// --- unknown / malformed ---------------------------------------------------

void test_unknown_command_rejected() {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, /*command=*/9, 0, 0, 0, 0);
  ControlOutcome o = evaluateControl(p, sizeof(p), unlinked());
  assert_reject(o, 9, "unknown_command", 7);
  TEST_ASSERT_EQUAL_STRING("unsupported control command", o.message);
}

void test_wrong_length_is_invalid_packet() {
  for (size_t i = 0; i < kDsbControlInvalidCount; ++i) {
    const DsbRawVector& v = kDsbControlInvalid[i];
    if (v.len == CONTROL_COMMAND_SIZE) continue;  // handled by version test
    ControlOutcome o = evaluateControl(v.bytes, v.len, unlinked());
    assert_reject(o, 0, "invalid_packet", 1);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(
        "control command must be 12 bytes and version=1", o.message, v.name);
  }
}

void test_wrong_version_is_invalid_packet() {
  const DsbRawVector* found = nullptr;
  for (size_t i = 0; i < kDsbControlInvalidCount; ++i) {
    if (strcmp(kDsbControlInvalid[i].name, "wrong_version_2") == 0) found = &kDsbControlInvalid[i];
  }
  TEST_ASSERT_NOT_NULL(found);
  ControlOutcome o = evaluateControl(found->bytes, found->len, unlinked());
  assert_reject(o, 0, "invalid_packet", 1);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_link_group0_rejected);
  RUN_TEST(test_link_slot0_rejected);
  RUN_TEST(test_link_slot3_rejected);
  RUN_TEST(test_link_unlinked_accept);
  RUN_TEST(test_link_relink_same_group_accepted);
  RUN_TEST(test_link_different_group_no_force_conflict);
  RUN_TEST(test_link_force_overwrites_other_group);
  RUN_TEST(test_link_generation_increments_each_accept);
  RUN_TEST(test_unlink_when_unlinked_is_noop);
  RUN_TEST(test_unlink_same_group_clears);
  RUN_TEST(test_unlink_group0_clears);
  RUN_TEST(test_unlink_different_group_no_force_conflict);
  RUN_TEST(test_unlink_force_clears);
  RUN_TEST(test_set_armed_true);
  RUN_TEST(test_set_armed_false);
  RUN_TEST(test_identify_value_3000_kept);
  RUN_TEST(test_identify_value_0_becomes_3000);
  RUN_TEST(test_identify_value_30001_becomes_3000);
  RUN_TEST(test_identify_value_12345_kept);
  RUN_TEST(test_factory_reset_no_force_rejected);
  RUN_TEST(test_factory_reset_force_clears);
  RUN_TEST(test_unlink_ignores_nonzero_slot);
  RUN_TEST(test_set_armed_ignores_slot_and_group);
  RUN_TEST(test_identify_ignores_nonzero_slot);
  RUN_TEST(test_factory_reset_ignores_nonzero_group);
  RUN_TEST(test_unknown_command_rejected);
  RUN_TEST(test_wrong_length_is_invalid_packet);
  RUN_TEST(test_wrong_version_is_invalid_packet);
  return UNITY_END();
}
