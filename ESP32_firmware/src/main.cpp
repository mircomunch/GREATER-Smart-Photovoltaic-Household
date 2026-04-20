// ============================================================================
// main.cpp
// ----------------------------------------------------------------------------
// This file implements the core firmware logic for the ESP32-based monitoring
// and control system. It handles:
//   - WiFi connectivity and reconnection logic
//   - MQTT communication (publish/subscribe)
//   - Modbus communication with the solar charge controller (Tracer)
//   - Optional Modbus communication with AC power meters (JSY)
//   - Environmental temperature and humidity measurement (DHT)
//   - Timed acquisition and load control using FreeRTOS software timers
//   - JSON payload construction and publication to the MQTT broker
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

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <TimeLib.h>
#include <ArduinoJson.h>

// Project-specific includes
#include <setup.hpp>
#include <MQTT.h>
#include <chargeContrModb.h>
#include <modbusAddress.hpp>
#include <temphumDHT.h>
#include <acPowerMeter.h>

// ============================================================================
// GLOBAL OBJECTS AND INITIALIZATION
// ============================================================================

// Solar charge controller Modbus interface (RS485)
SolarModbus TRACER(PIN_RE_DE, PIN_DI, PIN_RO, READ_TENTATIVE, TRACER_MODBUS_ID,
                   TRACER_SERIAL_STREAM_SPEED, TRACER_SERIAL_STREAM, TRACER_SERIAL_STREAM);

// Convert MQTT host constant to modifiable char array
char _host_char[sizeof(MQTT_HOST)] = MQTT_HOST;

// MQTT transport object with or without authentication
#ifdef MQTT_AUTH
char _host_user[sizeof(MQTT_USER)] = MQTT_USER;
char _host_psw[sizeof(MQTT_PASSWORD)] = MQTT_PASSWORD;
MQTT mqtt(BOARD_ID, _host_char, MQTT_PORT, MQTT_PUBLISH_MESSAGE_MAX_SIZE,
          MQTT_CERT_EN, _host_user, _host_psw);
#else
MQTT mqtt(BOARD_ID, _host_char, MQTT_PORT);
#endif

// Timers for acquisition and load control
TimerHandle_t publishTimer;
TimerHandle_t loadTimer;

// Tracking reconnection backoff for WiFi and MQTT
long mqttReconnectAttempt = 0;
long wifiReconnectAttempt = 0;

// Optional temperature & humidity sensor
#ifdef SENS_TEMPHUM
    DHTsensor dht(DHT_PIN, DHT_TYPE);
#endif

// Optional AC inverter power meter
#ifdef SENS_POWERINV
PowerReader JSY_MK(JSY_PIN_TX, JSY_PIN_RX, JSY_SERIAL_STREAM_SPEED,
                   JSY_MODBUS_ID, READ_TENTATIVE, JSY_SERIAL_STREAM, JSY_SERIAL_STREAM);
#endif

// Control flags (set through MQTT commands and timers)
bool datetimeSetted = false;
bool cmdRun = false;
bool cmdLoadDC = false;
bool cmdLoadAC = false;
bool acq = false;      // Data acquisition trigger
bool control = false;  // Load control trigger

// ============================================================================
// DATA STRUCTURES FOR SENSOR READINGS AND CONTROLLER SETTINGS
// ============================================================================

// Real-time readings from the Tracer charge controller
struct SolarData {
    uint16_t batteryType;
    uint16_t batteryCapacity;
    uint16_t controllerSupply;
    uint16_t batteryRatedVoltageCode;
    uint16_t outputMode;

    uint16_t batteryRatedVoltage;
    uint16_t batteryTemperature;
    float batteryVoltage;
    float batteryCurrent;
    float batterySOC;
    float batteryMaxVoltageToday;
    uint16_t batteryStatus;

    float pvVoltage;
    float pvCurrent;

    float loadVoltage;
    float loadCurrent;
    uint16_t loadPower;
} TracerReadings;

// Configuration values written to the Tracer charge controller at startup
struct SolarSettings {
    uint16_t batteryType[1] = {0x01};
    uint16_t batteryCapacity[1] = {0x01};
    uint16_t controllerSupply[1] = {0x01};
    uint16_t batteryVoltage[1] = {0x01};
    uint16_t outputMode = 0x0001;
    uint16_t loadOn = 0x0001;
    uint16_t loadOff = 0x0000;
} TracerSettings;

