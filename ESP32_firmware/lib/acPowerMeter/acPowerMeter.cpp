// ============================================================================
// acPowerMeter.cpp
// ----------------------------------------------------------------------------
// This module implements the PowerReader class, which interfaces with a JSY-MK
// AC power meter using Modbus RTU over UART. It provides high-level methods for
// reading electrical quantities from one or two measurement channels.
//
// Functionalities:
//   - Initialize UART and Modbus communication
//   - Verify correct device presence via test register
//   - Read voltage, current, power, energy, power factor, direction, frequency
//   - Apply scaling factors defined by the JSY-MK Modbus register map
//   - Retry Modbus transactions to improve robustness
//   - Translate Modbus exception codes into human-readable descriptions
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

#include <acPowerMeter.h>

// -----------------------------------------------------------------------------
// Constructor
// Stores configuration parameters for later initialization.
// No serial communication is started here.
// -----------------------------------------------------------------------------
PowerReader::PowerReader(uint8_t txPin, uint8_t rxPin, uint32_t baudRate,
                         uint8_t slaveID, int nIter,
                         Stream &serial, HardwareSerial &serial_stream)
{
    this->_nIter = nIter;               // Number of Modbus retry attempts
    this->_txPin = txPin;               // UART TX pin
    this->_rxPin = rxPin;               // UART RX pin
    this->_slaveID = slaveID;           // Modbus slave ID
    this->_baudRate = baudRate;         // UART baud rate
    this->_serial = serial;             // Stream abstraction
    this->_serialPort = serial_stream;  // Hardware serial port
}

// -----------------------------------------------------------------------------
// begin()
// Initializes the UART interface and Modbus node.
// Must be called once during system setup.
// -----------------------------------------------------------------------------
void PowerReader::begin()
{
    // Initialize UART with given pins and baud rate
    this->_serialPort.begin(_baudRate, SERIAL_8N1, _rxPin, _txPin);

    // Initialize Modbus RTU node
    this->_modbusNode.begin(_slaveID, _serial);
}

// -----------------------------------------------------------------------------
// exceptionDescription()
// Translates Modbus exception codes into readable text.
// -----------------------------------------------------------------------------
String PowerReader::exceptionDescription(uint8_t exception)
{
    String exceptionDescription = "";
    switch (exception)
    {
    case this->_modbusNode.ku8MBSuccess:
        exceptionDescription = "";
        break;
    case this->_modbusNode.ku8MBIllegalFunction:
        exceptionDescription = "Illegal function";
        break;
    case this->_modbusNode.ku8MBIllegalDataAddress:
        exceptionDescription = "Illegal address";
        break;
    case this->_modbusNode.ku8MBIllegalDataValue:
        exceptionDescription = "Illegal data";
        break;
    case this->_modbusNode.ku8MBSlaveDeviceFailure:
        exceptionDescription = "Device failure";
        break;
    case this->_modbusNode.ku8MBInvalidSlaveID:
        exceptionDescription = "Invalid ID";
        break;
    case this->_modbusNode.ku8MBInvalidFunction:
        exceptionDescription = "Invalid function";
        break;
    case this->_modbusNode.ku8MBResponseTimedOut:
        exceptionDescription = "Response timeout";
        break;
    case this->_modbusNode.ku8MBInvalidCRC:
        exceptionDescription = "Invalid CRC";
        break;
    default:
        exceptionDescription = "Exception occurred";
        break;
    }
    return exceptionDescription;
}

// -----------------------------------------------------------------------------
// readModbusRegister()
// Generic helper to read a 32-bit value from two holding registers and scale it.
// -----------------------------------------------------------------------------
uint8_t PowerReader::readModbusRegister(uint16_t reg, float divisor, float *variable)
{
    uint8_t result = 255;
    int iter = 0;

    // Retry logic
    while (result != 0 && iter < this->_nIter)
    {
        result = this->_modbusNode.readHoldingRegisters(reg, 2);
        iter++;
    }

    // Successful read
    if (result == this->_modbusNode.ku8MBSuccess)
    {
        uint32_t rawData = (this->_modbusNode.getResponseBuffer(0) << 16) |
                           this->_modbusNode.getResponseBuffer(1);
        float value = rawData / divisor;
        memcpy(variable, &value, sizeof(variable));
    }
    return result;
}

