#include <Arduino.h>
#include <Preferences.h>
#include <NimBLEDevice.h>

#include "config.h"
#include "protocol.h"
#include "link_control.h"
#include "DeviceAdapter.h"

using namespace dsb;

namespace {

DeviceAdapter device;
Preferences prefs;

NimBLEServer* server = nullptr;
NimBLECharacteristic* deviceInfoChr = nullptr;
NimBLECharacteristic* stateChr = nullptr;
NimBLECharacteristic* controlResultChr = nullptr;

String deviceId;
String deviceName;
uint32_t deviceHash = 0;

uint32_t linkGroupId = 0;
uint8_t linkSlot = 0;
uint32_t linkGeneration = 0;
bool armed = true;
bool connected = false;
bool shouldStartAdvertising = false;

bool stablePressed = false;
bool lastRawPressed = false;
uint32_t rawChangedAtMs = 0;
uint32_t pressStartedAtMs = 0;
uint16_t lastHoldMs = 0;
bool longHoldResetTriggered = false;

uint16_t seq = 0;
uint32_t lastHeartbeatMs = 0;
uint32_t lastUiUpdateMs = 0;

String hexDeviceIdFromEfuse() {
  const uint64_t mac = ESP.getEfuseMac();
  char buf[13];
  snprintf(buf, sizeof(buf), "%04X%08X", static_cast<uint16_t>((mac >> 32) & 0xffff), static_cast<uint32_t>(mac & 0xffffffff));
  return String(buf);
}

String last4(const String& s) {
  if (s.length() <= 4) return s;
  return s.substring(s.length() - 4);
}

void saveLink() {
  prefs.putUInt("group", linkGroupId);
  prefs.putUChar("slot", linkSlot);
  prefs.putUInt("gen", linkGeneration);
}

void loadLink() {
  linkGroupId = prefs.getUInt("group", 0);
  linkSlot = prefs.getUChar("slot", 0);
  if (linkSlot > 2) linkSlot = 0;
  linkGeneration = prefs.getUInt("gen", 0);
}

String deviceInfoJson() {
  String j;
  j.reserve(320);
  j += "{\"v\":1";
  j += ",\"device_id\":\"" + deviceId + "\"";
  j += ",\"device_hash\":" + String(deviceHash);
  j += ",\"name\":\"" + deviceName + "\"";
  j += ",\"fw\":\"" DSB_FW_VERSION "\"";
  j += ",\"model\":\"" DSB_TARGET_NAME "\"";
  j += ",\"protocol\":\"dual-start-button-gatt-v1\"";
  j += ",\"link_group_id\":" + String(linkGroupId);
  j += ",\"link_slot\":" + String(linkSlot);
  j += ",\"link_generation\":" + String(linkGeneration);
  j += "}";
  return j;
}

void refreshDeviceInfoCharacteristic() {
  if (!deviceInfoChr) return;
  const String info = deviceInfoJson();
  deviceInfoChr->setValue(info.c_str());
}

uint8_t currentFlags() {
  uint8_t flags = 0;
  if (stablePressed) flags |= FlagPressed;
  if (armed) flags |= FlagArmed;
  if (linkGroupId != 0 && linkSlot != 0) flags |= FlagLinked;
  if (stablePressed && (millis() - pressStartedAtMs >= DSB_LONG_HOLD_RESET_MS)) flags |= FlagLongPressed;
  if (connected) flags |= FlagConnected;
  return flags;
}

void publishState(StateType type, uint16_t aux = 0, bool notify = true) {
  if (!stateChr) return;

  ButtonStateV1 s{};
  s.version = PROTOCOL_VERSION;
  s.type = static_cast<uint8_t>(type);
  s.flags = currentFlags();
  s.link_slot = linkSlot;
  s.seq = ++seq;
  s.uptime_ms = millis();
  s.device_hash = deviceHash;
  s.link_group_id = linkGroupId;
  s.aux = aux;

  uint8_t packet[BUTTON_STATE_SIZE];
  encodeButtonState(s, packet);
  stateChr->setValue(packet, sizeof(packet));
  if (notify && connected) {
    stateChr->notify();
  }

  Serial.printf("state type=%u flags=0x%02x slot=%u group=%lu seq=%u aux=%u\n",
                s.type, s.flags, s.link_slot, static_cast<unsigned long>(s.link_group_id), s.seq, s.aux);
}

void sendControlResult(bool ok, uint8_t cmd, const char* code, const char* message) {
  if (!controlResultChr) return;

  String j;
  j.reserve(320);
  j += "{\"v\":1";
  j += ",\"ok\":";
  j += ok ? "true" : "false";
  j += ",\"cmd\":" + String(cmd);
  if (!ok && code) {
    j += ",\"error\":\"";
    j += code;
    j += "\"";
  }
  j += ",\"message\":\"";
  j += message ? message : "";
  j += "\"";
  j += ",\"device_id\":\"" + deviceId + "\"";
  j += ",\"device_hash\":" + String(deviceHash);
  j += ",\"link_group_id\":" + String(linkGroupId);
  j += ",\"link_slot\":" + String(linkSlot);
  j += ",\"link_generation\":" + String(linkGeneration);
  j += ",\"armed\":";
  j += armed ? "true" : "false";
  j += "}";

  controlResultChr->setValue(j.c_str());
  if (connected) {
    controlResultChr->notify();
  }
  Serial.println(j);
}

void updateStatusIndication() {
  if (stablePressed) {
    device.setStatusRgb(64, 0, 0); // red
  } else if (linkGroupId != 0 && linkSlot != 0 && armed) {
    device.setStatusRgb(0, 48, 0); // green
  } else if (connected) {
    device.setStatusRgb(0, 0, 48); // blue
  } else {
    device.setStatusRgb(0, 0, 16); // dim blue
  }
}

void updateDisplay() {
  const uint32_t now = millis();
  if (now - lastUiUpdateMs < 500) return;
  lastUiUpdateMs = now;

  String l1 = deviceName + " " + String(DSB_FW_VERSION);
  String l2 = connected ? "connected" : "advertising";
  String l3 = "slot=" + String(linkSlot) + " pressed=" + String(stablePressed ? "1" : "0");
  device.showText(l1, l2, l3);
}

void applyControlOutcome(const ControlOutcome& outcome) {
  // Apply the resulting link state, then persist link fields if required.
  linkGroupId = outcome.state.group_id;
  linkSlot = outcome.state.slot;
  linkGeneration = outcome.state.generation;
  armed = outcome.state.armed;
  if (outcome.persist) {
    saveLink();
    refreshDeviceInfoCharacteristic();
  }

  // Emit the ControlResult / ButtonState / blink in the exact order the
  // original code did; the two notifying characteristics fire in different
  // orders across commands. A blink_ms of 0 skips the blink.
  switch (outcome.order) {
    case SideEffectOrder::ResultPublishBlink:
      sendControlResult(outcome.ok, outcome.cmd, outcome.error_code, outcome.message);
      publishState(outcome.publish_type, outcome.publish_aux, true);
      if (outcome.blink_ms) device.blinkStatus(outcome.blink_r, outcome.blink_g, outcome.blink_b, outcome.blink_ms);
      break;
    case SideEffectOrder::ResultBlinkPublish:
      sendControlResult(outcome.ok, outcome.cmd, outcome.error_code, outcome.message);
      if (outcome.blink_ms) device.blinkStatus(outcome.blink_r, outcome.blink_g, outcome.blink_b, outcome.blink_ms);
      publishState(outcome.publish_type, outcome.publish_aux, true);
      break;
    case SideEffectOrder::BlinkPublishResult:
      if (outcome.blink_ms) device.blinkStatus(outcome.blink_r, outcome.blink_g, outcome.blink_b, outcome.blink_ms);
      publishState(outcome.publish_type, outcome.publish_aux, true);
      sendControlResult(outcome.ok, outcome.cmd, outcome.error_code, outcome.message);
      break;
  }

  updateStatusIndication();
}

void handleControlBytes(const uint8_t* data, size_t len) {
  const LinkState current{linkGroupId, linkSlot, linkGeneration, armed};
  const ControlOutcome outcome = evaluateControl(data, len, current);
  applyControlOutcome(outcome);
}

class ServerCallbacks final : public NimBLEServerCallbacks {
public:
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    (void)pServer;
    (void)connInfo;
    connected = true;
    Serial.println("BLE connected");
    publishState(StateType::State, 0, true);
    updateStatusIndication();
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    (void)pServer;
    (void)connInfo;
    connected = false;
    Serial.printf("BLE disconnected reason=%d\n", reason);
    shouldStartAdvertising = true;
    updateStatusIndication();
  }
};

