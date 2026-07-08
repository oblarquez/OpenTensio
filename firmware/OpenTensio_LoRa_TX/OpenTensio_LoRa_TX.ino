#include "Arduino.h"
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_BME280.h>

#include "config.h"

/*
  OpenTensio LoRa TX v0.1.0-draft

  Field transmitter for resistive granular matrix soil water potential sensors.

  Hardware target used during initial development:
  - ESP32 / Heltec-style LoRa board
  - ADS1115 ADC on secondary I2C bus
  - 2 resistive soil water tension sensors
  - DS18B20 soil temperature probe
  - optional DS18B20 enclosure temperature probe
  - optional BME280 air temperature / humidity / pressure sensor
  - optional OLED display

  Sensor excitation:
  - The soil sensors are NOT powered permanently from 3.3 V.
  - Each sensor divider is powered by a dedicated GPIO only during measurement.
  - This is a short DC pulse method intended to reduce polarization compared with
    permanent DC excitation.
  - For long-term reference measurements, AC excitation remains the preferred method
    for resistive granular matrix sensors.
*/

#ifndef OPENTENSIO_NODE_ID
#error "Missing config.h. Copy config.example.h to config.h and configure this node."
#endif

TwoWire I2C_ADS = TwoWire(1);
SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
Adafruit_ADS1115 ads;
Adafruit_BME280 bme;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensors(&oneWire);

static DeviceAddress soilTempAddress;
static DeviceAddress boxTempAddress;
static bool hasSoilTempAddress = false;
static bool hasBoxTempAddress = false;

void setSensorExcitationOff() {
  pinMode(SENSOR_1_EXC_PIN, OUTPUT);
  pinMode(SENSOR_2_EXC_PIN, OUTPUT);
  digitalWrite(SENSOR_1_EXC_PIN, LOW);
  digitalWrite(SENSOR_2_EXC_PIN, LOW);
}

float readBatteryVoltage() {
  pinMode(BATTERY_ENABLE_PIN, OUTPUT);
  digitalWrite(BATTERY_ENABLE_PIN, LOW);
  delay(50);

  uint16_t raw = analogRead(BATTERY_ADC_PIN);

  digitalWrite(BATTERY_ENABLE_PIN, HIGH);

  // Board-specific calibration factor from the original field setup.
  // Adjust for your own voltage divider / board.
  return (float)raw * BATTERY_CALIBRATION_FACTOR;
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

void discoverTemperatureSensors() {
  tempSensors.begin();
  int count = tempSensors.getDeviceCount();

#ifdef OPENTENSIO_USE_FIXED_SOIL_TEMP_ADDRESS
  memcpy(soilTempAddress, SOIL_TEMP_ADDRESS, 8);
  hasSoilTempAddress = true;
#endif

  for (int i = 0; i < count; i++) {
    DeviceAddress addr;
    if (!tempSensors.getAddress(addr, i)) continue;

#ifndef OPENTENSIO_USE_FIXED_SOIL_TEMP_ADDRESS
    if (!hasSoilTempAddress) {
      memcpy(soilTempAddress, addr, 8);
      hasSoilTempAddress = true;
      continue;
    }
#endif

    if (!hasBoxTempAddress) {
      memcpy(boxTempAddress, addr, 8);
      hasBoxTempAddress = true;
    }
  }
}

float readTemperatureOrDefault(DeviceAddress address, bool available, float fallback) {
  if (!available) return fallback;

  float value = tempSensors.getTempC(address);
  if (value == DEVICE_DISCONNECTED_C || value < -40.0 || value > 85.0) {
    return fallback;
  }

  return value;
}

void setup() {
  // Power external peripherals on Heltec-style boards.
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);

  setSensorExcitationOff();
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  Serial.begin(115200);
  Serial.println();
  Serial.println("===============================================");
  Serial.println("          OpenTensio LoRa TX v0.1.0            ");
  Serial.println("        GPIO short-pulse sensor excitation     ");
  Serial.println("===============================================");

  I2C_ADS.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000);

  bool adsOK = ads.begin(0x48, &I2C_ADS);
  if (!adsOK) {
    Serial.println("[!] ADS1115 not found at 0x48");
  }
  ads.setGain(GAIN_ONE);
#ifdef RATE_ADS1115_860SPS
  ads.setDataRate(RATE_ADS1115_860SPS);
