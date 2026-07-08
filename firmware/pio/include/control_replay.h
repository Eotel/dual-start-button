#pragma once
#include <stdint.h>

#include "link_control.h"

// Hardware-free replay of a ControlOutcome.
//
// evaluateControl() (link_control.h) decides *what* should happen; this file
// pins *how* the firmware carries it out: apply the new link state first, then
// persist + refresh DeviceInfo when required, then emit ControlResult /
// ButtonState / blink in the outcome's SideEffectOrder. main.cpp implements
// ControlEffects against NVS, NimBLE, and the DeviceAdapter; native tests use
// a recording fake.

namespace dsb {

// Method names deliberately differ from main.cpp's free functions
// (saveLink/sendControlResult/publishState) so the implementations can call
// them without :: qualification or accidental self-recursion.
struct ControlEffects {
  virtual ~ControlEffects() = default;
  virtual void applyLinkState(const LinkState& state) = 0;
  virtual void persistLink() = 0;
  virtual void refreshDeviceInfo() = 0;
  virtual void emitControlResult(bool ok, uint8_t cmd, const char* error_code, const char* message) = 0;
  virtual void emitButtonState(StateType type, uint16_t aux) = 0;
  virtual void blinkStatus(uint8_t r, uint8_t g, uint8_t b, uint32_t duration_ms) = 0;
};

inline void replayControlOutcome(const ControlOutcome& outcome, ControlEffects& fx) {
  fx.applyLinkState(outcome.state);
  if (outcome.persist) {
    fx.persistLink();
    fx.refreshDeviceInfo();
  }

  // A blink_ms of 0 skips the blink.
  switch (outcome.order) {
    case SideEffectOrder::ResultPublishBlink:
      fx.emitControlResult(outcome.ok, outcome.cmd, outcome.error_code, outcome.message);
      fx.emitButtonState(outcome.publish_type, outcome.publish_aux);
      if (outcome.blink_ms) fx.blinkStatus(outcome.blink_r, outcome.blink_g, outcome.blink_b, outcome.blink_ms);
      break;
    case SideEffectOrder::ResultBlinkPublish:
      fx.emitControlResult(outcome.ok, outcome.cmd, outcome.error_code, outcome.message);
      if (outcome.blink_ms) fx.blinkStatus(outcome.blink_r, outcome.blink_g, outcome.blink_b, outcome.blink_ms);
      fx.emitButtonState(outcome.publish_type, outcome.publish_aux);
      break;
    case SideEffectOrder::BlinkPublishResult:
      if (outcome.blink_ms) fx.blinkStatus(outcome.blink_r, outcome.blink_g, outcome.blink_b, outcome.blink_ms);
      fx.emitButtonState(outcome.publish_type, outcome.publish_aux);
      fx.emitControlResult(outcome.ok, outcome.cmd, outcome.error_code, outcome.message);
      break;
  }
}

}  // namespace dsb
