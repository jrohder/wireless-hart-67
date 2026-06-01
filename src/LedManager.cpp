#include "LedManager.h"

LedManager::LedManager()
    : rgbCurrentColor(COLOR_OFF), rgbBrightness(RGB_LED_BRIGHTNESS),
      rgbAnimationInterval(0), rgbLastToggleTime(0), rgbAnimColor1(COLOR_OFF),
      rgbAnimColor2(COLOR_OFF), rgbAnimating(false), rgbAlternating(false),
      hartCurrentColor(COLOR_OFF), hartBrightness(HART_LED_BRIGHTNESS),
      hartPulsing(false), hartPulseStartTime(0), batteryStatusActive(false),
      batteryStatusStartTime(0) {}

void LedManager::begin() {
  // Configure RGB LED pins as outputs
  pinMode(RGB_LED_RED, OUTPUT);
  pinMode(RGB_LED_GREEN, OUTPUT);
  pinMode(RGB_LED_BLUE, OUTPUT);

  // Configure HART LED pins as outputs
  pinMode(HART_LED_RED, OUTPUT);
  pinMode(HART_LED_GREEN, OUTPUT);

  // Set LEDC PWM for RGB LED (50% = 128/255)
  ledcSetup(0, 5000, 8);  // Channel 0, 5kHz, 8-bit
  ledcSetup(1, 5000, 8);  // Channel 1
  ledcSetup(2, 5000, 8);  // Channel 2
  ledcAttachPin(RGB_LED_RED, 0);
  ledcAttachPin(RGB_LED_GREEN, 1);
  ledcAttachPin(RGB_LED_BLUE, 2);

  // Set LEDC PWM for HART LED
  ledcSetup(3, 5000, 8);  // Channel 3
  ledcSetup(4, 5000, 8);  // Channel 4
  ledcAttachPin(HART_LED_RED, 3);
  ledcAttachPin(HART_LED_GREEN, 4);

  // Initialize all LEDs to off
  setRgbColor(COLOR_OFF);
  setHartColor(COLOR_OFF);
}

void LedManager::update() {
  unsigned long now = millis();

  // Battery status override active
  if (batteryStatusActive) {
    if (now - batteryStatusStartTime > LED_BATTERY_DISPLAY_MS) {
      clearBatteryStatus();
    }
    return;
  }

  updateRgb();
  updateHart();
}

void LedManager::setRgbColor(LedColor color) {
  rgbCurrentColor = color;
  rgbAnimating = false;
  rgbAlternating = false;
  applyColor(RGB_LED_RED, RGB_LED_GREEN, RGB_LED_BLUE, color,
             rgbBrightness);
}

void LedManager::setRgbBrightness(uint8_t brightness) {
  rgbBrightness = brightness;
}

void LedManager::setRgbAlternating(LedColor color1, LedColor color2,
                                    unsigned long intervalMs) {
  rgbAnimColor1 = color1;
  rgbAnimColor2 = color2;
  rgbAnimationInterval = intervalMs;
  rgbAnimating = true;
  rgbAlternating = true;
  rgbLastToggleTime = millis();
  applyColor(RGB_LED_RED, RGB_LED_GREEN, RGB_LED_BLUE, color1, rgbBrightness);
}

void LedManager::setRgbFlashing(LedColor color, unsigned long intervalMs) {
  rgbAnimColor1 = color;
  rgbAnimColor2 = COLOR_OFF;
  rgbAnimationInterval = intervalMs;
  rgbAnimating = true;
  rgbAlternating = false;
  rgbLastToggleTime = millis();
}

void LedManager::stopRgbAnimation() {
  rgbAnimating = false;
  rgbAlternating = false;
  setRgbColor(COLOR_OFF);
}

void LedManager::setHartColor(LedColor color) {
  hartCurrentColor = color;
  hartPulsing = false;
  applyColor(HART_LED_RED, HART_LED_GREEN, 0xFF, color, hartBrightness);
}

void LedManager::setHartBrightness(uint8_t brightness) {
  hartBrightness = brightness;
}

void LedManager::setHartPulse() {
  hartPulsing = true;
  hartPulseStartTime = millis();
  hartCurrentColor = COLOR_GREEN;
}