// Real-time readings from AC power meter (JSY MK)
struct JSYData {
    float voltage;
    float current;
    float power;
    float energy;
    float powerfactor;
    float powerdirection;
    float frequency;
} JSYReadings;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

void readTracer(JsonArray *readArray);
void setupTracer();
#ifdef SENS_TEMPHUM
void readTempHum(JsonArray *readArray);
#endif
#ifdef SENS_POWERINV
void readPower(JsonArray *readArray);
#endif
void setDatetime(String datetime);
void mqttSubscribeCallback(char *topic, byte *payload, unsigned int length);
void mqttParseMessage(char *topic, const char *payloadStr);
void dataPublishCallback();
void loadControlCallback();

// ============================================================================
// SYSTEM SETUP: initializes hardware, network, timers, and subsystems
// ============================================================================
void set()
{
    #ifdef DEBUG
        SERIAL_DEBUG.begin(SERIAL_DEBUG_SPEED);
    #endif

    // Configure AC relay pin
    pinMode(RELE_PIN, OUTPUT);

    // Start WiFi in station mode
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Create software timers for data publishing and load management
    publishTimer = xTimerCreate("publishTimer", pdMS_TO_TICKS(DATA_TIMER),
                                pdFALSE, (void *)0,
                                reinterpret_cast<TimerCallbackFunction_t>(dataPublishCallback));

    loadTimer = xTimerCreate("loadTimer", pdMS_TO_TICKS(LOAD_TIMER),
                             pdFALSE, (void *)0,
                             reinterpret_cast<TimerCallbackFunction_t>(loadControlCallback));

    // Initialize Modbus charge controller and AC power meter
    setupTracer();
    #ifdef SENS_POWERINV
    JSY_MK.begin();
    #endif

    // Set relay output to OFF by default
    digitalWrite(RELE_PIN, RELE_OFF);

    #ifdef DEBUG
    SERIAL_DEBUG.println("---------------");
    #endif

    // Start timers for data publishing and load management (non-periodic; restarted inside callbacks)
    xTimerStart(publishTimer, 0);
    xTimerStart(loadTimer, 0);
}