// -----------------------------------------------------------------------------
// test()
// Reads a known identification register to verify correct device presence.
// Expected value: 0x0194 (decimal 404)
// -----------------------------------------------------------------------------
uint8_t PowerReader::test()
{
    uint8_t result = 255;
    int iter = 0;
    uint16_t variable;

    while (result != 0 && iter < this->_nIter)
    {
        result = this->_modbusNode.readHoldingRegisters(0x0000, 1);
        iter++;
    }

    if (result == this->_modbusNode.ku8MBSuccess)
    {
        uint16_t value = this->_modbusNode.getResponseBuffer(0);
        memcpy(&variable, &value, sizeof(variable));
    }

    // Validate expected identifier
    if (variable != 404)
    {
        result = 255;
    }
    return result;
}

// -----------------------------------------------------------------------------
// readVoltage()
// Reads RMS voltage for selected channel.
// -----------------------------------------------------------------------------
uint8_t PowerReader::readVoltage(int channel, float *variable)
{
    uint8_t result = 254;
    if (channel == 1)
        result = readModbusRegister(0x0048, 10000.0, variable);
    else if (channel == 2)
        result = readModbusRegister(0x0050, 10000.0, variable);
    return result;
}

// -----------------------------------------------------------------------------
// readCurrent()
// Reads RMS current for selected channel.
// -----------------------------------------------------------------------------
uint8_t PowerReader::readCurrent(int channel, float *variable)
{
    uint8_t result = 254;
    if (channel == 1)
        result = readModbusRegister(0x0049, 10000.0, variable);
    else if (channel == 2)
        result = readModbusRegister(0x0051, 10000.0, variable);
    return result;
}

// -----------------------------------------------------------------------------
// readPower()
// Reads active power for selected channel.
// -----------------------------------------------------------------------------
uint8_t PowerReader::readPower(int channel, float *variable)
{
    uint8_t result = 254;
    if (channel == 1)
        result = readModbusRegister(0x004A, 10000.0, variable);
    else if (channel == 2)
        result = readModbusRegister(0x0052, 10000.0, variable);
    return result;
}

// -----------------------------------------------------------------------------
// readEnergy()
// Reads accumulated energy for selected channel.
// -----------------------------------------------------------------------------
uint8_t PowerReader::readEnergy(int channel, float *variable)
{
    uint8_t result = 254;
    if (channel == 1)
        result = readModbusRegister(0x004B, 10000.0, variable);
    else if (channel == 2)
        result = readModbusRegister(0x0053, 10000.0, variable);
    return result;
}

// -----------------------------------------------------------------------------
// readPowerfactor()
// Reads power factor for selected channel.
// -----------------------------------------------------------------------------
uint8_t PowerReader::readPowerfactor(int channel, float *variable)
{
    uint8_t result = 254;
    if (channel == 1)
        result = readModbusRegister(0x004C, 1000.0, variable);
    else if (channel == 2)
        result = readModbusRegister(0x0054, 1000.0, variable);
    return result;
}

// -----------------------------------------------------------------------------
// readPowerdirection()
// Reads power flow direction (import/export).
// -----------------------------------------------------------------------------
uint8_t PowerReader::readPowerdirection(float *variable)
{
    return readModbusRegister(0x004E, 1.0, variable);
}

// -----------------------------------------------------------------------------
// readFrequency()
// Reads AC line frequency.
// -----------------------------------------------------------------------------
uint8_t PowerReader::readFrequency(float *variable)
{
    return readModbusRegister(0x004F, 100.0, variable);
}
