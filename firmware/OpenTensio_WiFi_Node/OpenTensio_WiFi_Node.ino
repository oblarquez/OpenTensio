#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "config.h"

/*
  OpenTensio WiFi Node v0.1.0-draft

  Standalone ESP32 node for resistive granular matrix soil water potential sensors.

  v0.1.0 default behavior is drop-in compatible with the original standalone
  Wi-Fi prototype:
  - Publishes one JSON payload to MQTT_STATE_TOPIC, default: jardin/sondes
  - Keeps the original JSON keys: v_bat, t_sol, rssi, s1_v, s1_r, s1_cb,
    s2_v, s2_r, s2_cb

  Measurement logic is intentionally aligned with OpenTensio LoRa TX v0.1.0:
  - GPIO/VIN_EXC -> fixed resistor -> ADS input -> sensor -> GND
  - short DC pulse on each sensor excitation GPIO
  - same resistance conversion
  - same IRROMETER-style centibar equation and 24 °C temperature correction

  For long-term reference measurements, AC excitation remains the preferred
  method for resistive granular matrix sensors.
*/

#ifndef WIFI_SSID
#error "Missing config.h. Copy config.example.h to config.h and configure this node."
#endif

Adafruit_ADS1115 ads;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);

void setSensorExcitationOff() {
  pinMode(SENSOR_1_EXC_PIN, OUTPUT);
  pinMode(SENSOR_2_EXC_PIN, OUTPUT);
  digitalWrite(SENSOR_1_EXC_PIN, LOW);
  digitalWrite(SENSOR_2_EXC_PIN, LOW);
}

float readBatteryVoltage() {
  uint32_t sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(BATTERY_ADC_PIN);
    delay(2);
  }

  float rawAverage = sum / 10.0;
  return (rawAverage / 4095.0) * 3.3 * BATTERY_DIVIDER_RATIO * BATTERY_CALIBRATION_FACTOR;
}

float readPulsedSensorVoltage(uint8_t excitationPin, uint8_t adsChannel) {
  digitalWrite(excitationPin, LOW);
  delay(SENSOR_DISCHARGE_MS);

  digitalWrite(excitationPin, HIGH);
  delayMicroseconds(SENSOR_SETTLE_US);

  // readADC_SingleEnded() blocks for the ADS1115 conversion time.
  // The pin remains HIGH only during settle + ADC conversion.
  int16_t raw = ads.readADC_SingleEnded(adsChannel);

  digitalWrite(excitationPin, LOW);

  return ads.computeVolts(raw);
}

float voltageToResistanceOhms(float vSensor) {
  if (vSensor <= SENSOR_MIN_VALID_VOLTAGE) return 0.0;
  if (vSensor >= VIN_EXC * SENSOR_MAX_VALID_RATIO) return SENSOR_OPEN_CIRCUIT_OHMS;

  // Circuit: GPIO/VIN_EXC -> fixed resistor -> ADS input -> sensor -> GND
  return (REFERENCE_RESISTOR_OHMS * vSensor) / (VIN_EXC - vSensor);
}

float calculateCentibars(float resistanceKOhms, float soilTemperatureC) {
  float resistanceOhms = resistanceKOhms * 1000.0;

  // Free water / saturation floor.
  if (resistanceOhms <= 550.0) return 0.0;

  // IRROMETER-style temperature correction, 24 °C base.
  float tempD = 1.00 + 0.018 * (soilTemperatureC - 24.00);
  float cb = 0.0;

  if (resistanceOhms <= 8000.0) {
    // Wet range.
    cb = (0.124 * resistanceKOhms - 0.018) / (0.018 * tempD);
  } else {
    // Dry range.
    cb = 2.246
       + (5.239 * resistanceKOhms * tempD)
       + (0.06756 * resistanceKOhms * resistanceKOhms * tempD * tempD);
  }

  if (cb > 200.0) cb = 200.0;
  if (cb < 0.0) cb = 0.0;

  return cb;
}

float readSoilTemperatureOrDefault() {
  tempSensors.requestTemperatures();
  float soilTemperatureC = tempSensors.getTempCByIndex(0);

  if (soilTemperatureC == DEVICE_DISCONNECTED_C || soilTemperatureC < -40.0 || soilTemperatureC > 85.0) {
    return 24.0;
  }

  return soilTemperatureC;
}

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.print("Wi-Fi connecting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(" OK, IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println(" failed");
  return false;
}

bool publishMQTT(const char *payload) {
  if (WiFi.status() != WL_CONNECTED) return false;

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  Serial.print("MQTT connecting");
  if (!mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
    Serial.println(" failed");
    return false;
  }

  Serial.println(" OK");
  bool ok = mqttClient.publish(MQTT_STATE_TOPIC, payload, true);
  mqttClient.loop();
  delay(500);
  mqttClient.disconnect();

  return ok;
}

