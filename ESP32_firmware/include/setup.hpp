#include "chargeContrModb_setup.hpp"
#include "temphumDHT_setup.hpp"
#include "acPowerMeter_setup.hpp"
#include "acRelay_setup.hpp"

// Serial debug
#define DEBUG
#ifdef DEBUG
    #define SERIAL_DEBUG Serial
    #define SERIAL_DEBUG_SPEED 115200
#endif

// BOARD NAME
#define BOARD_ID "---INSERT YOUR ID HERE---" 		// e.g., "LabKit_01"
#define LOCATION "---INSERT YOUR LOCATION HERE---"	// e.g., "Office"

// Sensors activated
#define SENS_TEMPHUM
#define SENS_POWERINV

// WiFI configurations
#define WIFI_SSID "---INSERT YOUR WIFI-SSID HERE---"  // e.g., "LabKit_wifi"
#define WIFI_PASSWORD "---INSERT YOUR WIFI-PSW HERE---"  // e.g., "LabKit_psw"

// MQTT configurations
// #define MQTT_AUTH
#ifdef MQTT_AUTH
	#define MQTT_HOST "---INSERT YOUR AUTHENTICATED-BROKER-HOSTNAME HERE---" // e.g., "mqtt.myserver.com"
	#define MQTT_PORT 1234 // INSERT YOUR AUTHENTICATED-BROKER-PORT-NUMBER // e.g., 8883
	#define MQTT_USER "---INSERT YOUR AUTHENTICATED-USERNAME HERE---" // e.g., "my_user"
	#define MQTT_PASSWORD "---INSERT YOUR AUTHENTICATED-USERNAME HERE---" // e.g., "my_password"
	#define MQTT_CERT_EN false // define if certificate needed for broker connection - setup your Broker certificate accordingly in MQTT.h
#else
	#define MQTT_HOST "---INSERT YOUR PUBLIC-BROKER-HOSTNAME HERE---" // e.g., "test.mosquitto.com"
	#define MQTT_PORT 1234 // INSERT YOUR UNAUTHENTICATED-BROKER-PORT-NUMBER // e.g., 1883
#endif

#define MQTT_TOPIC "---INSERT YOUR ROOT-TOPIC HERE---" // e.g., "smpvhh/labkit01/"
#define MQTT_PUBLISH_TOPIC MQTT_TOPIC "---INSERT YOUR DATA-TOPIC HERE---" // e.g., "smpvhh/labkit01/data"
#define MQTT_DATETIME_TOPIC MQTT_TOPIC "---INSERT YOUR DATETIME-TOPIC HERE---" // e.g., "smpvhh/labkit01/datetime"
#define MQTT_CMD_TOPIC MQTT_TOPIC "---INSERT YOUR COMMAND-TOPIC HERE---" // e.g., "smpvhh/labkit01/cmd"
#define MQTT_QOS_SUB 1
#define MQTT_PUBLISH_MESSAGE_MAX_SIZE 65536 / 2 // MQTT publish max size = 256MB

// Timer configurations
#define DATA_TIMER 5000
#define LOAD_TIMER 1000

// Number of tentatives for each readings
#define READ_TENTATIVE 2

#define READINGS_N SOLAR_N_READINGS

#ifdef SENS_TEMPHUM
    #define READINGS_N READINGS_N + TEMPHUM_N_READINGS
#endif
#ifdef SENS_POWERINV
    #define READINGS_N READINGS_N + POWER_N_READINGS
#endif