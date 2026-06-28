#include "SimpleLed.h"

SimpleLed::SimpleLed(int pin) {
  _pin = pin;
  _state = false;
}

void SimpleLed::begin() {
  pinMode(_pin, OUTPUT);
  setOff();
}

void SimpleLed::setOn() {
  _state = true;
  digitalWrite(_pin, HIGH);
}

void SimpleLed::setOff() {
  _state = false;
  digitalWrite(_pin, LOW);
}

void SimpleLed::toggle() {
  if (_state) {
    setOff();
  } else {
    setOn();
  }
}

bool SimpleLed::isOn() const {
  return _state;
}