void LedManager::stopHartPulse() {
  hartPulsing = false;
  setHartColor(COLOR_GREEN);
}

void LedManager::showBatteryStatus(uint8_t percentage) {
  batteryStatusActive = true;
  batteryStatusStartTime = millis();

  LedColor color;
  if (percentage > 70) {
    color = COLOR_BLUE;
  } else if (percentage > 30) {
    color = COLOR_GREEN;
  } else {
    color = COLOR_RED;
  }

  applyColor(RGB_LED_RED, RGB_LED_GREEN, RGB_LED_BLUE, color, rgbBrightness);
}

void LedManager::clearBatteryStatus() {
  batteryStatusActive = false;
  rgbCurrentColor = COLOR_OFF;
  applyColor(RGB_LED_RED, RGB_LED_GREEN, RGB_LED_BLUE, COLOR_OFF,
             rgbBrightness);
}

void LedManager::setPin(uint8_t pin, bool active) {
  // Common anode = active LOW
  digitalWrite(pin, active ? LOW : HIGH);
}

void LedManager::updateRgb() {
  if (!rgbAnimating) return;

  unsigned long now = millis();
  if (now - rgbLastToggleTime >= rgbAnimationInterval) {
    rgbLastToggleTime = now;

    if (rgbAlternating) {
      // Swap between two colors
      LedColor temp = rgbAnimColor1;
      rgbAnimColor1 = rgbAnimColor2;
      rgbAnimColor2 = temp;
      applyColor(RGB_LED_RED, RGB_LED_GREEN, RGB_LED_BLUE, rgbAnimColor1,
                 rgbBrightness);
    } else {
      // Flash: toggle between color and off
      if (rgbCurrentColor == COLOR_OFF) {
        applyColor(RGB_LED_RED, RGB_LED_GREEN, RGB_LED_BLUE, rgbAnimColor1,
                   rgbBrightness);
        rgbCurrentColor = rgbAnimColor1;
      } else {
        applyColor(RGB_LED_RED, RGB_LED_GREEN, RGB_LED_BLUE, COLOR_OFF,
                   rgbBrightness);
        rgbCurrentColor = COLOR_OFF;
      }
    }
  }
}

void LedManager::updateHart() {
  if (!hartPulsing) return;

  unsigned long elapsed = millis() - hartPulseStartTime;
  if (elapsed > LED_HART_ACTIVITY_PULSE_MS) {
    hartPulsing = false;
    applyColor(HART_LED_RED, HART_LED_GREEN, 0xFF, hartCurrentColor,
               hartBrightness);
  }
}

void LedManager::applyColor(uint8_t redPin, uint8_t greenPin, uint8_t bluePin,
                             LedColor color, uint8_t brightness) {
  // Common anode: 0xFF (255) = OFF, 0x00 (0) = ON at full brightness
  // For PWM, invert: pwmValue = 255 - (brightness * colorComponent / 255)

  uint8_t red = 255, green = 255, blue = 255;

  switch (color) {
  case COLOR_OFF:
    red = green = blue = 255;
    break;
  case COLOR_RED:
    red = 255 - brightness;
    break;
  case COLOR_GREEN:
    green = 255 - brightness;
    break;
  case COLOR_BLUE:
    blue = 255 - brightness;
    break;
  case COLOR_YELLOW:  // Red + Green
    red = 255 - brightness;
    green = 255 - brightness;
    break;
  case COLOR_CYAN:  // Green + Blue
    green = 255 - brightness;
    blue = 255 - brightness;
    break;
  case COLOR_MAGENTA:  // Red + Blue
    red = 255 - brightness;
    blue = 255 - brightness;
    break;
  case COLOR_WHITE:  // All
    red = green = blue = 255 - brightness;
    break;
  }

  ledcWrite(0, red);    // RGB Red channel
  ledcWrite(1, green);  // RGB Green channel
  ledcWrite(2, blue);   // RGB Blue channel

  // Handle HART LED (only 2 colors: red and green, no blue)
  if (redPin == HART_LED_RED) {
    ledcWrite(3, red);    // HART Red channel
    ledcWrite(4, green);  // HART Green channel
  }
}
