#include <string.h>
#include <unity.h>

#include "hid_fallback.h"

using namespace dsb;

void setUp() {}
void tearDown() {}

void test_hid_key_mapping_uses_stable_f13_to_f24_range() {
  HidKeyMapping first = hidKeyForDeviceHash(0);
  TEST_ASSERT_EQUAL_UINT8(0x68, first.usage);
  TEST_ASSERT_EQUAL_STRING("F13", first.code);

  HidKeyMapping last = hidKeyForDeviceHash(11);
  TEST_ASSERT_EQUAL_UINT8(0x73, last.usage);
  TEST_ASSERT_EQUAL_STRING("F24", last.code);

  HidKeyMapping wrapped = hidKeyForDeviceHash(12);
  TEST_ASSERT_EQUAL_UINT8(0x68, wrapped.usage);
  TEST_ASSERT_EQUAL_STRING("F13", wrapped.code);
}

void test_hid_key_mapping_is_not_slot_based() {
  const HidKeyMapping a = hidKeyForDeviceHash(2777541019u);
  const HidKeyMapping b = hidKeyForDeviceHash(2777541019u);
  TEST_ASSERT_EQUAL_UINT8(a.usage, b.usage);
  TEST_ASSERT_EQUAL_STRING(a.code, b.code);
}

void test_hid_keyboard_report_contains_single_usage_when_pressed() {
  uint8_t report[HID_KEYBOARD_REPORT_SIZE];
  makeHidKeyboardReport(0x6a, true, report);

  const uint8_t expected[HID_KEYBOARD_REPORT_SIZE] = {0x00, 0x00, 0x6a, 0x00, 0x00, 0x00, 0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, report, HID_KEYBOARD_REPORT_SIZE);
}

void test_hid_keyboard_report_is_empty_when_released() {
  uint8_t report[HID_KEYBOARD_REPORT_SIZE];
  makeHidKeyboardReport(0x6a, false, report);

  const uint8_t expected[HID_KEYBOARD_REPORT_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, report, HID_KEYBOARD_REPORT_SIZE);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_hid_key_mapping_uses_stable_f13_to_f24_range);
  RUN_TEST(test_hid_key_mapping_is_not_slot_based);
  RUN_TEST(test_hid_keyboard_report_contains_single_usage_when_pressed);
  RUN_TEST(test_hid_keyboard_report_is_empty_when_released);
  return UNITY_END();
}
