#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_system.h>

#include "BatteryManager.h"
#include "BluetoothManager.h"
#include "ButtonManager.h"
#include "Config.h"
#include "HartBridge.h"
#include "LedManager.h"
#include "WiFiDashboard.h"

ButtonManager buttonManager;
LedManager ledManager;
BatteryManager batteryManager;
HartBridge hartBridge;
BluetoothManager bluetoothManager;
WiFiDashboard wifiDashboard;
Preferences preferences;

OperatingMode currentMode = MODE_BLUETOOTH;
ModemOwner modemOwner = OWNER_NONE;

unsigned long lastUsbActivity = 0;
unsigned long lastBluetoothActivity = 0;
unsigned long lastHartActivity = 0;
unsigned long lastAutoSleepCheck = 0;
unsigned long bootTime = 0;

void printWakeReason();
void printBootInfo();
void handleButtonEvent(ButtonManager::ButtonEvent event);
void updateModemOwnership();
bool canTransmitFrom(ModemOwner owner);
void checkAutoSleep();
void handleUsbData();
void handleBluetoothData();
void handleHartData();
void updateLedStatus();
void updateDashboard();
DashboardSnapshot buildDashboardSnapshot();
void enterDeepSleep();
float readChipTemperatureC();

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0);

  esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  if (wakeCause != ESP_SLEEP_WAKEUP_EXT0) {
    esp_deep_sleep_start();
    return;
  }

  Serial.begin(115200);
  delay(500);

  Serial.println("\n\n=== Wireless HART 67 Communicator ===");
  Serial.printf("Firmware Version: %s\n", FW_VERSION);
  Serial.printf("Build Date: %s\n", FW_BUILD_DATE);
  printWakeReason();

  preferences.begin(PREF_NAMESPACE, false);
  uint32_t bootCount = preferences.getUInt(PREF_KEY_BOOT_COUNT, 0) + 1;
  preferences.putUInt(PREF_KEY_BOOT_COUNT, bootCount);
  Serial.printf("Boot Count: %u\n", bootCount);

  currentMode =
      static_cast<OperatingMode>(preferences.getUInt(PREF_KEY_LAST_MODE,
                                                     MODE_BLUETOOTH));
  Serial.printf("Restoring mode: %s\n",
                currentMode == MODE_BLUETOOTH ? "Bluetooth" : "WiFi");

  printBootInfo();

  Serial.println("\n[INIT] Initializing peripherals...");
  buttonManager.begin();
  ledManager.begin();
  batteryManager.begin();
  hartBridge.begin();

  const unsigned long now = millis();
  lastUsbActivity = now;
  lastBluetoothActivity = now;
  lastHartActivity = now;
  bootTime = now;

  if (currentMode == MODE_BLUETOOTH) {
    bluetoothManager.begin();
    ledManager.setRgbAlternating(COLOR_RED, COLOR_BLUE,
                                 LED_ALTERNATE_INTERVAL_MS);
    Serial.println("[MODE] Bluetooth mode enabled");
  } else {
    wifiDashboard.begin();
    ledManager.setRgbAlternating(COLOR_RED, COLOR_GREEN,
                                 LED_ALTERNATE_INTERVAL_MS);
    Serial.println("[MODE] WiFi Dashboard mode enabled");
  }

  ledManager.setHartColor(COLOR_RED);
  Serial.println("[INIT] Initialization complete\n");
}

void loop() {
  const unsigned long now = millis();

  buttonManager.update();
  ledManager.update();
  batteryManager.update();
  hartBridge.update();

  ButtonManager::ButtonEvent buttonEvent = buttonManager.getEvent();
  if (buttonEvent != ButtonManager::BUTTON_NONE) {
    handleButtonEvent(buttonEvent);
  }

  updateModemOwnership();
  handleUsbData();
  handleBluetoothData();
  handleHartData();
  updateLedStatus();

  static bool lowBatteryFlashActive = false;
  if (batteryManager.isLowBattery() && !ledManager.isBatteryStatusShowing()) {
    if (!lowBatteryFlashActive) {
      ledManager.setRgbFlashing(COLOR_RED, 1000 / LED_LOW_BATTERY_FLASH_HZ);
      lowBatteryFlashActive = true;
    }
  } else if (lowBatteryFlashActive) {
    lowBatteryFlashActive = false;
    updateLedStatus();
  }

  if (currentMode == MODE_BLUETOOTH) {
    bluetoothManager.update();
  } else {
    wifiDashboard.update(buildDashboardSnapshot());
  }

  if (now - lastAutoSleepCheck > 1000) {
    lastAutoSleepCheck = now;
    checkAutoSleep();
  }
}

void printWakeReason() {
  Serial.print("[BOOT] Wake reason: ");
  switch (esp_sleep_get_wakeup_cause()) {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Button (EXT0)");
    break;
  case ESP_SLEEP_WAKEUP_UNDEFINED:
    Serial.println("Power-on / reset");
    break;
  default:
    Serial.println("Other");
    break;
  }
}

