#include "DeviceAdapter.h"

void DeviceAdapter::begin() {
  auto cfg = M5.config();
  cfg.clear_display = true;
  M5.begin(cfg);

#if DSB_BUTTON_SOURCE == DSB_BUTTON_SOURCE_GPIO
  pinMode(DSB_BUTTON_GPIO, INPUT);
#endif

  if (M5.Led.isEnabled()) {
    M5.Led.setBrightness(32);
    M5.Led.setAllColor(0x000000);
  }

  if (M5.Display.width() > 0 && M5.Display.height() > 0) {
    M5.Display.setTextSize(1);
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  }
}

void DeviceAdapter::update() {
  M5.update();

  const uint32_t now = millis();
  if (blinkUntilMs_ > now) {
    if (now - lastBlinkToggleMs_ >= 160) {
      lastBlinkToggleMs_ = now;
      blinkOn_ = !blinkOn_;
      if (blinkOn_) {
        applyRgb(blinkR_, blinkG_, blinkB_);
      } else {
        applyRgb(0, 0, 0);
      }
    }
  } else if (blinkUntilMs_ != 0) {
    blinkUntilMs_ = 0;
    applyRgb(currentR_, currentG_, currentB_);
  }
}

bool DeviceAdapter::readButtonPressed() {
#if DSB_BUTTON_SOURCE == DSB_BUTTON_SOURCE_A
  return M5.BtnA.isPressed();
#elif DSB_BUTTON_SOURCE == DSB_BUTTON_SOURCE_B
  return M5.BtnB.isPressed();
#elif DSB_BUTTON_SOURCE == DSB_BUTTON_SOURCE_C
  return M5.BtnC.isPressed();
#elif DSB_BUTTON_SOURCE == DSB_BUTTON_SOURCE_PWR
  return M5.BtnPWR.isPressed();
#elif DSB_BUTTON_SOURCE == DSB_BUTTON_SOURCE_GPIO
  return digitalRead(DSB_BUTTON_GPIO) == DSB_BUTTON_ACTIVE_LEVEL;
#else
  return M5.BtnA.isPressed();
#endif
}

void DeviceAdapter::setStatusRgb(uint8_t r, uint8_t g, uint8_t b) {
  currentR_ = r;
  currentG_ = g;
  currentB_ = b;
  if (blinkUntilMs_ == 0) {
    applyRgb(r, g, b);
  }
}

void DeviceAdapter::blinkStatus(uint8_t r, uint8_t g, uint8_t b, uint32_t durationMs) {
  blinkR_ = r;
  blinkG_ = g;
  blinkB_ = b;
  blinkUntilMs_ = millis() + durationMs;
  lastBlinkToggleMs_ = 0;
  blinkOn_ = false;
}

void DeviceAdapter::showText(const String& line1, const String& line2, const String& line3) {
  if (M5.Display.width() <= 0 || M5.Display.height() <= 0) return;
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.println(line1);
  if (line2.length()) M5.Display.println(line2);
  if (line3.length()) M5.Display.println(line3);
}

void DeviceAdapter::applyRgb(uint8_t r, uint8_t g, uint8_t b) {
  if (M5.Led.isEnabled()) {
    M5.Led.setColor(0, r, g, b);
  }

  if (M5.Display.width() > 0 && M5.Display.height() > 0) {
    // Keep display simple and non-invasive. We don't fill the whole screen here,
    // because showText() owns display content for debug-capable devices.
    M5.Display.fillRect(0, M5.Display.height() - 8, M5.Display.width(), 8, M5.Display.color565(r, g, b));
  }
}
