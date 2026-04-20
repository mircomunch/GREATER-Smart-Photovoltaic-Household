// ============================================================================
// MQTT.cpp
// ----------------------------------------------------------------------------
// This module provides a lightweight wrapper around PubSubClient for MQTT
// communication. It supports:
//   - multiple constructor options (with/without authentication)
//   - TLS capability (optional certificate loading)
//   - reconnection handling
//   - publishing messages
//   - callback wiring for incoming subscribed messages (set externally)
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

#include <MQTT.h>

// -----------------------------------------------------------------------------
// MQTT(board_id, host, port)
// Basic constructor for unsecured MQTT communication.
//   - board_id: unique device identifier appended to the MQTT client ID
//   - host: MQTT broker domain/IP
//   - port: MQTT broker port
// This constructor uses the unsecured WiFiClient (non-TLS).
// -----------------------------------------------------------------------------
MQTT::MQTT(String board_id, char *host, uint16_t port)
{
    this->_board_id = board_id;
    this->_host = host;
    this->_port = port;
    this->_cert_en = false;        // Certificate not enabled

    // Configure MQTT client to use non-secure client
    this->client.setServer(this->_host, this->_port);
    this->client.setClient(this->_unsecuredClient);
}

// -----------------------------------------------------------------------------
// MQTT(board_id, host, port, msg_size)
// Constructor variant allowing custom MQTT payload buffer size.
// Useful for large JSON messages.
// -----------------------------------------------------------------------------
MQTT::MQTT(String board_id, char *host, uint16_t port, int msg_size)
{
    this->_board_id = board_id;
    this->_host = host;
    this->_port = port;
    this->_msg_size = msg_size;
    this->_cert_en = false;

    this->client.setServer(this->_host, this->_port);
    this->client.setClient(this->_unsecuredClient);
    this->client.setBufferSize(this->_msg_size);  // Configure custom buffer
}

// -----------------------------------------------------------------------------
// MQTT(board_id, host, port, msg_size, cert_en, user, psw)
// Full constructor supporting:
//   - TLS certificate loading (if cert_en=true)
//   - MQTT authentication (username/password)
//   - custom buffer size
// -----------------------------------------------------------------------------
MQTT::MQTT(String board_id, char *host, uint16_t port, int msg_size,
           bool cert_en, char *user, char *psw)
{
    this->_board_id = board_id;
    this->_host = host;
    this->_port = port;
    this->_msg_size = msg_size;
    this->_cert_en = cert_en;
    this->_user = user;
    this->_psw = psw;

    // Configure secure or unsecured client depending on certificate flag
    this->client.setServer(this->_host, this->_port);
    this->client.setClient(this->_securedClient);

    if (_cert_en)
    {
        // Load CA certificate for TLS
        this->_securedClient.setCACert(root_ca);
    }

    this->client.setBufferSize(this->_msg_size);
}

// -----------------------------------------------------------------------------
// reconnect()
// Tries to establish a connection to the MQTT broker.
// Returns true if successful, false otherwise.
// Client ID = board_id + random hex suffix (minimizes collisions).
// If credentials exist, authenticated connect() is used.
// -----------------------------------------------------------------------------
bool MQTT::reconnect()
{
    Serial.print("Attempting MQTT connection… ");

    // Create dynamic client ID
    String clientId = this->_board_id;
    clientId += String(random(0xffff), HEX);

    // Attempt connection (authenticated if credentials provided)
    if (this->_user && this->_psw)
    {
        Serial.println("authenticated");
        this->client.connect(clientId.c_str(), this->_user, this->_psw);
    }
    else
    {
        Serial.println("un-authenticated");
        this->client.connect(clientId.c_str());
    }

    // Return connection status
    return client.connected();
}

// -----------------------------------------------------------------------------
// publishMessage(topic, payload, retained)
// Wraps the PubSubClient publish() for consistent string handling.
// Returns true if message was successfully sent.
// -----------------------------------------------------------------------------
bool MQTT::publishMessage(const char *topic, String payload, bool retained)
{
    return this->client.publish(topic, payload.c_str(), retained);
}
