#include "SwitchPullDown.h"

SwitchPullDown::SwitchPullDown(int pin) {
  this->pin = pin;
}

void SwitchPullDown::begin() {
  pinMode(this->pin, INPUT);
  this->state = digitalRead(pin);
  this->lastState = this->state;
}

void SwitchPullDown::update() {
  int newRead = digitalRead(pin);

  if (newRead != lastState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceTime) {
    if (newRead != state) {
      state = newRead;
      changedFlag = true;
    }
  }

  lastState = newRead;
}

bool SwitchPullDown::isOn() {
  return (state == HIGH);
}

bool SwitchPullDown::hasChanged() {
  if (changedFlag) {
    changedFlag = false; // Reseta a flag para a próxima mudança
    return true;
  }
  return false;
}