// ============================================================================
// EXECUTION LOOP: handles network reconnection, MQTT, data acquisition, control
// ============================================================================
void exec()
{
    // ------------------------------------------------------------------------
    // WiFi RECONNECTION HANDLER
    // ------------------------------------------------------------------------
    if (WiFi.status() != WL_CONNECTED)
    {
        long now = millis();
        if (now - wifiReconnectAttempt > 5000)  // Retry every 5 seconds
        {
            wifiReconnectAttempt = now;
            #ifdef DEBUG
            SERIAL_DEBUG.println("Reconnecting to WiFi...");
            #endif
            if (WiFi.begin(WIFI_SSID, WIFI_PASSWORD) == WL_CONNECTED)
            {
                #ifdef DEBUG
                SERIAL_DEBUG.println("Connected to: " + String(WIFI_SSID));
                #endif
                wifiReconnectAttempt = 0;
                return;
            }
        }
    }

    // ------------------------------------------------------------------------
    // MQTT RECONNECTION HANDLER
    // ------------------------------------------------------------------------
    if (!mqtt.client.connected())
    {
        long now = millis();
        if (now - mqttReconnectAttempt > 5000)  // Retry every 5 seconds
        {
            mqttReconnectAttempt = now;
            if (mqtt.reconnect())
            {
                // Subscribe to control topics once connected
                // DateTime topic to receive current time
                if (mqtt.client.subscribe(MQTT_DATETIME_TOPIC, MQTT_QOS_SUB))
                {
                    #ifdef DEBUG
                    SERIAL_DEBUG.println("Subscribed to: " + String(MQTT_DATETIME_TOPIC));
                    #endif
                }
                // Command topic to receive operational commands
                if (mqtt.client.subscribe(MQTT_CMD_TOPIC, MQTT_QOS_SUB))
                {
                    #ifdef DEBUG
                    SERIAL_DEBUG.println("Subscribed to: " + String(MQTT_CMD_TOPIC));
                    #endif
                }

                // Set callback handler for incoming MQTT messages
                mqtt.client.setCallback(mqttSubscribeCallback);
                mqttReconnectAttempt = 0;
                return;
            }
        }
    }

    // Maintain MQTT connection loop
    mqtt.client.loop();

    // ------------------------------------------------------------------------
    // DATA ACQUISITION BLOCK
    // Triggered via publishTimer
    // ------------------------------------------------------------------------
    if (acq)
    {
        acq = false;

        // Only publish data if datetime is set and run command enabled
        if (datetimeSetted && cmdRun)
        {
            DynamicJsonDocument doc(2048);
            doc["id"] = BOARD_ID;
            doc["location"] = LOCATION;
            JsonArray readings = doc.createNestedArray("readings");

            // Read from charge controller
            readTracer(&readings);

            // Read from temperature & humidity sensor only if enabled
            #ifdef SENS_TEMPHUM
            readTempHum(&readings);
            #endif

            // Read from AC power meter only if enabled
            #ifdef SENS_POWERINV
            readPower(&readings);
            #endif

            // Convert JSON to string and publish via MQTT
            String message;
            serializeJson(doc, message);

            #ifdef DEBUG
            SERIAL_DEBUG.println(message);
            #endif

            // Publish message to MQTT broker
            bool pubStatus = mqtt.publishMessage(MQTT_PUBLISH_TOPIC, message, false);

            #ifdef DEBUG
            if (!pubStatus)
                SERIAL_DEBUG.println("--- PUBLISH ERROR OCCURRED ---");
            else
                SERIAL_DEBUG.println("- MESSAGE PUBLISHED -");
            #endif
        }

        // Restart acquisition timer
        xTimerStart(publishTimer, 0);
    }

    // ------------------------------------------------------------------------
    // LOAD CONTROL BLOCK
    // Triggered via loadTimer
    // ------------------------------------------------------------------------
    if (control)
    {
        uint8_t result;

        // DC load control through Modbus coil
        if (cmdLoadDC)
        {
            // Turn ON DC load
            result = TRACER.writeSingleCoil(TracerSettings.loadOn,
                                            MODBUS_ADDRESS_LOAD_MANUAL_ONOFF);
        }
        else
        {
            // Turn OFF DC load
            result = TRACER.writeSingleCoil(TracerSettings.loadOff,
                                            MODBUS_ADDRESS_LOAD_MANUAL_ONOFF);
        }
        TRACER.exceptionHandler(result, "Write", "MODBUS_ADDRESS_LOAD_MANUAL_ONOFF");

        // AC load control through external relay GPIO
        if (cmdLoadAC)
        {
            // Turn ON AC load
            digitalWrite(RELE_PIN, RELE_ON);
        }
        else
        {
            // Turn OFF AC load
            digitalWrite(RELE_PIN, RELE_OFF);
        }

        // Restart load control timer
        xTimerStart(loadTimer, 0);
    }
}

// ============================================================================
// TRACER CONTROLLER SETUP: configure Modbus registers and confirm settings
// ============================================================================
void setupTracer()
{
    uint8_t result;

    // Disable load before configuring
    // WRITE LOAD_OFF
	result = TRACER.writeSingleCoil(TracerSettings.loadOff, MODBUS_ADDRESS_LOAD_MANUAL_ONOFF);
	TRACER.exceptionHandler(result, "Write", "MODBUS_ADDRESS_LOAD_MANUAL_ONOFF");
	// WRITE BATTERY_TYPE
	result = TRACER.writeMultipleRegisters(TracerSettings.batteryType, MODBUS_ADDRESS_BATTERY_TYPE, 1);
	TRACER.exceptionHandler(result, "Write", "MODBUS_ADDRESS_BATTERY_TYPE");
	// WRITE BATTERY_CAPACITY
	result = TRACER.writeMultipleRegisters(TracerSettings.batteryCapacity, MODBUS_ADDRESS_BATTERY_CAPACITY, 1);
	TRACER.exceptionHandler(result, "Write", "MODBUS_ADDRESS_BATTERY_CAPACITY");
	// WRITE BATTERY_RATED_VOLTAGE
	result = TRACER.writeMultipleRegisters(TracerSettings.batteryVoltage, MODBUS_ADDRESS_BATTERY_RATED_VOLTAGE_CODE, 1);
	TRACER.exceptionHandler(result, "Write", "MODBUS_ADDRESS_BATTERY_RATED_VOLTAGE");

    // ------------------------------------------------------------------------
    // READ BACK SETTINGS FROM TRACER (Validation)
    // ------------------------------------------------------------------------

	// READ BATTERY_TYPE
	result = TRACER.readHoldingRegisters(MODBUS_ADDRESS_BATTERY_TYPE, &TracerReadings.batteryType, 1);
	if (result != 0)
	{
		TRACER.exceptionHandler(result, "Read", "MODBUS_ADDRESS_BATTERY_TYPE");
	}
	else
	{
		#ifdef DEBUG
				SERIAL_DEBUG.printf("BATTERY_TYPE: 0x%X\n", TracerReadings.batteryType);
		#endif
	}
	// READ BATTERY_CAPACITY
	result = TRACER.readHoldingRegisters(MODBUS_ADDRESS_BATTERY_CAPACITY, &TracerReadings.batteryCapacity, 1);
	if (result != 0)
	{
		TRACER.exceptionHandler(result, "Read", "MODBUS_ADDRESS_BATTERY_CAPACITY");
	}
	else
	{
		#ifdef DEBUG
				SERIAL_DEBUG.printf("BATTERY_CAPACITY: 0x%X\n", TracerReadings.batteryCapacity);
		#endif
	}
	// READ BATTERY_RATED_VOLTAGE_CODE
	result = TRACER.readHoldingRegisters(MODBUS_ADDRESS_BATTERY_RATED_VOLTAGE_CODE, &TracerReadings.batteryRatedVoltageCode, 1);
	if (result != 0)
	{
		TRACER.exceptionHandler(result, "Read", "MODBUS_ADDRESS_BATTERY_RATED_VOLTAGE_CODE");
	}
	else
	{
		#ifdef DEBUG
				SERIAL_DEBUG.printf("BATTERY_RATED_VOLTAGE_CODE: 0x%X\n", TracerReadings.batteryRatedVoltageCode);
		#endif
	}
}