String buildLegacyJsonPayload(
  float batteryVoltage,
  float soilTemperatureC,
  int32_t wifiRssi,
  float sensor1Voltage,
  float sensor1ResistanceKOhms,
  float sensor1Centibars,
  float sensor2Voltage,
  float sensor2ResistanceKOhms,
  float sensor2Centibars
) {
  // Keep the original JSON keys for v0.1.0 drop-in compatibility.
  String payload = "{";
  payload += "\"v_bat\":" + String(batteryVoltage, 2) + ",";
  payload += "\"t_sol\":" + String(soilTemperatureC, 1) + ",";
  payload += "\"rssi\":" + String(wifiRssi) + ",";
  payload += "\"s1_v\":" + String(sensor1Voltage, 3) + ",";
  payload += "\"s1_r\":" + String(sensor1ResistanceKOhms, 2) + ",";
  payload += "\"s1_cb\":" + String(sensor1Centibars, 1) + ",";
  payload += "\"s2_v\":" + String(sensor2Voltage, 3) + ",";
  payload += "\"s2_r\":" + String(sensor2ResistanceKOhms, 2) + ",";
  payload += "\"s2_cb\":" + String(sensor2Centibars, 1);
  payload += "}";
  return payload;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("==============================================");
  Serial.println("        OpenTensio WiFi Node v0.1.0           ");
  Serial.println("      GPIO short-pulse sensor excitation      ");
  Serial.println("==============================================");

  setSensorExcitationOff();
  analogReadResolution(12);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  bool adsOK = ads.begin(0x48);
  if (!adsOK) {
    Serial.println("[!] ADS1115 not found at 0x48");
  }
  ads.setGain(GAIN_ONE);
#ifdef RATE_ADS1115_860SPS
  ads.setDataRate(RATE_ADS1115_860SPS);
#endif

  tempSensors.begin();

  float batteryVoltage = readBatteryVoltage();
  float soilTemperatureC = readSoilTemperatureOrDefault();

  float sensor1Voltage = -1.0;
  float sensor2Voltage = -1.0;

  if (adsOK) {
    sensor1Voltage = readPulsedSensorVoltage(SENSOR_1_EXC_PIN, ADS_CHANNEL_SENSOR_1);
    sensor2Voltage = readPulsedSensorVoltage(SENSOR_2_EXC_PIN, ADS_CHANNEL_SENSOR_2);
  }

  setSensorExcitationOff();

  float sensor1ResistanceOhms = voltageToResistanceOhms(sensor1Voltage);
  float sensor2ResistanceOhms = voltageToResistanceOhms(sensor2Voltage);
  float sensor1ResistanceKOhms = sensor1ResistanceOhms / 1000.0;
  float sensor2ResistanceKOhms = sensor2ResistanceOhms / 1000.0;

  float sensor1Centibars = calculateCentibars(sensor1ResistanceKOhms, soilTemperatureC);
  float sensor2Centibars = calculateCentibars(sensor2ResistanceKOhms, soilTemperatureC);

  bool wifiOK = connectWiFi();
  int32_t wifiRssi = wifiOK ? WiFi.RSSI() : -999;

  Serial.println();
  Serial.println("--- Results ---");
  Serial.printf("Node        : %s\n", OPENTENSIO_NODE_ID);
  Serial.printf("Soil temp   : %.1f C\n", soilTemperatureC);
  Serial.printf("Sensor 1    : %.3f V | %.2f kOhm -> %.1f cb\n", sensor1Voltage, sensor1ResistanceKOhms, sensor1Centibars);
  Serial.printf("Sensor 2    : %.3f V | %.2f kOhm -> %.1f cb\n", sensor2Voltage, sensor2ResistanceKOhms, sensor2Centibars);
  Serial.printf("Battery     : %.2f V\n", batteryVoltage);
  Serial.printf("Wi-Fi RSSI  : %ld dBm\n", (long)wifiRssi);
  Serial.println("---------------");

  String payload = buildLegacyJsonPayload(
    batteryVoltage,
    soilTemperatureC,
    wifiRssi,
    sensor1Voltage,
    sensor1ResistanceKOhms,
    sensor1Centibars,
    sensor2Voltage,
    sensor2ResistanceKOhms,
    sensor2Centibars
  );

  Serial.printf("MQTT topic  : %s\n", MQTT_STATE_TOPIC);
  Serial.printf("MQTT payload: %s\n", payload.c_str());

  if (wifiOK) {
    bool sent = publishMQTT(payload.c_str());
    Serial.println(sent ? "MQTT sent" : "MQTT send failed");
  }

  // Double safety: pins low before deep sleep.
  setSensorExcitationOff();

  Serial.println("Deep sleep...");
  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_SECONDS * 1000000ULL);
  esp_deep_sleep_start();
}

void loop() {}
