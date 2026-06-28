#ifndef SIMPLE_LED_H
#define SIMPLE_LED_H

#include <Arduino.h>

class SimpleLed {
public:
    /**
     * @param pin O pino digital onde o LED está conectado
     */
    SimpleLed(int pin);

    // Inicialização do hardware
    void begin();

    void setOn();
    void setOff();
    void toggle();

    bool isOn() const;

private:
    int _pin;
    bool _state;
};

#endif // SIMPLE_LED_H