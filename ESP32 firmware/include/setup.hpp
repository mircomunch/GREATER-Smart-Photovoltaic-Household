#include "solarModbus_setup.hpp"
#include "dht11_setup.hpp"
#include "powerReader_setup.hpp"

// Serial debug
#define DEBUG
#ifdef DEBUG
    #define SERIAL_DEBUG Serial
    #define SERIAL_DEBUG_SPEED 115200
#endif

// BOARD NAME
#define BOARD_ID "---INSERT YOUR ID HERE---"
#define LOCATION "---INSERT YOUR LOCATION HERE---"

// Sensors activated
#define SENS_TEMPHUM
#define SENS_POWERINV

#define READINGS_N SOLAR_N_READINGS

#ifdef SENS_TEMPHUM
    #define READINGS_N READINGS_N + TEMPHUM_N_READINGS
#endif
#ifdef SENS_POWERINV
    #define READINGS_N READINGS_N + POWER_N_READINGS
#endif

// Number of tentatives for each readings
#define READ_TENTATIVE 2

// WiFI configurations
#define WIFI_SSID "---INSERT YOUR WIFI-SSID HERE---"
#define WIFI_PASSWORD "---INSERT YOUR WIFI-PSW HERE---"

// MQTT configurations
// #define MQTT_AUTH
#ifdef MQTT_AUTH
	#define MQTT_HOST "---INSERT YOUR AUTHENTICATED-BROKER-HOSTNAME HERE---"
	#define MQTT_PORT AUTHENTICATED-BROKER-PORT-NUMBER
	#define MQTT_USER "---INSERT YOUR AUTHENTICATED-USERNAME HERE---"
	#define MQTT_PASSWORD "---INSERT YOUR AUTHENTICATED-USERNAME HERE---"
	#define MQTT_CERT_EN false // define if certificate needed for broker connection
#else
	#define MQTT_HOST "---INSERT YOUR PUBLIC-BROKER-HOSTNAME HERE---"
	#define MQTT_PORT AUTHENTICATED-BROKER-PORT-NUMBER
#endif

#define MQTT_TOPIC "---INSERT YOUR ROOT-TOPIC HERE---"
#define MQTT_PUBLISH_TOPIC MQTT_TOPIC "---INSERT YOUR DATA-TOPIC HERE---"
#define MQTT_DATETIME_TOPIC MQTT_TOPIC "---INSERT YOUR DATETIME-TOPIC HERE---"
#define MQTT_CMD_TOPIC MQTT_TOPIC "---INSERT YOUR COMMAND-TOPIC HERE---"
#define MQTT_QOS_SUB 1
#define MQTT_PUBLISH_MESSAGE_MAX_SIZE 65536 / 2 // MQTT publish max size = 256MB

// Timer configurations
#define DATA_TIMER 5000
#define LOAD_TIMER 1000

//#define SOLAR_TIMER 5000
//#define DHT_TIMER 5000
//#define PUBLISH_TIMER 10000