// Native (host) Unity tests for the ControlOutcome replay in
// include/control_replay.h. A recording fake stands in for main.cpp's real
// side effects (NVS save, DeviceInfo refresh, ControlResult, ButtonState
// publish, status blink) so the apply -> persist -> notify sequencing is
// pinned without hardware.
//
// Outcomes are produced by the real evaluateControl() so these tests cover the
// same packets a host would write.

#include <unity.h>

#include <string>
#include <vector>

#include "control_replay.h"
#include "link_control.h"
#include "../support/craft_control.h"

using namespace dsb;

void setUp() {}
void tearDown() {}

// --- recording fake ----------------------------------------------------------

struct FakeEffects final : ControlEffects {
  std::vector<std::string> events;

  void applyLinkState(const LinkState& state) override {
    char buf[64];
    snprintf(buf, sizeof(buf), "apply g=%lu s=%u gen=%lu armed=%d",
             static_cast<unsigned long>(state.group_id), state.slot,
             static_cast<unsigned long>(state.generation), state.armed ? 1 : 0);
    events.push_back(buf);
  }
  void persistLink() override { events.push_back("save"); }
  void refreshDeviceInfo() override { events.push_back("refresh"); }
  void emitControlResult(bool ok, uint8_t cmd, const char* error_code,
                         const char* message) override {
    (void)message;
    char buf[64];
    snprintf(buf, sizeof(buf), "result ok=%d cmd=%u err=%s", ok ? 1 : 0, cmd,
             error_code ? error_code : "-");
    events.push_back(buf);
  }
  void emitButtonState(StateType type, uint16_t aux) override {
    char buf[32];
    snprintf(buf, sizeof(buf), "publish t=%u aux=%u", static_cast<uint8_t>(type), aux);
    events.push_back(buf);
  }
  void blinkStatus(uint8_t r, uint8_t g, uint8_t b, uint32_t duration_ms) override {
    char buf[48];
    snprintf(buf, sizeof(buf), "blink %u,%u,%u,%lu", r, g, b,
             static_cast<unsigned long>(duration_ms));
    events.push_back(buf);
  }
};

static FakeEffects replay(uint8_t command, uint8_t slot, uint8_t flags,
                          uint32_t group_id, uint32_t value, const LinkState& current) {
  uint8_t p[CONTROL_COMMAND_SIZE];
  craft(p, command, slot, flags, group_id, value);
  FakeEffects fx;
  replayControlOutcome(evaluateControl(p, sizeof(p), current), fx);
  return fx;
}

static void assert_events(const std::vector<std::string>& expected, const FakeEffects& fx) {
  TEST_ASSERT_EQUAL_UINT32_MESSAGE(expected.size(), fx.events.size(),
                                   "unexpected side-effect count");
  for (size_t i = 0; i < expected.size(); ++i) {
    TEST_ASSERT_EQUAL_STRING(expected[i].c_str(), fx.events[i].c_str());
  }
}

// --- accepted commands -------------------------------------------------------

void test_link_accept_persists_then_result_blink_publish() {
  const FakeEffects fx = replay((uint8_t)ControlCommand::Link, 1, 0, 999, 0,
                                LinkState{0, 0, 0, true});
  assert_events({
      "apply g=999 s=1 gen=1 armed=1",
      "save",
      "refresh",
      "result ok=1 cmd=1 err=-",
      "blink 0,64,0,900",
      "publish t=4 aux=0",
  }, fx);
}

void test_unlink_clear_persists_then_blink_publish_result() {
  const FakeEffects fx = replay((uint8_t)ControlCommand::Unlink, 0, 0, 777, 0,
                                LinkState{777, 1, 4, true});
  assert_events({
      "apply g=0 s=0 gen=5 armed=1",
      "save",
      "refresh",
      "blink 0,64,64,1200",
      "publish t=4 aux=0",
      "result ok=1 cmd=2 err=-",
  }, fx);
}

void test_unlink_noop_skips_persist_and_blink() {
  const FakeEffects fx = replay((uint8_t)ControlCommand::Unlink, 0, 0, 777, 0,
                                LinkState{0, 0, 0, true});
  assert_events({
      "apply g=0 s=0 gen=0 armed=1",
      "result ok=1 cmd=2 err=-",
      "publish t=4 aux=0",
  }, fx);
}

void test_set_armed_false_publishes_state_without_persist() {
  const FakeEffects fx = replay((uint8_t)ControlCommand::SetArmed, 0, 0, 0, 0,
                                LinkState{777, 1, 4, true});
  assert_events({
      "apply g=777 s=1 gen=4 armed=0",
      "result ok=1 cmd=3 err=-",
      "publish t=1 aux=0",
      "blink 64,32,0,700",
  }, fx);
}

// --- rejected commands -------------------------------------------------------

void test_link_conflict_reject_keeps_state_and_publishes_error() {
  const FakeEffects fx = replay((uint8_t)ControlCommand::Link, 1, 0, 999, 0,
                                LinkState{777, 1, 4, true});
  assert_events({
      "apply g=777 s=1 gen=4 armed=1",  // unchanged; no save/refresh
      "result ok=0 cmd=1 err=link_conflict",
      "publish t=5 aux=4",
      "blink 64,32,0,1500",
  }, fx);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_link_accept_persists_then_result_blink_publish);
  RUN_TEST(test_unlink_clear_persists_then_blink_publish_result);
  RUN_TEST(test_unlink_noop_skips_persist_and_blink);
  RUN_TEST(test_set_armed_false_publishes_state_without_persist);
  RUN_TEST(test_link_conflict_reject_keeps_state_and_publishes_error);
  return UNITY_END();
}
