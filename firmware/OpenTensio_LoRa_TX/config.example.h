#pragma once

/*
  OpenTensio LoRa TX configuration example

  Copy this file to config.h and adapt values to your hardware.
  config.h is ignored by Git to avoid publishing local/private settings.
*/

#define OPENTENSIO_NODE_ID "field_node_01"

// LoRa settings
#define RF_FREQUENCY 868000000
#define LORA_TX_POWER 14
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 12
#define LORA_CODING_RATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_TX_WAIT_MS 15000

// Pins / buses
#define ONE_WIRE_BUS 1
#define I2C_SDA_PIN 41
#define I2C_SCL_PIN 42
#define BATTERY_ADC_PIN 1
#define BATTERY_ENABLE_PIN 37

// Soil sensor excitation pins.
// Wiring: GPIO/VIN_EXC -> fixed resistor -> ADS input -> soil sensor -> GND
#define SENSOR_1_EXC_PIN 4
#define SENSOR_2_EXC_PIN 5
#define ADS_CHANNEL_SENSOR_1 0
#define ADS_CHANNEL_SENSOR_2 1

// Measurement circuit
#define VIN_EXC 3.30
#define REFERENCE_RESISTOR_OHMS 10000.0
#define SENSOR_SETTLE_US 150
#define SENSOR_DISCHARGE_MS 50
#define SENSOR_MIN_VALID_VOLTAGE 0.02
#define SENSOR_MAX_VALID_RATIO 0.98
#define SENSOR_OPEN_CIRCUIT_OHMS 999999.0

// Board-specific battery calibration.
// Adjust this value for your board and voltage divider.
#define BATTERY_CALIBRATION_FACTOR 0.001085

// Sleep
#define SLEEP_SECONDS 3600

// Optional: define the expected DS18B20 soil temperature probe address.
// Leave OPENTENSIO_USE_FIXED_SOIL_TEMP_ADDRESS undefined to auto-detect.
// #define OPENTENSIO_USE_FIXED_SOIL_TEMP_ADDRESS
// static DeviceAddress SOIL_TEMP_ADDRESS = { 0x28, 0xA3, 0x6F, 0xB6, 0x09, 0x00, 0x00, 0x73 };
