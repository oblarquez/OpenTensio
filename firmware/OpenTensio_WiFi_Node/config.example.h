#pragma once

/*
  OpenTensio WiFi Node configuration example

  Copy this file to config.h and adapt values to your hardware/network.
  config.h is ignored by Git to avoid publishing Wi-Fi/MQTT secrets.
*/

#define OPENTENSIO_NODE_ID "wifi_node_01"

// Wi-Fi
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
#define WIFI_CONNECT_TIMEOUT_MS 12500

// MQTT
#define MQTT_SERVER "192.168.1.10"
#define MQTT_PORT 1883
#define MQTT_USER "your_mqtt_user"
#define MQTT_PASSWORD "your_mqtt_password"
#define MQTT_CLIENT_ID "opentensio_wifi_node_01"

// v0.1.0 drop-in compatibility with the original standalone Wi-Fi prototype.
#define MQTT_STATE_TOPIC "jardin/sondes"

// Pins / buses
#define BATTERY_ADC_PIN 34
#define ONE_WIRE_BUS 18
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// Soil sensor excitation pins.
// Wiring: GPIO/VIN_EXC -> fixed resistor -> ADS input -> soil sensor -> GND
#define SENSOR_1_EXC_PIN 4
#define SENSOR_2_EXC_PIN 5
#define ADS_CHANNEL_SENSOR_1 0
#define ADS_CHANNEL_SENSOR_2 1

// Measurement circuit. These defaults match OpenTensio LoRa TX v0.1.0.
#define VIN_EXC 3.30
#define REFERENCE_RESISTOR_OHMS 10000.0
#define SENSOR_SETTLE_US 150
#define SENSOR_DISCHARGE_MS 50
#define SENSOR_MIN_VALID_VOLTAGE 0.02
#define SENSOR_MAX_VALID_RATIO 0.98
#define SENSOR_OPEN_CIRCUIT_OHMS 999999.0

// Board-specific battery calibration for simple ESP32 ADC divider.
// Original formula: raw/4095 * 3.3 * 2.0 * 1.05
#define BATTERY_DIVIDER_RATIO 2.0
#define BATTERY_CALIBRATION_FACTOR 1.05

// Sleep
#define SLEEP_SECONDS 3600