void printBootInfo() {
  Serial.println("\n[BOOT] System Information:");
  Serial.printf("  ESP32 Chip: %s\n", ESP.getChipModel());
  Serial.printf("  Chip Revision: %d\n", ESP.getChipRevision());
  Serial.printf("  SDK Version: %s\n", ESP.getSdkVersion());
  Serial.printf("  CPU Frequency: %u MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("  Free Heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("  Total Heap: %u bytes\n", ESP.getHeapSize());
  Serial.printf("  MAC Address: %s\n", WiFi.macAddress().c_str());
  Serial.printf("  Lifetime Uptime: %u s\n",
                preferences.getUInt(PREF_KEY_UPTIME, 0));
  Serial.printf("  Lifetime HART Bytes: %u\n",
                preferences.getUInt(PREF_KEY_HART_BYTES, 0));

  float tempC = readChipTemperatureC();
  if (isnan(tempC)) {
    Serial.println("  Temperature: N/A");
  } else {
    Serial.printf("  Temperature: %.1f C\n", tempC);
  }
  Serial.println();
}

float readChipTemperatureC() {
#if defined(TEMP_SENSOR) || defined(CONFIG_IDF_TARGET_ESP32)
  return temperatureRead();
#else
  return NAN;
#endif
}

void handleButtonEvent(ButtonManager::ButtonEvent event) {
  Serial.printf("[BUTTON] Event: ");

  switch (event) {
  case ButtonManager::BUTTON_SINGLE_PRESS:
    Serial.println("Single Press");
    ledManager.showBatteryStatus(batteryManager.getPercentage());
    Serial.printf("  Battery: %u%% (%.2fV)\n", batteryManager.getPercentage(),
                  batteryManager.getVoltage());
    break;

  case ButtonManager::BUTTON_TRIPLE_PRESS:
    Serial.println("Triple Press");
    if (currentMode == MODE_BLUETOOTH) {
      currentMode = MODE_WIFI;
      bluetoothManager.end();
      wifiDashboard.begin();
      ledManager.setRgbAlternating(COLOR_RED, COLOR_GREEN,
                                   LED_ALTERNATE_INTERVAL_MS);
      Serial.println("[MODE] Switched to WiFi Dashboard");
    } else {
      currentMode = MODE_BLUETOOTH;
      wifiDashboard.end();
      bluetoothManager.begin();
      ledManager.setRgbAlternating(COLOR_RED, COLOR_BLUE,
                                   LED_ALTERNATE_INTERVAL_MS);
      Serial.println("[MODE] Switched to Bluetooth");
    }
    preferences.putUInt(PREF_KEY_LAST_MODE, static_cast<uint32_t>(currentMode));
    break;

  case ButtonManager::BUTTON_LONG_PRESS:
    Serial.println("Long Press - Entering Deep Sleep");
    enterDeepSleep();
    break;

  default:
    Serial.println("Unknown");
    break;
  }
}

void updateModemOwnership() {
  static unsigned long lastOwnershipCheck = 0;
  const unsigned long now = millis();
  if (now - lastOwnershipCheck < 100) {
    return;
  }
  lastOwnershipCheck = now;

  const bool usbActive =
      (now - lastUsbActivity) < MODEM_OWNERSHIP_TIMEOUT_MS;
  const bool btActive =
      (now - lastBluetoothActivity) < MODEM_OWNERSHIP_TIMEOUT_MS;

  ModemOwner newOwner = OWNER_NONE;
  if (usbActive) {
    newOwner = OWNER_USB;
  } else if (btActive && currentMode == MODE_BLUETOOTH) {
    newOwner = OWNER_BLUETOOTH;
  }

  if (newOwner == modemOwner) {
    return;
  }

  modemOwner = newOwner;
  Serial.printf("[MODEM] Owner changed to: ");
  switch (modemOwner) {
  case OWNER_NONE:
    Serial.println("NONE");
    break;
  case OWNER_USB:
    Serial.println("USB");
    break;
  case OWNER_BLUETOOTH:
    Serial.println("BLUETOOTH");
    break;
  case OWNER_WIFI:
    Serial.println("WIFI");
    break;
  }
}

bool canTransmitFrom(ModemOwner owner) {
  return modemOwner == OWNER_NONE || modemOwner == owner;
}

void handleUsbData() {
  while (Serial.available()) {
    const uint8_t byte = static_cast<uint8_t>(Serial.read());
    lastUsbActivity = millis();

    if (!canTransmitFrom(OWNER_USB)) {
      continue;
    }

    hartBridge.write(byte);
    modemOwner = OWNER_USB;
    lastUsbActivity = millis();
  }
}

void handleBluetoothData() {
  if (currentMode != MODE_BLUETOOTH || !bluetoothManager.isConnected()) {
    return;
  }

  while (bluetoothManager.available()) {
    const uint8_t byte = bluetoothManager.read();
    lastBluetoothActivity = millis();

    if (!canTransmitFrom(OWNER_BLUETOOTH)) {
      continue;
    }

    hartBridge.write(byte);
    modemOwner = OWNER_BLUETOOTH;
    lastBluetoothActivity = millis();
  }
}

void handleHartData() {
  while (hartBridge.available()) {
    const uint8_t byte = hartBridge.read();
    lastHartActivity = millis();
    ledManager.setHartPulse();

    Serial.write(byte);

    if (currentMode == MODE_BLUETOOTH && bluetoothManager.isConnected()) {
      bluetoothManager.write(byte);
    }
  }
}

void updateLedStatus() {
  if (ledManager.isBatteryStatusShowing() || batteryManager.isLowBattery()) {
    return;
  }

  if (currentMode == MODE_BLUETOOTH) {
    if (bluetoothManager.isConnected()) {
      ledManager.setRgbColor(COLOR_BLUE);
    } else {
      ledManager.setRgbAlternating(COLOR_RED, COLOR_BLUE,
                                   LED_ALTERNATE_INTERVAL_MS);
    }
  } else if (wifiDashboard.hasClient()) {
    ledManager.setRgbColor(COLOR_GREEN);
  } else {
    ledManager.setRgbAlternating(COLOR_RED, COLOR_GREEN,
                                 LED_ALTERNATE_INTERVAL_MS);
  }

  if (!ledManager.isHartPulsing()) {
    if (hartBridge.isCarrierDetected()) {
      ledManager.setHartColor(COLOR_GREEN);
    } else {
      ledManager.setHartColor(COLOR_RED);
    }
  }
}

DashboardSnapshot buildDashboardSnapshot() {
  DashboardSnapshot snap;
  snap.mode = currentMode;
  snap.btConnected = bluetoothManager.isConnected();
  snap.wifiClientConnected = wifiDashboard.hasClient();
  snap.batteryVoltage = batteryManager.getVoltage();
  snap.batteryPercent = batteryManager.getPercentage();
  snap.uptimeSec = (millis() - bootTime) / 1000;
  snap.hartCarrier = hartBridge.isCarrierDetected();
  snap.hartTxBytes = hartBridge.getTxBytes();
  snap.hartRxBytes = hartBridge.getRxBytes();

  if (hartBridge.getLastActivityMs() == 0) {
    snap.hartLastActivitySecAgo = UINT32_MAX;
  } else {
    snap.hartLastActivitySecAgo =
        (millis() - hartBridge.getLastActivityMs()) / 1000;
  }

  snap.freeHeap = ESP.getFreeHeap();
  snap.wifiRssi = WiFi.RSSI();
  snap.cpuFreqMhz = ESP.getCpuFreqMHz();
  snap.macAddress = WiFi.macAddress();
  snap.chipRevision = ESP.getChipRevision();
  snap.chipTempC = readChipTemperatureC();
  return snap;
}

void checkAutoSleep() {
  const unsigned long now = millis();
  const bool usbActive = (now - lastUsbActivity) < AUTO_SLEEP_TIMEOUT_MS;
  const bool btConnected =
      currentMode == MODE_BLUETOOTH && bluetoothManager.isConnected();
  const bool wifiConnected =
      currentMode == MODE_WIFI && wifiDashboard.hasClient();
  const bool hartActive = (now - lastHartActivity) < AUTO_SLEEP_TIMEOUT_MS;

  if (usbActive) {
    return;
  }

  if (!btConnected && !wifiConnected && !hartActive) {
    Serial.println("[SLEEP] Auto-sleep timeout reached - entering deep sleep");
    enterDeepSleep();
  }
}

void enterDeepSleep() {
  const uint32_t uptimeSeconds = (millis() - bootTime) / 1000;
  const uint32_t totalUptime =
      preferences.getUInt(PREF_KEY_UPTIME, 0) + uptimeSeconds;
  preferences.putUInt(PREF_KEY_UPTIME, totalUptime);

  const uint32_t sessionBytes =
      hartBridge.getTxBytes() + hartBridge.getRxBytes();
  const uint32_t lifetimeBytes =
      preferences.getUInt(PREF_KEY_HART_BYTES, 0) + sessionBytes;
  preferences.putUInt(PREF_KEY_HART_BYTES, lifetimeBytes);

  bluetoothManager.end();
  wifiDashboard.end();
  ledManager.setRgbColor(COLOR_OFF);
  ledManager.setHartColor(COLOR_OFF);

  Serial.println("[SLEEP] Powering down - press button to wake");
  Serial.flush();
  delay(100);

  esp_deep_sleep_start();
}
