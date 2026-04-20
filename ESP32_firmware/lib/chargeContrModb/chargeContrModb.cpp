// ============================================================================
// chargeContrModb.cpp
// ----------------------------------------------------------------------------
// This module implements the SolarModbus class, which provides a Modbus RTU
// communication interface over RS485 for a solar charge controller (e.g. EPEVER
// Tracer series).
//
// Responsibilities:
//   - Configure RS485 direction control (RE/DE pin)
//   - Initialize Modbus RTU communication
//   - Provide wrapped Modbus read/write operations with retry logic
//   - Decode Modbus exception codes into human-readable messages
//   - Handle transmission direction switching transparently
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

#include <chargeContrModb.h>

// Static member used by Modbus callbacks for RS485 direction control
uint8_t SolarModbus::_pin_re_de;

// -----------------------------------------------------------------------------
// Constructor
// Initializes RS485 pins, serial interface, and Modbus node configuration.
// -----------------------------------------------------------------------------
SolarModbus::SolarModbus(uint8_t pin_re_de, uint8_t pin_di, uint8_t pin_ro,
                         int nIter, uint8_t modbus_id, int serial_speed,
                         Stream &serial, HardwareSerial &serial_stream)
{
    this->_pin_re_de = pin_re_de;
    this->_nIter = nIter;               // Number of retry attempts
    this->_pin_di = pin_di;             // UART TX pin
    this->_pin_ro = pin_ro;             // UART RX pin
    this->_modbus_id = modbus_id;       // Slave ID
    this->_serial_speed = serial_speed; // Baud rate
    this->_serial = serial;             // Stream abstraction
    this->_serial_stream = serial_stream;

    // Configure RS485 RE/DE pin
    pinMode(_pin_re_de, OUTPUT);
    digitalWrite(_pin_re_de, LOW);      // Default to receive mode

    // Initialize UART for Modbus RTU
    _serial_stream.begin(_serial_speed, SERIAL_8N1, _pin_ro, _pin_di);

    // Initialize Modbus node
    this->node.begin(_modbus_id, _serial);

    // Register callbacks to control RS485 direction
    this->node.preTransmission(setPin);
    this->node.postTransmission(resetPin);

#if defined(SERIAL_DEBUG)
    SERIAL_DEBUG.begin(SERIAL_DEBUG_SPEED);
#endif
}

// -----------------------------------------------------------------------------
// exceptionDescription()
// Converts Modbus exception codes into readable strings.
// -----------------------------------------------------------------------------
String SolarModbus::exceptionDescription(uint8_t exception)
{
    String exceptionDescription = "";
    switch (exception)
    {
    case this->node.ku8MBSuccess:
        exceptionDescription = "";
        break;
    case this->node.ku8MBIllegalFunction:
        exceptionDescription = "Illegal function";
        break;
    case this->node.ku8MBIllegalDataAddress:
        exceptionDescription = "Illegal address";
        break;
    case this->node.ku8MBIllegalDataValue:
        exceptionDescription = "Illegal data";
        break;
    case this->node.ku8MBSlaveDeviceFailure:
        exceptionDescription = "Device failure";
        break;
    case this->node.ku8MBInvalidSlaveID:
        exceptionDescription = "Invalid ID";
        break;
    case this->node.ku8MBInvalidFunction:
        exceptionDescription = "Invalid function";
        break;
    case this->node.ku8MBResponseTimedOut:
        exceptionDescription = "Response timeout";
        break;
    case this->node.ku8MBInvalidCRC:
        exceptionDescription = "Invalid CRC";
        break;
    default:
        exceptionDescription = "Exception occurred";
        break;
    }
    return exceptionDescription;
}

// -----------------------------------------------------------------------------
// exceptionHandler()
// Prints Modbus error information when DEBUG is enabled.
// -----------------------------------------------------------------------------
void SolarModbus::exceptionHandler(uint8_t result, String mode, String address)
{
    if (result != 0)
    {
#ifdef DEBUG
        SERIAL_DEBUG.println(mode + " (" + address + "): " + this->exceptionDescription(result));
#endif
    }
}

// -----------------------------------------------------------------------------
// writeMultipleRegisters() - Modbus function 0x10
// Writes multiple holding registers with retry logic.
// -----------------------------------------------------------------------------
uint8_t SolarModbus::writeMultipleRegisters(uint16_t *data, uint16_t address, uint8_t nReg)
{
    uint8_t result = 255;
    int iter = 0;

    while (result != 0 && iter < _nIter)
    {
        for (int i = 0; i < sizeof(data) / sizeof(uint16_t); i++)
        {
            this->node.setTransmitBuffer(i, data[i]);
        }
        result = this->node.writeMultipleRegisters(address, nReg);
        iter++;
    }
    return result;
}

