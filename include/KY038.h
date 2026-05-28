
#ifndef LED_h
#define LED_h
#include <Arduino.h>

class SENSOR{

private:
    uint8_t pin;
    bool type; // true = analogico false = digital
public:
    SENSOR(uint8_t pin);
    int read();
    float getPercentage(int SENSE);
};
#endif