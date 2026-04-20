#ifndef TEMPHUMDHT_H
#define TEMPHUMDHT_H

#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT_U.h>

class DHTsensor
{
    DHT_Unified dhtu;
    int _pin;
    int _type;

public:
    DHTsensor(int pin, int type);
    float readTemperature();
    float readHumidity();
};
#endif