#ifndef BUTTON_PULLDOWN_H
#define BUTTON_PULLDOWN_H

#include <Arduino.h>

class PulldownButton {
private:
  int pin;
  int state;
  int lastState = LOW;
  bool pressedFlag = false;
  unsigned long lastClickTime = 0;
  unsigned long debounceTime = 50;

public:
  PulldownButton(int pin);
  void update();
  void begin();
  int getState();
  bool wasPressed(); 
};

#endif