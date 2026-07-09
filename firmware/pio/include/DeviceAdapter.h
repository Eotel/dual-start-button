#pragma once

#include <Arduino.h>
#include <M5Unified.h>

#include "config.h"

class DeviceAdapter {
 public:
  void begin();
  void update();
  bool readButtonPressed();
  void setStatusRgb(uint8_t r, uint8_t g, uint8_t b);
  void blinkStatus(uint8_t r, uint8_t g, uint8_t b, uint32_t durationMs);
  void showText(const String& line1, const String& line2 = "", const String& line3 = "");
  const char* modelName() const { return DSB_TARGET_NAME; }

 private:
  uint32_t blinkUntilMs_ = 0;
  uint8_t blinkR_ = 0;
  uint8_t blinkG_ = 0;
  uint8_t blinkB_ = 0;
  uint32_t lastBlinkToggleMs_ = 0;
  bool blinkOn_ = false;
  uint8_t currentR_ = 0;
  uint8_t currentG_ = 0;
  uint8_t currentB_ = 0;
  bool appliedRgbValid_ = false;
  uint8_t appliedR_ = 0;
  uint8_t appliedG_ = 0;
  uint8_t appliedB_ = 0;
  String shownLine1_;
  String shownLine2_;
  String shownLine3_;

  void applyRgb(uint8_t r, uint8_t g, uint8_t b);
  void drawTextLine(int16_t y, const String& next, String& previous);
};
