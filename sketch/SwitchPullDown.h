#ifndef SWITCH_PULLDOWN_H
#define SWITCH_PULLDOWN_H

#include <Arduino.h>

class SwitchPullDown {
private:
  int pin;
  int state;
  int lastState = LOW;
  bool changedFlag = false;
  unsigned long lastDebounceTime = 0;
  unsigned long debounceTime = 50;

public:
  SwitchPullDown(int pin);
  void begin();
  void update();
  bool isOn();
  bool hasChanged();
};

#endif