#include "KY038.h"

SENSOR::SENSOR(uint8_t pin) : pin(pin)
{
    pinMode(pin, INPUT);
}

int SENSOR::read()
{
    return digitalRead(pin);
}

float SENSOR::getPercentage(int SENSE = 20)
{
    int noise = 0;
    int buffer = 0;
    for (int i = 0; i < SENSE; i++)
    {
        noise = SENSOR::read();
        buffer += noise;
        delay(10);
    }
    return ((float)buffer / (float)SENSE) * 100;
}