// -----------------------------------------------------------------------------
// writeSingleCoil() - Modbus function 0x05
// Writes a single coil (ON/OFF) with retry logic.
// -----------------------------------------------------------------------------
uint8_t SolarModbus::writeSingleCoil(uint16_t data, uint16_t address)
{
    uint8_t result = 255;
    int iter = 0;

    while (result != 0 && iter < _nIter)
    {
        result = this->node.writeSingleCoil(address, data);
        iter++;
    }
    return result;
}

// -----------------------------------------------------------------------------
// readCoils() - Modbus function 0x01
// Reads discrete coil states.
// -----------------------------------------------------------------------------
uint8_t SolarModbus::readCoils(uint16_t address, void *variable)
{
    uint8_t result = 255;
    int iter = 0;

    while (result != 0 && iter < _nIter)
    {
        result = this->node.readCoils(address, 1);
        iter++;
    }

    if (result == this->node.ku8MBSuccess)
    {
        uint16_t value = this->node.getResponseBuffer(0);
        memcpy(variable, &value, sizeof(variable));
    }
    return result;
}

// -----------------------------------------------------------------------------
// readHoldingRegisters() - Modbus function 0x03
// Reads holding registers.
// -----------------------------------------------------------------------------
uint8_t SolarModbus::readHoldingRegisters(uint16_t address, void *variable, uint8_t nReg)
{
    uint8_t result = 255;
    int iter = 0;

    while (result != 0 && iter < _nIter)
    {
        result = this->node.readHoldingRegisters(address, nReg);
        iter++;
    }

    if (result == this->node.ku8MBSuccess)
    {
        uint16_t value = this->node.getResponseBuffer(0);
        memcpy(variable, &value, sizeof(variable));
    }
    return result;
}

// -----------------------------------------------------------------------------
// readInputRegisters() - Modbus function 0x04
// Reads multi-register input values (e.g. power, energy).
// -----------------------------------------------------------------------------
uint8_t SolarModbus::readInputRegisters(uint16_t address, void *variable,
                                       uint8_t nReg, bool oneHundredTimes)
{
    uint8_t result = 255;
    int iter = 0;

    while (result != 0 && iter < _nIter)
    {
        result = this->node.readInputRegisters(address, nReg);
        iter++;
    }

    if (result == this->node.ku8MBSuccess)
    {
        uint64_t value = 0x00;
        for (int i = 0; i < nReg; i++)
        {
            value = value | node.getResponseBuffer(i) << 16;
        }
        memcpy(variable, &value, sizeof(variable));
    }
    return result;
}

// -----------------------------------------------------------------------------
// readSingleInputRegisters() - Modbus function 0x04
// Reads a single input register and applies optional scaling.
// -----------------------------------------------------------------------------
uint8_t SolarModbus::readSingleInputRegisters(uint16_t address, void *variable,
                                             bool oneHundredTimes)
{
    uint8_t result = 255;
    int iter = 0;

    while (result != 0 && iter < _nIter)
    {
        result = this->node.readInputRegisters(address, 1);
        iter++;
    }

    if (result == this->node.ku8MBSuccess)
    {
        float value = (oneHundredTimes)
                          ? node.getResponseBuffer(0) / 100.0f
                          : node.getResponseBuffer(0);
        value = roundf(value * 100) / 100;
        memcpy(variable, &value, sizeof(variable));
    }
    return result;
}

// -----------------------------------------------------------------------------
// readDiscreteInputs() - Modbus function 0x02
// Reads discrete input status flags.
// -----------------------------------------------------------------------------
uint8_t SolarModbus::readDiscreteInputs(uint16_t address, void *variable)
{
    uint8_t result = 255;
    int iter = 0;

    while (result != 0 && iter < this->_nIter)
    {
        result = this->node.readDiscreteInputs(address, 1);
        iter++;
    }

    if (result == this->node.ku8MBSuccess)
    {
        uint16_t value = this->node.getResponseBuffer(0);
        memcpy(variable, &value, sizeof(variable));
    }
    return result;
}

// -----------------------------------------------------------------------------
// RS485 direction control callbacks
// -----------------------------------------------------------------------------
void SolarModbus::setPin()
{
    digitalWrite(_pin_re_de, HIGH); // Enable transmit
}

void SolarModbus::resetPin()
{
    digitalWrite(_pin_re_de, LOW);  // Enable receive
}
