#ifndef RGB_LED_H
#define RGB_LED_H

#include <Arduino.h>

#define RGB_PWM_FREQ  5000
#define RGB_PWM_RES   8
#define RGB_INIT_VAL  0
#define RGB_MAX_VAL   255
#define RGB_MIN_VAL   0
#define RGB_LOW_VAL   0

class RGBLed {
private:
  int pinR;
  int pinG;
  int pinB;
  int pinCommon;

  void setupPWM(int pin);

public:
  RGBLed(int redPin, int greenPin, int bluePin);
  void begin();
  void setRed(int value);
  void setGreen(int value);
  void setBlue(int value);
  void setColor(int r, int g, int b);
  void clear();
};

#endif