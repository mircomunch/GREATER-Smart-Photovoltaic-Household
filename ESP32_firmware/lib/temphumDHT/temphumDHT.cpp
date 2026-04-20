// ============================================================================
// temphumDHT.cpp
// ----------------------------------------------------------------------------
// This module provides an abstraction layer for reading temperature and humidity
// data from a DHT11 sensor. It wraps the Adafruit DHT Unified Sensor API into a
// compact class (DHTsensor) used in the main firmware.
//
// The class exposes two main functions:
//   - readTemperature(): returns ambient temperature in Celsius
//   - readHumidity(): returns relative humidity in percentage
// ----------------------------------------------------------------------------
// This code has been made available as part of the "Smart Photovoltaic Household system"
// Do-It-Yourself Laboratory Kit of GREATER Project.
// The GREATER project has received funding from the European Union’s ERASMUS+ CBHE
// (Capacity Building in Higher Education) Programme.
// 
// Project grant agreement: 101083081-GREATER — ERASMUS-EDU-2022-CBHE
// Project website: https://www.greaterproject.eu/
// GitHub repository: https://github.com/mircomunch/GREATER-Smart-Photovoltaic-Household
// Author: Mirco Mongilli - Università di Parma (Italy) 
// ============================================================================

#include <temphumDHT.h>

// -----------------------------------------------------------------------------
// Constructor
// Initializes the DHT unified driver on the specified GPIO pin using the
// DHT11 temperature/humidity sensor type.
// -----------------------------------------------------------------------------
// DHTsensor::DHTsensor(int pin, int type) : dhtu(this->_pin, this->_type)
// {
//     this->_pin = pin;
//     this->_type = type;
// }
DHTsensor::DHTsensor(int pin, int type)
    : dhtu(pin, type), _pin(pin), _type(type)
{
    this->dhtu.begin();
}

// -----------------------------------------------------------------------------
// readTemperature()
// Reads the temperature from the DHT11 sensor using the unified sensor API.
// The function retrieves the latest sensor event, extracts the temperature
// field, and returns it as a floating-point value.
// -----------------------------------------------------------------------------
float DHTsensor::readTemperature()
{
    sensors_event_t event;

    // Request the latest temperature measurement
    this->dhtu.temperature().getEvent(&event);

    // Temperature is provided in °C
    int temperature = event.temperature;

    // Serial.printf("Temperature: %2d °C\n", temperature);

    return temperature;
}

// -----------------------------------------------------------------------------
// readHumidity()
// Reads the relative humidity value from the DHT11 sensor.
// Similar to readTemperature(), this function queries the unified sensor event
// structure and retrieves the humidity field.
// -----------------------------------------------------------------------------
float DHTsensor::readHumidity()
{
    sensors_event_t event;

    // Request the latest humidity measurement
    this->dhtu.humidity().getEvent(&event);

    // Relative humidity (%)
    float humidity = event.relative_humidity;

    // Serial.printf("Humidity: %.1d %%\n", humidity);

    return humidity;
}
