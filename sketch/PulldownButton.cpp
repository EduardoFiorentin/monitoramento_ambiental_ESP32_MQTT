#include "PulldownButton.h"

PulldownButton::PulldownButton(int pin) {
  this->pin = pin;
}
void PulldownButton::begin() {
  pinMode(this->pin, INPUT); 
}

void PulldownButton::update() {
  int newRead = digitalRead(pin);

  // Se o estado mudou devido a ruído ou pressionamento
  if (newRead != lastState) {
    lastClickTime = millis();
  }

  // Verifica se o tempo de debounce já passou
  if ((millis() - lastClickTime) > debounceTime) {
    // Se a leitura for realmente diferente do estado atual
    if (newRead != state) {
      state = newRead;
      
      // Na lógica pull-down, o pressionamento resulta em sinal HIGH
      if (state == HIGH) {
        pressedFlag = true;
      }
    }
  }

  lastState = newRead;
}

int PulldownButton::getState() {
  return state;
}

bool PulldownButton::wasPressed() {
  if (pressedFlag) {
    pressedFlag = false;
    return true;
  }
  return false;
}