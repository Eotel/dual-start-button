#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

#include "DeviceAdapter.h"
#include "config.h"
#include "hid_fallback.h"
#include "protocol.h"

using namespace dsb;

namespace {

DeviceAdapter device;

NimBLEServer* server = nullptr;
NimBLEHIDDevice* hid = nullptr;
NimBLECharacteristic* inputReportChr = nullptr;
NimBLECharacteristic* bootInputChr = nullptr;

String deviceId;
String deviceName;
uint32_t deviceHash = 0;
HidKeyMapping keyMapping{0x68, "F13"};

bool connected = false;
bool shouldStartAdvertising = false;

bool stablePressed = false;
bool lastRawPressed = false;
uint32_t rawChangedAtMs = 0;
uint32_t lastDisplayMs = 0;
bool displaySnapshotValid = false;
bool lastDisplayConnected = false;
bool lastDisplayPressed = false;

uint8_t keyboardReportMap[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xa1, 0x01,  // Collection (Application)
    0x85, HID_KEYBOARD_REPORT_ID,
    0x05, 0x07,  // Usage Page (Keyboard/Keypad)
    0x19, 0xe0,  // Usage Minimum (Left Control)
    0x29, 0xe7,  // Usage Maximum (Right GUI)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0x01,  // Logical Maximum (1)
    0x75, 0x01,  // Report Size (1)
    0x95, 0x08,  // Report Count (8)
    0x81, 0x02,  // Input (Data, Variable, Absolute)
    0x95, 0x01,  // Report Count (1)
    0x75, 0x08,  // Report Size (8)
    0x81, 0x01,  // Input (Constant)
    0x95, 0x06,  // Report Count (6)
    0x75, 0x08,  // Report Size (8)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0x73,  // Logical Maximum (F24 usage)
    0x05, 0x07,  // Usage Page (Keyboard/Keypad)
    0x19, 0x00,  // Usage Minimum (0)
    0x29, 0x73,  // Usage Maximum (F24)
    0x81, 0x00,  // Input (Data, Array)
    0xc0,        // End Collection
};

String hexDeviceIdFromEfuse() {
  const uint64_t mac = ESP.getEfuseMac();
  char buf[13];
  snprintf(buf, sizeof(buf), "%04X%08X", static_cast<uint16_t>((mac >> 32) & 0xffff),
           static_cast<uint32_t>(mac & 0xffffffff));
  return String(buf);
}

String shortNameSuffix(uint32_t hash) {
  char buf[7];
  snprintf(buf, sizeof(buf), "%06lX", static_cast<unsigned long>(hash & 0x00ffffffUL));
  return String(buf);
}

void updateStatusIndication() {
  if (stablePressed) {
    device.setStatusRgb(64, 0, 0);
  } else if (connected) {
    device.setStatusRgb(0, 48, 0);
  } else {
    device.setStatusRgb(0, 0, 32);
  }
}

void sendKeyboardReport(bool pressed) {
  uint8_t report[HID_KEYBOARD_REPORT_SIZE];
  makeHidKeyboardReport(keyMapping.usage, pressed, report);

  if (inputReportChr) {
    inputReportChr->setValue(report, sizeof(report));
    if (connected) inputReportChr->notify();
  }
  if (bootInputChr) {
    bootInputChr->setValue(report, sizeof(report));
    if (connected) bootInputChr->notify();
  }

  Serial.printf("hid %s key=%s usage=0x%02x\n", pressed ? "down" : "up", keyMapping.code, keyMapping.usage);
}

void updateDisplay() {
  const uint32_t now = millis();
  if (displaySnapshotValid && lastDisplayConnected == connected && lastDisplayPressed == stablePressed &&
      now - lastDisplayMs < 1000) {
    return;
  }
  displaySnapshotValid = true;
  lastDisplayConnected = connected;
  lastDisplayPressed = stablePressed;
  lastDisplayMs = now;

  String l1 = deviceName + " HID " + String(DSB_FW_VERSION);
  String l2 = connected ? "paired/connected" : "advertising";
  String l3 = "key=" + String(keyMapping.code) + " pressed=" + String(stablePressed ? "1" : "0");
  device.showText(l1, l2, l3);
}

class ServerCallbacks final : public NimBLEServerCallbacks {
 public:
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    (void)pServer;
    connected = true;
    NimBLEDevice::startSecurity(connInfo.getConnHandle());
    Serial.printf("HID connected handle=%u bonded=%u encrypted=%u\n", connInfo.getConnHandle(), connInfo.isBonded(),
                  connInfo.isEncrypted());
    sendKeyboardReport(false);
    updateStatusIndication();
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    (void)pServer;
    (void)connInfo;
    connected = false;
    stablePressed = false;
    Serial.printf("HID disconnected reason=%d\n", reason);
    sendKeyboardReport(false);
    shouldStartAdvertising = true;
    updateStatusIndication();
  }
};

void setupHid() {
  NimBLEDevice::init(deviceName.c_str());
  NimBLEDevice::setPower(ESP_PWR_LVL_P6);
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  hid = new NimBLEHIDDevice(server);
  hid->setManufacturer("Dual Start Button");
  hid->setPnp(0x02, 0x0000, 0x0000, 0x0100);
  hid->setHidInfo(0x00, 0x02);
  hid->setReportMap(keyboardReportMap, sizeof(keyboardReportMap));
  hid->setBatteryLevel(100);

  inputReportChr = hid->getInputReport(HID_KEYBOARD_REPORT_ID);
  bootInputChr = hid->getBootInput();
  sendKeyboardReport(false);

  server->start();

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->setAppearance(HID_KEYBOARD);
  advertising->addServiceUUID("1812");
  advertising->setName(deviceName.c_str());
  advertising->start();

  Serial.printf("HID advertising %s key=%s usage=0x%02x\n", deviceName.c_str(), keyMapping.code, keyMapping.usage);
}

void updateButton() {
  const uint32_t now = millis();
  const bool raw = device.readButtonPressed();

  if (raw != lastRawPressed) {
    lastRawPressed = raw;
    rawChangedAtMs = now;
  }

  if (raw != stablePressed && now - rawChangedAtMs >= DSB_DEBOUNCE_MS) {
    stablePressed = raw;
    sendKeyboardReport(stablePressed);
    updateStatusIndication();
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println("Dual Start Button HID fallback booting");

  device.begin();
  device.blinkStatus(64, 64, 64, 800);

  deviceId = hexDeviceIdFromEfuse();
  deviceHash = fnv1a32(deviceId.c_str());
  deviceName = "DSB-HID-" + shortNameSuffix(deviceHash);
  keyMapping = hidKeyForDeviceHash(deviceHash);

  Serial.printf("device_id=%s hash=%lu model=%s fw=%s hid_key=%s usage=0x%02x\n", deviceId.c_str(),
                static_cast<unsigned long>(deviceHash), DSB_TARGET_NAME, DSB_FW_VERSION, keyMapping.code,
                keyMapping.usage);

  updateStatusIndication();
  setupHid();
}

void loop() {
  device.update();
  updateButton();
  updateDisplay();

  if (shouldStartAdvertising) {
    shouldStartAdvertising = false;
    delay(50);
    NimBLEDevice::getAdvertising()->start();
    Serial.println("HID advertising restarted");
  }

  delay(5);
}