#endif

  float v1 = -1.0;
  float v2 = -1.0;
  if (adsOK) {
    v1 = readPulsedSensorVoltage(SENSOR_1_EXC_PIN, ADS_CHANNEL_SENSOR_1);
    v2 = readPulsedSensorVoltage(SENSOR_2_EXC_PIN, ADS_CHANNEL_SENSOR_2);
  }
  setSensorExcitationOff();

  display.init();
  display.flipScreenVertically();

  bool bmeOK = bme.begin(0x76, &I2C_ADS);
  if (!bmeOK) bmeOK = bme.begin(0x77, &I2C_ADS);

  discoverTemperatureSensors();
  tempSensors.requestTemperatures();

  float soilTemperatureC = readTemperatureOrDefault(soilTempAddress, hasSoilTempAddress, 24.0);
  float boxTemperatureC = readTemperatureOrDefault(boxTempAddress, hasBoxTempAddress, -99.0);

  float batteryVoltage = readBatteryVoltage();

  float airTemperatureC = -99.0;
  float airHumidityPct = -99.0;
  float airPressureHPa = -99.0;

  if (bmeOK) {
    airTemperatureC = bme.readTemperature();
    airHumidityPct = bme.readHumidity();
    airPressureHPa = bme.readPressure() / 100.0F;
  }

  float r1Ohms = voltageToResistanceOhms(v1);
  float r2Ohms = voltageToResistanceOhms(v2);

  float cb1 = calculateCentibars(r1Ohms / 1000.0, soilTemperatureC);
  float cb2 = calculateCentibars(r2Ohms / 1000.0, soilTemperatureC);

  Serial.printf("NODE        : %s\n", OPENTENSIO_NODE_ID);
  Serial.printf("BATTERY     : %.2f V\n", batteryVoltage);
  Serial.printf("SOIL TEMP   : %.2f C\n", soilTemperatureC);
  Serial.printf("BOX TEMP    : %.2f C\n", boxTemperatureC);
  Serial.println("-----------------------------------------------");
  Serial.printf("SENSOR 1 | %.3f V | %.0f Ohms | %.1f cb\n", v1, r1Ohms, cb1);
  Serial.printf("SENSOR 2 | %.3f V | %.0f Ohms | %.1f cb\n", v2, r2Ohms, cb2);
  Serial.println("===============================================");

  display.clear();
  display.setFont(ArialMT_Plain_10);

  if (bmeOK) {
    display.drawString(0, 0, String(airTemperatureC, 1) + "C | " + String(airHumidityPct, 0) + "% | " + String(airPressureHPa, 0) + "hPa");
  } else {
    display.drawString(0, 0, "OpenTensio LoRa TX");
  }

  display.drawString(0, 13, "BAT: " + String(batteryVoltage, 2) + "V | SOIL: " + String(soilTemperatureC, 1) + "C");
  display.drawString(0, 26, "S1: " + String(cb1, 1) + "cb | S2: " + String(cb2, 1) + "cb");
  display.drawString(0, 38, "V1: " + String(v1, 2) + "V | V2: " + String(v2, 2) + "V");
  display.drawString(0, 53, "MC payload ready");
  display.display();

  Mcu.begin();
  Radio.Init(NULL);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(
    MODEM_LORA,
    LORA_TX_POWER,
    0,
    LORA_BANDWIDTH,
    LORA_SPREADING_FACTOR,
    LORA_CODING_RATE,
    LORA_PREAMBLE_LENGTH,
    false,
    true,
    0,
    0,
    false,
    3000
  );

  // v0.1.0 uses the legacy MC payload by default for drop-in compatibility
  // with the original field deployment and existing Home Assistant MQTT sensors.
  // Future protocol idea, not enabled by default:
  // OT1;node_id;cb1;cb2;t_box;t_soil;v1;v2;r1;r2;vbat;t_air;h_air;p_air
  char payload[180];
  snprintf(
    payload,
    sizeof(payload),
    "MC;%.1f;%.1f;%.1f;%.1f;%.3f;%.3f;%.0f;%.0f;%.2f;%.1f;%.0f;%.0f",
    cb1,
    cb2,
    boxTemperatureC,
    soilTemperatureC,
    v1,
    v2,
    r1Ohms,
    r2Ohms,
    batteryVoltage,
    airTemperatureC,
    airHumidityPct,
    airPressureHPa
  );

  Serial.printf("LoRa payload: %s\n", payload);
  Radio.Send((uint8_t *)payload, strlen(payload));

  delay(LORA_TX_WAIT_MS);
  Radio.Sleep();
  display.sleep();

  // Double safety: soil sensor excitation pins are off before deep sleep.
  setSensorExcitationOff();

  // Power external peripherals off before deep sleep.
  digitalWrite(Vext, HIGH);
  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_SECONDS * 1000000ULL);
  esp_deep_sleep_start();
}

void loop() {}