// ============================================================================
// READ REAL-TIME VALUES FROM CHARGE CONTROLLER AND APPEND TO JSON ARRAY
// ============================================================================
void readTracer(JsonArray *readArray)
{
	uint8_t result;

	// READ VALUES - REAL-TIME
	// READ BATTERY_CURRENT_VOLTAGE
	result = TRACER.readSingleInputRegisters(MODBUS_ADDRESS_BATTERY_CURRENT_VOLTAGE, &TracerReadings.batteryVoltage, true);
	if (result != 0)
	{
		TRACER.exceptionHandler(result, "Read", "MODBUS_ADDRESS_BATTERY_CURRENT_VOLTAGE");
	}
	else
	{
		JsonObject reading = readArray->createNestedObject();
		reading["data_id"] = BATT_V_ID;
		reading["unit"] = BATT_V_UNIT;
		JsonArray data = reading.createNestedArray("data");
		JsonObject singleData = data.createNestedObject();
		singleData["timestamp"] = (int)now();
		singleData["val"] = TracerReadings.batteryVoltage;
		#ifdef DEBUG
				SERIAL_DEBUG.printf("BATTERY_VOLTAGE: %.2f\n", TracerReadings.batteryVoltage);
		#endif
	}
	// READ BATTERY_CHARGE_CURRENT
	result = TRACER.readSingleInputRegisters(MODBUS_ADDRESS_BATTERY_CHARGE_CURRENT, &TracerReadings.batteryCurrent, true);
	if (result != 0)
	{
		TRACER.exceptionHandler(result, "Read", "MODBUS_ADDRESS_BATTERY_CHARGE_CURRENT");
	}
	else
	{
		JsonObject reading = readArray->createNestedObject();
		reading["data_id"] = BATT_C_ID;
		reading["unit"] = BATT_C_UNIT;
		JsonArray data = reading.createNestedArray("data");
		JsonObject singleData = data.createNestedObject();
		singleData["timestamp"] = (int)now();
		singleData["val"] = TracerReadings.batteryCurrent;
		#ifdef DEBUG
				SERIAL_DEBUG.printf("BATTERY_CURRENT: %.2f\n", TracerReadings.batteryCurrent);
		#endif
	}
	// READ BATTERY_SOC
	result = TRACER.readSingleInputRegisters(MODBUS_ADDRESS_BATTERY_SOC, &TracerReadings.batterySOC, false);
	if (result != 0)
	{
		TRACER.exceptionHandler(result, "Read", "MODBUS_ADDRESS_BATTERY_SOC");
	}
	else
	{
		JsonObject reading = readArray->createNestedObject();
		reading["data_id"] = BATT_SOC_ID;
		reading["unit"] = BATT_SOC_UNIT;
		JsonArray data = reading.createNestedArray("data");
		JsonObject singleData = data.createNestedObject();
		singleData["timestamp"] = (int)now();
		singleData["val"] = TracerReadings.batterySOC;
		#ifdef DEBUG
				SERIAL_DEBUG.printf("BATTERY_SOC: %.2f\n", TracerReadings.batterySOC);
		#endif
	}
	// READ LOAD_VOLTAGE
	result = TRACER.readSingleInputRegisters(MODBUS_ADDRESS_LOAD_VOLTAGE, &TracerReadings.loadVoltage, true);
	if (result != 0)
	{
		TRACER.exceptionHandler(result, "Read", "MODBUS_ADDRESS_LOAD_VOLTAGE");
	}
	else
	{
		JsonObject reading = readArray->createNestedObject();
		reading["data_id"] = LOAD_V_ID;
		reading["unit"] = LOAD_V_UNIT;
		JsonArray data = reading.createNestedArray("data");
		JsonObject singleData = data.createNestedObject();
		singleData["timestamp"] = (int)now();
		singleData["val"] = TracerReadings.loadVoltage;
		#ifdef DEBUG
				SERIAL_DEBUG.printf("LOAD_VOLTAGE: %.2f\n", TracerReadings.loadVoltage);
		#endif
	}
	// READ LOAD_CURRENT
	result = TRACER.readSingleInputRegisters(MODBUS_ADDRESS_LOAD_CURRENT, &TracerReadings.loadCurrent, true);
	if (result != 0)
	{
		TRACER.exceptionHandler(result, "Read", "MODBUS_ADDRESS_LOAD_CURRENT");
	}
	else
	{
		JsonObject reading = readArray->createNestedObject();
		reading["data_id"] = LOAD_C_ID;
		reading["unit"] = LOAD_C_UNIT;
		JsonArray data = reading.createNestedArray("data");
		JsonObject singleData = data.createNestedObject();
		singleData["timestamp"] = (int)now();
		singleData["val"] = TracerReadings.loadCurrent;
		#ifdef DEBUG
				SERIAL_DEBUG.printf("LOAD_CURRENT: %.2f\n", TracerReadings.loadCurrent);
		#endif
	}
	// READ PV_VOLTAGE
	result = TRACER.readSingleInputRegisters(MODBUS_ADDRESS_PV_VOLTAGE, &TracerReadings.pvVoltage, true);
	if (result != 0)
	{
		TRACER.exceptionHandler(result, "Read", "MODBUS_ADDRESS_PV_VOLTAGE");
	}
	else
	{
		JsonObject reading = readArray->createNestedObject();
		reading["data_id"] = PV_V_ID;
		reading["unit"] = PV_V_UNIT;
		JsonArray data = reading.createNestedArray("data");
		JsonObject singleData = data.createNestedObject();
		singleData["timestamp"] = (int)now();
		singleData["val"] = TracerReadings.pvVoltage;
		#ifdef DEBUG
				SERIAL_DEBUG.printf("PV_VOLTAGE: %.2f\n", TracerReadings.pvVoltage);
		#endif
	}
	// READ PV_CURRENT
	result = TRACER.readSingleInputRegisters(MODBUS_ADDRESS_PV_CURRENT, &TracerReadings.pvCurrent, true);
	if (result != 0)
	{
		TRACER.exceptionHandler(result, "Read", "MODBUS_ADDRESS_PV_CURRENT");
	}
	else
	{
		JsonObject reading = readArray->createNestedObject();
		reading["data_id"] = PV_C_ID;
		reading["unit"] = PV_C_UNIT;
		JsonArray data = reading.createNestedArray("data");
		JsonObject singleData = data.createNestedObject();
		singleData["timestamp"] = (int)now();
		singleData["val"] = TracerReadings.pvCurrent;
		#ifdef DEBUG
				SERIAL_DEBUG.printf("PV_CURRENT: %.2f\n", TracerReadings.pvCurrent);
		#endif
	}
}

