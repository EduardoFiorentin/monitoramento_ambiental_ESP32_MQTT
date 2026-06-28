#include "RGBLed.h"

RGBLed::RGBLed(int redPin, int greenPin, int bluePin) {
  this->pinR = redPin;
  this->pinG = greenPin;
  this->pinB = bluePin;
}

void RGBLed::begin() {
  setupPWM(this->pinR);
  setupPWM(this->pinG);
  setupPWM(this->pinB);
}

void RGBLed::setupPWM(int pin) {
  ledcAttach(pin, RGB_PWM_FREQ, RGB_PWM_RES);
  ledcWrite(pin, RGB_INIT_VAL); 
}

void RGBLed::setRed(int value) {
  ledcWrite(this->pinR, constrain(value, RGB_MIN_VAL, RGB_MAX_VAL));
}

void RGBLed::setGreen(int value) {
  ledcWrite(this->pinG, constrain(value, RGB_MIN_VAL, RGB_MAX_VAL));
}

void RGBLed::setBlue(int value) {
  ledcWrite(this->pinB, constrain(value, RGB_MIN_VAL, RGB_MAX_VAL));
}

void RGBLed::setColor(int r, int g, int b) {
  setRed(r);
  setGreen(g);
  setBlue(b);
}

void RGBLed::clear() {
  setRed(RGB_LOW_VAL);
  setGreen(RGB_LOW_VAL);
  setBlue(RGB_LOW_VAL);
}