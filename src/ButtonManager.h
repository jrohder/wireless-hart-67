#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>
#include "Config.h"

class ButtonManager {
public:
  enum ButtonEvent {
    BUTTON_NONE = 0,
    BUTTON_SINGLE_PRESS = 1,
    BUTTON_TRIPLE_PRESS = 2,
    BUTTON_LONG_PRESS = 3
  };

  ButtonManager();
  void begin();
  void update();
  ButtonEvent getEvent();
  bool isPressed();
  bool canWakeFromSleep();

private:
  bool lastState;
  bool currentState;
  unsigned long pressStartTime;
  unsigned long lastReleaseTime;
  int pressCount;
  ButtonEvent lastEvent;
  unsigned long lastEventTime;
  bool eventConsumed;

  void debounceAndRead();
  void handlePress();
  void handleRelease();
};

#endif  // BUTTON_MANAGER_H