// -----------------------------------------------------------------------------
// READ AC POWER METER DATA (JSY-MK) – inverter side measurements
// This block is compiled only if SENS_POWERINV is enabled
// -----------------------------------------------------------------------------
void readPower(JsonArray *readArray)
{
    #ifdef SENS_POWERINV
    uint8_t result;

    // Verify that the power meter is responding correctly
    result = JSY_MK.test();
    if (result == 0)
    {
        // -------------------------------
        // Voltage
        // -------------------------------
        result = JSY_MK.readVoltage(JSY_CLAMP, &JSYReadings.voltage);
        if (result == 0)
        {
            JsonObject reading = readArray->createNestedObject();
            reading["data_id"] = INV_V_ID;
            reading["unit"] = INV_V_UNIT;

            JsonArray data = reading.createNestedArray("data");
            JsonObject singleData = data.createNestedObject();
            singleData["timestamp"] = (int)now();
            singleData["val"] = JSYReadings.voltage;

            #ifdef DEBUG
                SERIAL_DEBUG.printf("INV_VOLTAGE: %.2f\n", JSYReadings.voltage);
            #endif
        }

        // -------------------------------
        // Current
        // -------------------------------
        result = JSY_MK.readCurrent(JSY_CLAMP, &JSYReadings.current);
        if (result == 0)
        {
            JsonObject reading = readArray->createNestedObject();
            reading["data_id"] = INV_C_ID;
            reading["unit"] = INV_C_UNIT;

            JsonArray data = reading.createNestedArray("data");
            JsonObject singleData = data.createNestedObject();
            singleData["timestamp"] = (int)now();
            singleData["val"] = JSYReadings.current;

            #ifdef DEBUG
                SERIAL_DEBUG.printf("INV_CURRENT: %.2f\n", JSYReadings.current);
            #endif
        }

        // -------------------------------
        // Active power
        // -------------------------------
        result = JSY_MK.readPower(JSY_CLAMP, &JSYReadings.power);
        if (result == 0)
        {
            JsonObject reading = readArray->createNestedObject();
            reading["data_id"] = INV_POW_ID;
            reading["unit"] = INV_POW_UNIT;

            JsonArray data = reading.createNestedArray("data");
            JsonObject singleData = data.createNestedObject();
            singleData["timestamp"] = (int)now();
            singleData["val"] = JSYReadings.power;

            #ifdef DEBUG
                SERIAL_DEBUG.printf("INV_POWER: %.2f\n", JSYReadings.power);
            #endif
        }

        // -------------------------------
        // Energy
        // -------------------------------
        result = JSY_MK.readEnergy(JSY_CLAMP, &JSYReadings.energy);
        if (result == 0)
        {
            JsonObject reading = readArray->createNestedObject();
            reading["data_id"] = INV_ENER_ID;
            reading["unit"] = INV_ENER_UNIT;

            JsonArray data = reading.createNestedArray("data");
            JsonObject singleData = data.createNestedObject();
            singleData["timestamp"] = (int)now();
            singleData["val"] = JSYReadings.energy;

            #ifdef DEBUG
                SERIAL_DEBUG.printf("INV_ENERGY: %.2f\n", JSYReadings.energy);
            #endif
        }

        // -------------------------------
        // Power factor
        // -------------------------------
        result = JSY_MK.readPowerfactor(JSY_CLAMP, &JSYReadings.powerfactor);
        if (result == 0)
        {
            JsonObject reading = readArray->createNestedObject();
            reading["data_id"] = INV_PF_ID;
            reading["unit"] = INV_PF_UNIT;

            JsonArray data = reading.createNestedArray("data");
            JsonObject singleData = data.createNestedObject();
            singleData["timestamp"] = (int)now();
            singleData["val"] = JSYReadings.powerfactor;

            #ifdef DEBUG
                SERIAL_DEBUG.printf("INV_PF: %.2f\n", JSYReadings.powerfactor);
            #endif
        }

        // -------------------------------
        // Power direction (import/export)
        // -------------------------------
        result = JSY_MK.readPowerdirection(&JSYReadings.powerdirection);
        if (result == 0)
        {
            JsonObject reading = readArray->createNestedObject();
            reading["data_id"] = INV_PD_ID;
            reading["unit"] = INV_PD_UNIT;

            JsonArray data = reading.createNestedArray("data");
            JsonObject singleData = data.createNestedObject();
            singleData["timestamp"] = (int)now();
            singleData["val"] = JSYReadings.powerdirection;

            #ifdef DEBUG
                SERIAL_DEBUG.printf("INV_PD: %.2f\n", JSYReadings.powerdirection);
            #endif
        }

        // -------------------------------
        // Frequency
        // -------------------------------
        result = JSY_MK.readFrequency(&JSYReadings.frequency);
        if (result == 0)
        {
            JsonObject reading = readArray->createNestedObject();
            reading["data_id"] = INV_F_ID;
            reading["unit"] = INV_F_UNIT;

            JsonArray data = reading.createNestedArray("data");
            JsonObject singleData = data.createNestedObject();
            singleData["timestamp"] = (int)now();
            singleData["val"] = JSYReadings.frequency;

            #ifdef DEBUG
                SERIAL_DEBUG.printf("INV_FREQ: %.2f\n", JSYReadings.frequency);
            #endif
        }
    }
    #endif
}
// -----------------------------------------------------------------------------
// READ TEMPERATURE AND HUMIDITY (DHT sensor)
// -----------------------------------------------------------------------------
void readTempHum(JsonArray *readArray)
{
    #ifdef SENS_TEMPHUM
    // Temperature measurement
    JsonObject env_temp = readArray->createNestedObject();
    env_temp["data_id"] = TEMP_ID;
    env_temp["unit"] = TEMP_UNIT;

    JsonArray temp = env_temp.createNestedArray("data");
    JsonObject singleTemp = temp.createNestedObject();
    singleTemp["timestamp"] = (int)now();
    singleTemp["val"] = dht.readTemperature();

    #ifdef DEBUG
        SERIAL_DEBUG.printf("ENV_TEMP: %.2f\n", dht.readTemperature());
    #endif

    // Humidity measurement
    JsonObject env_hum = readArray->createNestedObject();
    env_hum["data_id"] = HUM_ID;
    env_hum["unit"] = HUM_UNIT;

    JsonArray hum = env_hum.createNestedArray("data");
    JsonObject singleHum = hum.createNestedObject();
    singleHum["timestamp"] = (int)now();
    singleHum["val"] = dht.readHumidity();

    #ifdef DEBUG
        SERIAL_DEBUG.printf("ENV_HUM: %.2f\n", dht.readHumidity());
    #endif
    #endif
}
// -----------------------------------------------------------------------------
// MQTT CALLBACKS AND COMMAND PARSING
// -----------------------------------------------------------------------------

