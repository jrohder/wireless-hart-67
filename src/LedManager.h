#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>
#include "Config.h"

class LedManager {
public:
  LedManager();
  void begin();
  void update();

  void setRgbColor(LedColor color);
  void setRgbBrightness(uint8_t brightness);
  void setRgbAlternating(LedColor color1, LedColor color2,
                         unsigned long intervalMs);
  void setRgbFlashing(LedColor color, unsigned long intervalMs);
  void stopRgbAnimation();

  void setHartColor(LedColor color);
  void setHartBrightness(uint8_t brightness);
  void setHartPulse();
  void stopHartPulse();

  void showBatteryStatus(uint8_t percentage);
  void clearBatteryStatus();

  bool isBatteryStatusShowing() const { return batteryStatusActive; }
  bool isHartPulsing() const { return hartPulsing; }

private:
  LedColor rgbCurrentColor;
  uint8_t rgbBrightness;
  unsigned long rgbAnimationInterval;
  unsigned long rgbLastToggleTime;
  LedColor rgbAnimColor1, rgbAnimColor2;
  bool rgbAnimating;
  bool rgbAlternating;

  LedColor hartCurrentColor;
  uint8_t hartBrightness;
  bool hartPulsing;
  unsigned long hartPulseStartTime;

  bool batteryStatusActive;
  unsigned long batteryStatusStartTime;

  void setPin(uint8_t pin, bool active);
  void updateRgb();
  void updateHart();
  void applyColor(uint8_t redPin, uint8_t greenPin, uint8_t bluePin,
                  LedColor color, uint8_t brightness);
};

#endif  // LED_MANAGER_H
