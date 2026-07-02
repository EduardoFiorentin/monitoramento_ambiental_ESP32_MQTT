#include "Timer.h"

Timer::Timer(uint32_t intervalMillis, TimerCallback callback) {
  this->_interval = intervalMillis;
  this->_previousMillis = 0;
  this->_callback = callback;
  this->_isRunning = false;
}

void Timer::start() {
  _isRunning = true;
  _previousMillis = millis();
}

void Timer::stop() {
  _isRunning = false;
}

void Timer::update() {
  // Fail-fast
  if (!_isRunning || _callback == nullptr) {
    return;
  }

  uint32_t currentMillis = millis();

  // A subtração com unsigned int garante que o reset (overflow) do millis()
  // seja tratado automaticamente.
  if (currentMillis - _previousMillis >= _interval) {
    // Atualiza o tempo de referência garantindo que pequenos atrasos
    // no loop não acumulem erro de drift ao longo do tempo.
    _previousMillis = currentMillis; 
    _callback();
  }
}

void Timer::reset() {
  _previousMillis = millis(); 
}

void Timer::setInterval(uint32_t intervalMillis) {
  _interval = intervalMillis;
}

void Timer::setCallback(TimerCallback callback) {
  _callback = callback;
}

bool Timer::isRunning() const {
  return _isRunning;
}