// Sets system time based on MQTT datetime message
void setDatetime(String datetime)
{
    uint64_t datetimeD = datetime.toDouble();
    if (datetimeD > 0)
    {
        setTime(datetimeD);   // Set system RTC
    }
    datetimeSetted = true;
}

// Called by MQTT client when a message arrives
void mqttSubscribeCallback(char *topic, byte *payload, unsigned int length)
{
    #ifdef DEBUG
        SERIAL_DEBUG.print("Message arrived [");
        SERIAL_DEBUG.print(topic);
        SERIAL_DEBUG.print("]: ");
    #endif

    // Convert payload to String
    String payloadStr;
    for (int i = 0; i < length; i++)
    {
        payloadStr += (char)payload[i];
    }

    #ifdef DEBUG
        SERIAL_DEBUG.println(payloadStr);
    #endif

    mqttParseMessage(topic, payloadStr.c_str());
}

// Parse MQTT messages based on topic
void mqttParseMessage(char *topic, const char *payloadStr)
{
    // Time synchronization topic
    if (strcmp(topic, MQTT_DATETIME_TOPIC) == 0)
    {
        setDatetime(payloadStr);
    }

    // Command topic (JSON array)
    if (strcmp(topic, MQTT_CMD_TOPIC) == 0)
    {
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payloadStr);
        JsonArray array = doc.as<JsonArray>();

        // Iterate over received command elements
        for (JsonVariant v : array)
        {
            DynamicJsonDocument elem(1024);
            deserializeJson(elem, v.as<String>());

            // Execute only commands addressed to this board
            if (strcmp(elem["id"], BOARD_ID) == 0)
            {
                cmdRun    = elem["run"];
                cmdLoadDC = elem["dc_load"];
                cmdLoadAC = elem["ac_load"];
            }
            elem.clear();
        }
        doc.clear();
    }
}
// -----------------------------------------------------------------------------
// TIMER CALLBACKS
// -----------------------------------------------------------------------------

// Trigger data acquisition cycle
void dataPublishCallback()
{
    acq = true;
}

// Trigger load control evaluation
void loadControlCallback()
{
    control = true;
}

// -----------------------------------------------------------------------------
// Main execution model
// -----------------------------------------------------------------------------

// setup() runs once, then enters a permanent execution loop
// This approach improves watchdog stability compared to standard Arduino loop()
void setup()
{
    set();
    while (1)
    {
        exec();
    }
}

// loop() intentionally left empty
void loop() {}