class ControlCallbacks final : public NimBLECharacteristicCallbacks {
public:
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override {
    (void)connInfo;
    std::string value = characteristic->getValue();
    handleControlBytes(reinterpret_cast<const uint8_t*>(value.data()), value.size());
  }
};

void setupBle() {
  NimBLEDevice::init(deviceName.c_str());
  NimBLEDevice::setPower(ESP_PWR_LVL_P6);

  server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* service = server->createService(DSB_SERVICE_UUID);

  deviceInfoChr = service->createCharacteristic(
    DSB_DEVICE_INFO_UUID,
    NIMBLE_PROPERTY::READ
  );

  stateChr = service->createCharacteristic(
    DSB_BUTTON_STATE_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );

  NimBLECharacteristic* controlChr = service->createCharacteristic(
    DSB_CONTROL_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  controlChr->setCallbacks(new ControlCallbacks());

  controlResultChr = service->createCharacteristic(
    DSB_CONTROL_RESULT_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );
  controlResultChr->setValue("{\"v\":1,\"ok\":true,\"message\":\"boot\"}");

  refreshDeviceInfoCharacteristic();
  service->start();

  publishState(StateType::Boot, 0, false);

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(DSB_SERVICE_UUID);
  advertising->setName(deviceName.c_str());
  advertising->start();

  Serial.printf("advertising %s service=%s\n", deviceName.c_str(), DSB_SERVICE_UUID);
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
    if (stablePressed) {
      pressStartedAtMs = now;
      lastHoldMs = 0;
      longHoldResetTriggered = false;
      publishState(StateType::State, 0, true);
    } else {
      const uint32_t hold = now - pressStartedAtMs;
      lastHoldMs = static_cast<uint16_t>(hold > 65535 ? 65535 : hold);
      publishState(StateType::State, lastHoldMs, true);
    }
    updateStatusIndication();
  }

  if (stablePressed && !longHoldResetTriggered && now - pressStartedAtMs >= DSB_LONG_HOLD_RESET_MS) {
    longHoldResetTriggered = true;
    const LinkState current{linkGroupId, linkSlot, linkGeneration, armed};
    applyControlOutcome(makeClear(current, static_cast<uint8_t>(ControlCommand::FactoryResetLink),
                                  "long hold reset link done"));
  }
}

