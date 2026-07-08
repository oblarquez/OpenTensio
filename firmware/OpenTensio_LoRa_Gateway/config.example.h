#pragma once

/*
  OpenTensio LoRa Gateway configuration example

  Copy this file to config.h and adapt values to your network.
  config.h is ignored by Git to avoid publishing Wi-Fi/MQTT secrets.
*/

// Wi-Fi
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
#define WIFI_CONNECT_TIMEOUT_MS 20000

// MQTT
#define MQTT_SERVER "192.168.1.10"
#define MQTT_PORT 1883
#define MQTT_USER "your_mqtt_user"
#define MQTT_PASSWORD "your_mqtt_password"
#define MQTT_CLIENT_ID "opentensio_gateway_01"

// v0.1.0 drop-in compatibility mode.
// Keep these defaults to reproduce the original Home Assistant MQTT topics.
#define OPENTENSIO_PUBLISH_LEGACY_HOMEASSISTANT_TOPICS 1
#define MQTT_LEGACY_PREFIX "homeassistant/sensor"

// Future/non-breaking JSON state output. Disabled by default for v0.1.0
// so existing Home Assistant setups keep receiving the same individual topics.
#define OPENTENSIO_PUBLISH_JSON_STATE 0
#define MQTT_BASE_TOPIC "opentensio"
#define MQTT_GATEWAY_ID "gateway_01"

// LoRa settings. These must match the transmitter.
#define RF_FREQUENCY 868000000
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 12
#define LORA_CODING_RATE 1
#define LORA_PREAMBLE_LENGTH 8

// OLED
#define OLED_AUTO_SLEEP_MS 60000
// #define OPENTENSIO_FLIP_OLED