void heartbeat() {
  const uint32_t now = millis();
  if (now - lastHeartbeatMs >= DSB_HEARTBEAT_MS) {
    lastHeartbeatMs = now;
    uint16_t aux = 0;
    if (stablePressed) {
      const uint32_t hold = now - pressStartedAtMs;
      aux = static_cast<uint16_t>(hold > 65535 ? 65535 : hold);
    } else {
      aux = lastHoldMs;
    }
    publishState(StateType::Heartbeat, aux, true);
  }
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println("Dual Start Button booting");

  device.begin();
  device.blinkStatus(64, 64, 64, 800);

  prefs.begin("dsb", false);
  deviceId = hexDeviceIdFromEfuse();
  deviceHash = fnv1a32(deviceId.c_str());
  deviceName = "DSB-" + last4(deviceId);
  loadLink();

  Serial.printf("device_id=%s hash=%lu model=%s fw=%s\n",
                deviceId.c_str(), static_cast<unsigned long>(deviceHash), DSB_TARGET_NAME, DSB_FW_VERSION);
  Serial.printf("link_group=%lu slot=%u generation=%lu\n",
                static_cast<unsigned long>(linkGroupId), linkSlot, static_cast<unsigned long>(linkGeneration));

  updateStatusIndication();
  setupBle();
}

void loop() {
  device.update();
  updateButton();
  heartbeat();
  updateDisplay();

  if (shouldStartAdvertising) {
    shouldStartAdvertising = false;
    delay(50);
    NimBLEDevice::getAdvertising()->start();
    Serial.println("advertising restarted");
  }

  delay(5);
}
